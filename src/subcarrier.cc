#include "subcarrier.h"

#include <complex>
#include <deque>
#include <iostream>

#include "liquid_wrappers.h"

namespace redsea {

namespace {

const float kFs = 171000.0f;
const float kFc_0 = 57000.0f;
const float kBitsPerSecond = 1187.5;
const int kInputBufferSize = 4096;
const int kSamplesPerSymbol = 4;

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
  fir_lpf_(256, 2100.0f / kFs), is_eof_(false), agc_(0.001f),
  nco_approx_(kFc_0 * 2 * M_PI / kFs), nco_exact_(0.0f),
  symsync_(LIQUID_FIRFILT_RRC, kSamplesPerSymbol, 5, 0.5f, 32),
  modem_(LIQUID_MODEM_PSK2), symbol_clock_(0), prev_biphase_(0),
  delta_decoder_(), symbol_errors_(0) {

    symsync_.setBandwidth(0.02f);
    symsync_.setOutputRate(1);
    nco_exact_.setPLLBandwidth(0.0004f);

}

Subcarrier::~Subcarrier() {

}

void Subcarrier::demodulateMoreBits() {

  int16_t sample[kInputBufferSize];
  int samplesread = fread(sample, sizeof(sample[0]), kInputBufferSize, stdin);
  if (samplesread < kInputBufferSize) {
    is_eof_ = true;
    return;
  }

  const int decimate = kFs / kBitsPerSecond / 2 / kSamplesPerSymbol;

  for (int i = 0; i < samplesread; i++) {

    std::complex<float> sample_baseband = nco_approx_.mixDown(sample[i]);

    fir_lpf_.push(sample_baseband);

    if (numsamples_ % decimate == 0) {

      std::complex<float> sample_lopass = agc_.execute(fir_lpf_.execute());

      nco_exact_.stepPLL(modem_.getPhaseError());
      sample_lopass = nco_exact_.mixDown(sample_lopass);

      std::vector<std::complex<float>> symbols =
        symsync_.execute(sample_lopass);

      for (std::complex<float> symbol : symbols) {
        unsigned biphase = modem_.demodulate(symbol);

        if (symbol_clock_ == 1) {
          bit_buffer_.push_back(delta_decoder_.decode(biphase));

          if (biphase ^ prev_biphase_) {
            symbol_errors_ = 0;
          } else {
            symbol_errors_ ++;
            if (symbol_errors_ >= 7) {
              symbol_clock_ ^= 1;
              symbol_errors_ = 0;
            }
          }
        }

        prev_biphase_ = biphase;

        symbol_clock_ ^= 1;

      }
    }

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

} // namespace redsea
