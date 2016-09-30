#include "subcarrier.h"

#ifdef HAVE_LIQUID

#include <complex>
#include <deque>
#include <iostream>

#include "liquid_wrappers.h"

namespace redsea {

namespace {

const float kFs_Hz               = 171000.0f;
const float kFc_0_Hz             = 57000.0f;
const float kBitsPerSecond       = 1187.5f;
const int   kInputBufferSize     = 4096;
const int   kSamplesPerSymbol    = 3;
const float kAGCBandwidth_Hz     = 900.0f;
const float kAGCInitialGain      = 0.0077f;
const float kLowpassCutoff_Hz    = 2600.0f;
const float kSymsyncBandwidth_Hz = 2400.0f;
const int   kSymsyncDelay        = 2;
const float kSymsyncBeta         = 0.8f;
const float kPLLBandwidth_Hz     = 0.8f;

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
  nco_approx_(kFc_0_Hz * 2 * M_PI / kFs_Hz),
  nco_exact_(0.0f),
  symsync_(LIQUID_FIRFILT_RRC, kSamplesPerSymbol, kSymsyncDelay,
           kSymsyncBeta, 32),
  modem_(LIQUID_MODEM_PSK2), is_eof_(false), symbol_clock_(0), prev_biphase_(0),
  delta_decoder_(), num_symbol_errors_(0) {

    symsync_.setBandwidth(kSymsyncBandwidth_Hz / kFs_Hz);
    symsync_.setOutputRate(1);
    nco_exact_.setPLLBandwidth(kPLLBandwidth_Hz / kFs_Hz);

}

Subcarrier::~Subcarrier() {

}

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

    std::complex<float> sample_baseband = nco_approx_.mixDown(sample);

    fir_lpf_.push(sample_baseband);

    if (numsamples_ % decimate == 0) {

      std::complex<float> sample_lopass = agc_.execute(fir_lpf_.execute());

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

        unsigned biphase = modem_.demodulate(symbol);
        nco_exact_.stepPLL(modem_.getPhaseError() * 6.f);// factor is a quickfix

        if (symbol_clock_ == 1) {
          bit_buffer_.push_back(delta_decoder_.decode(biphase));

          if (biphase ^ prev_biphase_) {
            num_symbol_errors_ = 0;
          } else {
            num_symbol_errors_ ++;
            if (num_symbol_errors_ >= 7) {
              symbol_clock_ ^= 1;
              num_symbol_errors_ = 0;
            }
          }
        }

        prev_biphase_ = biphase;

        symbol_clock_ ^= 1;

      }
#ifdef DEBUG
      printf("f:%f,%f,%f,%f,%f,%f,%f\n",
          numsamples_ / kFs_Hz,
          (float)sample,
          nco_exact_.getFrequency() * kFs_Hz / (2 * M_PI),
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
