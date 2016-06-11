#include "mpx2bits.h"

#include <complex>

#include "liquid/liquid.h"
#include "wdsp/wdsp.h"

#define FS        228000.0f
#define FC_0      56990.0f
#define IBUFLEN   4096
#define OBUFLEN   128
#define BITBUFLEN 1024
#define DECIMATE  8

#define PI_f      3.1415926535898f
#define PI_2_f    1.5707963267949f

namespace redsea {

BitStream::BitStream() : subcarr_freq_(FC_0), counter_(0), tot_errs_(2), reading_frame_(0),
  bit_buffer_(BITBUFLEN), antialias_fir_(wdsp::FIR(4000.0f / FS, 64)), gain_(1.0f),
  data_shaping_fir_(wdsp::FIR(1500.0f / (FS/8.0f), 64)),
  subcarr_baseband_(IBUFLEN), subcarr_shaped_(IBUFLEN/8), is_eof_(false), phase_diff_delay_(24),
  m_inte(0.0f), liq_modem_(modem_create(LIQUID_MODEM_DPSK2)),
  agc_(agc_crcf_create()),
  nco_if_(nco_crcf_create(LIQUID_VCO)),
  ph0_(0.0f) {

  modem_print(liq_modem_);
  nco_crcf_set_frequency(nco_if_, FC_0 * 2 * PI_f / FS);
  agc_crcf_set_bandwidth(agc_,1e-3f);
}

BitStream::~BitStream() {
  modem_destroy(liq_modem_);
  agc_crcf_destroy(agc_);
  nco_crcf_destroy(nco_if_);
}

void BitStream::demodulateMoreBits() {

  int16_t sample[IBUFLEN];
  int bytesread = fread(sample, sizeof(int16_t), IBUFLEN, stdin);
  if (bytesread < IBUFLEN) {
    is_eof_ = true;
    return;
  }

  for (int i = 0; i < bytesread; i++) {

    std::complex<float> mixed;
    nco_crcf_mix_down(nco_if_, sample[i], &mixed);
    subcarr_baseband_.appendOverlapFiltered(mixed, antialias_fir_);

    nco_crcf_step(nco_if_);

    if (numsamples_ % DECIMATE == 0) {

      std::complex<float> baseband_sample = subcarr_baseband_.at(0);
      subcarr_baseband_.forward(DECIMATE);

      subcarr_shaped_.appendOverlapFiltered(baseband_sample, data_shaping_fir_);
      std::complex<float> shaped_sample_unnorm = subcarr_shaped_.getNext();
      std::complex<float> shaped_sample;
      agc_crcf_execute(agc_, shaped_sample_unnorm, &shaped_sample);

      if (numsamples_ % 192 == 0) {
        unsigned bit;
        modem_demodulate(liq_modem_, shaped_sample, &bit);
        bit_buffer_.append(bit);
      }

    }

    numsamples_ ++;

  }

}

int BitStream::getNextBit() {
  while (bit_buffer_.getFillCount() < 1 && !isEOF())
    demodulateMoreBits();

  return bit_buffer_.getNext();
}

bool BitStream::isEOF() const {
  return is_eof_;
}

} // namespace redsea
