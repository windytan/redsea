#include "subcarrier.h"

#ifdef HAVE_LIQUID

#include <complex>
#include <deque>
#include <iostream>
#include <tuple>

#include "liquid_wrappers.h"

namespace redsea {

namespace {

const float kFs_Hz               = 171000.0f;
const float kFc_0_Hz             = 57000.0f;
const float kBitsPerSecond       = 1187.5f;
const int   kInputBufferSize     = 4096;
const int   kSamplesPerSymbol    = 3;
const float kAGCBandwidth_Hz     = 500.0f;
const float kAGCInitialGain      = 0.0077f;
const float kLowpassCutoff_Hz    = 2400.0f;
const float kSymsyncBandwidth_Hz = 2400.0f;
const int   kSymsyncDelay        = 2;
const float kSymsyncBeta         = 0.8f;
const float kPLLBandwidth_Hz     = 0.3f;
const float kPLLMultiplier       = 9.0f;

float hertz2step(float Hz) {
  return Hz * 2.0f * M_PI / kFs_Hz;
}

#ifdef DEBUG
float step2hertz(float step) {
  return step * kFs_Hz / (2.0f * M_PI);
}
#endif

}

BiphaseDecoder::BiphaseDecoder() : prev_psk_symbol_(0.0f),
  clock_history_(48), clock_(0), clock_polarity_(0) {

}

BiphaseDecoder::~BiphaseDecoder() {

}

// Return {is_clock, symbol}
//   is_clock: true if symbol valid
//   symbol:   binary symbol in constellation {-1,0} => 0, {1,0} => 1
std::pair<bool, std::complex<float>> BiphaseDecoder::push(
    std::complex<float> psk_symbol) {

  std::complex<float> biphase = (psk_symbol - prev_psk_symbol_) * 0.5f;
  bool is_clock = (clock_ % 2 == clock_polarity_);

  clock_history_[clock_] =
    biphase.real() < 0.f ? -biphase.real() : biphase.real(); // aka. fabs()

  // Periodically evaluate validity of the chosen biphase clock polarity
  if (++clock_ == clock_history_.size()) {

    float a=0;
    float b=0;

    for (int i=0; i<(int)clock_history_.size();i++) {
      if (i%2==0)
        a += clock_history_[i];
      else
        b += clock_history_[i];
      clock_history_[i] = 0.f;
    }

    if      (a>b) clock_polarity_ = 0;
    else if (b>a) clock_polarity_ = 1;

    clock_ = 0;

  }

  prev_psk_symbol_ = psk_symbol;

  return {is_clock, biphase};
}

DeltaDecoder::DeltaDecoder() : prev_(0) {

}

DeltaDecoder::~DeltaDecoder() {

}

unsigned DeltaDecoder::decode(unsigned d) {
  unsigned bit = (d != prev_);
  prev_ = d;
  return bit;
}

Subcarrier::Subcarrier() : numsamples_(0), bit_buffer_(),
  fir_lpf_(256, kLowpassCutoff_Hz / kFs_Hz),
  agc_(kAGCBandwidth_Hz / kFs_Hz, kAGCInitialGain),
  nco_approx_(hertz2step(kFc_0_Hz)),
  nco_exact_(hertz2step(kFc_0_Hz)),
  symsync_(LIQUID_FIRFILT_RRC, kSamplesPerSymbol, kSymsyncDelay,
           kSymsyncBeta, 32),
  modem_(LIQUID_MODEM_PSK2), is_eof_(false),
  delta_decoder_() {

    symsync_.setBandwidth(kSymsyncBandwidth_Hz / kFs_Hz);
    symsync_.setOutputRate(1);
    nco_exact_.setPLLBandwidth(kPLLBandwidth_Hz / kFs_Hz);

}

Subcarrier::~Subcarrier() {

}

/** MPX to bits
 */
void Subcarrier::demodulateMoreBits() {

  int16_t inbuffer[kInputBufferSize];
  int samplesread = fread(inbuffer, sizeof(inbuffer[0]), kInputBufferSize,
      stdin);
  if (samplesread < kInputBufferSize) {
    is_eof_ = true;
    return;
  }

  const int decimate = kFs_Hz / kBitsPerSecond / 2 / kSamplesPerSymbol;

  for (int16_t sample : inbuffer) {

    // Mix RDS to baseband for filtering purposes
    std::complex<float> sample_baseband = nco_approx_.mixDown(sample);

    fir_lpf_.push(sample_baseband);

    if (numsamples_ % decimate == 0) {

      std::complex<float> sample_lopass = agc_.execute(fir_lpf_.execute());

      // PLL-controlled 57 kHz mixdown - aliasing is intentional so we don't
      // have to mix it back up first
      sample_lopass = nco_exact_.mixDown(sample_lopass);

      std::vector<std::complex<float>> symbols =
        symsync_.execute(sample_lopass);

      for (std::complex<float> symbol : symbols) {
#ifdef DEBUG
        printf("sy:%f,%f,%f\n",
            numsamples_ / kFs_Hz,
            symbol.real(),
            symbol.imag());
#endif

        // Modem here is only used to track PLL phase error
        modem_.demodulate(symbol);
        nco_exact_.stepPLL(modem_.getPhaseError() * kPLLMultiplier);

        bool is_clock;
        std::complex<float> biphase;
        std::tie(is_clock,biphase) = biphase_decoder_.push(symbol);

        // One biphase symbol received for every 2 PSK symbols
        if (is_clock) {
          bit_buffer_.push_back(delta_decoder_.decode(
                biphase.real() >= 0));
#ifdef DEBUG
          printf("bi:%f,%f,%f\n",
              numsamples_ / kFs_Hz,
              biphase.real(),
              biphase.imag());
#endif
        }

      }
#ifdef DEBUG
      printf("f:%f,%f,%f,%f,%f,%f,%f\n",
          numsamples_ / kFs_Hz,
          (float)sample,
          step2hertz(nco_exact_.getFrequency()),
          modem_.getPhaseError(),
          agc_.getGain(),
          sample_lopass.real(),
          sample_lopass.imag());
#endif
    }

    nco_exact_.step();
    nco_approx_.step();

    numsamples_ ++;

  }
}

int Subcarrier::getNextBit() {
  while (bit_buffer_.size() < 1 && !isEOF())
    demodulateMoreBits();

  int bit = 0;

  if (bit_buffer_.size() > 0) {
    bit = bit_buffer_.front();
    bit_buffer_.pop_front();
  }

  return bit;
}

bool Subcarrier::isEOF() const {
  return is_eof_;
}

#ifdef DEBUG
float Subcarrier::getT() const {
  return numsamples_ / kFs_Hz;
}
#endif

} // namespace redsea

#endif // HAVE_LIQUID
