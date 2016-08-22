#include "mpx2bits.h"

#include <complex>
#include <deque>

#include "liquid_wrappers.h"

#define FS        228000.0f
#define FC_0      57000.0f
#define IBUFLEN   4096
#define OBUFLEN   128
#define BITBUFLEN 1024

#define PI_f      3.1415926535898f
#define PI_2_f    1.5707963267949f

namespace redsea {

DeltaDecoder::DeltaDecoder() : prev_(0) {

}

DeltaDecoder::~DeltaDecoder() {

}

unsigned DeltaDecoder::decode(unsigned d) {
  unsigned bit = (d != prev_);
  prev_ = d;
  return bit;
}

Subcarrier::Subcarrier() : subcarr_freq_(FC_0),
  bit_buffer_(),
  fir_lpf_(511, 2100.0f / FS),
  is_eof_(false),
  agc_(0.001f),
  nco_if_(FC_0 * 2 * PI_f / FS),
  nco_carrier_(0.0f),//FC_0 * 2 * PI_f / FS),
  symsync_(LIQUID_FIRFILT_RRC, 2, 5, 0.5f, 32),
  modem_(LIQUID_MODEM_DPSK2),
  prev_sym_(0), sym_clk_(0),
  biphase_(0), prev_biphase_(0)
  {

    symsync_.setBandwidth(0.02f);
    symsync_.setOutputRate(1);
    nco_carrier_.setPLLBandwidth(0.0004f);

}

Subcarrier::~Subcarrier() {

}

void Subcarrier::demodulateMoreBits() {

  int16_t sample[IBUFLEN];
  int samplesread = fread(sample, sizeof(int16_t), IBUFLEN, stdin);
  if (samplesread < IBUFLEN) {
    is_eof_ = true;
    return;
  }

  for (int i = 0; i < samplesread; i++) {

    std::complex<float> sample_down = nco_if_.mixDown(sample[i]);

    fir_lpf_.push(sample_down);
    std::complex<float> sample_shaped_unnorm = fir_lpf_.execute();

    std::complex<float> sample_shaped = agc_.execute(sample_shaped_unnorm);

    //std::complex<float> sample_bp = nco_if_.mixUp(sample_shaped);
    //std::complex<float> sample_pll = nco_carrier_.mixDown(sample_bp);

    //printf("pe:%.10f,%.10f\n",real(sample_bp), imag(sample_bp));

      //printf("pe:%.10f,%.10f\n",real(sample_pll),imag(sample_pll));
    if (numsamples_ % 48 == 0) {
      std::vector<std::complex<float>> y;
      y = symsync_.execute(sample_shaped);
      for (auto sy : y) {
        unsigned u = modem_.demodulate(sy);
        nco_carrier_.stepPLL(modem_.getPhaseError());
        biphase_ ^= u;

        if (sym_clk_ == 1) {

          if (prev_biphase_ == 0 && biphase_ == 1) {
            bit_buffer_.push_back(delta_decoder_.decode(1));
          }
          if (prev_biphase_ == 1 && biphase_ == 0) {
            bit_buffer_.push_back(delta_decoder_.decode(0));
          }
        }

        if (prev_biphase_ == biphase_)
          sym_clk_ = 0;

        prev_biphase_ = biphase_;

        sym_clk_ = (sym_clk_ + 1) % 2;

      }
    }

    nco_if_.step();
    nco_carrier_.step();

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

AsciiBits::AsciiBits() : is_eof_(false) {

}

AsciiBits::~AsciiBits() {

}

int AsciiBits::getNextBit() {
  int result = 0;
  while (result != '0' && result != '1' && result != EOF)
    result = getchar();

  if (result == EOF) {
    is_eof_ = true;
    return 0;
  }

  return (result == '1');

}

bool AsciiBits::isEOF() const {
  return is_eof_;
}

} // namespace redsea
