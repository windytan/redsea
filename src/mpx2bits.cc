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

namespace {

  int sign(float x) {
    return (x >= 0);
  }
}

float sinc(float x) {
  return (x == 0.0 ? 1.0f : sin(x) / x);
}

float Blackman (int i, int M) {
  return 0.42 - 0.5 * cos(2 * M_PI * i/M) +
    0.08 * cos(4 * M_PI * i/M);
}

std::vector<float> FIR(float f_cutoff, int len) {

  assert(f_cutoff >= 0 && f_cutoff <= 0.5);
  assert(len > 0);

  int M = len-1;
  std::vector<float> result(len);
  float sum = 0;

  for (int i=0; i<len; i++) {
    result[i] = sinc(2*M_PI*f_cutoff*(i-M/2.0)) * Blackman(i, M);
    sum += result[i];
  }
  for (int i=0; i<len; i++) {
    result[i] /= sum;
  }

  return result;

}

DPSK::DPSK() : subcarr_freq_(FC_0), gain_(1.0f),
  counter_(0), tot_errs_(2), reading_frame_(0), bit_buffer_(),
  fir_lpf_(512, 1500.0f / FS),
  fir_phase_(64, 1200.0f / FS * 12),
  is_eof_(false),
  agc_(agc_crcf_create()),
  nco_if_(nco_crcf_create(LIQUID_VCO)),
  ph0_(0.0f), phase_delay_(wdelayf_create(17)), prevsign_(0),
  clock_shift_(0), clock_phase_(0), last_rising_at_(0), lastbit_(0)
  {

  nco_crcf_set_frequency(nco_if_, FC_0 * 2 * PI_f / FS);
  agc_crcf_set_bandwidth(agc_,1e-3f);
}

DPSK::~DPSK() {
  agc_crcf_destroy(agc_);
  nco_crcf_destroy(nco_if_);
}

void DPSK::demodulateMoreBits() {

  int16_t sample[IBUFLEN];
  int bytesread = fread(sample, sizeof(int16_t), IBUFLEN, stdin);
  if (bytesread < IBUFLEN) {
    is_eof_ = true;
    return;
  }

  for (int i = 0; i < bytesread; i++) {

    std::complex<float> sample_down, sample_shaped_unnorm, sample_shaped;
    nco_crcf_mix_down(nco_if_, sample[i], &sample_down);

    fir_lpf_.push(sample_down);
    sample_shaped_unnorm = fir_lpf_.execute();

    agc_crcf_execute(agc_, sample_shaped_unnorm, &sample_shaped);

    if (numsamples_ % 12 == 0) {

      float ph0;
      float ph1 = arg(sample_shaped);
      wdelayf_push(phase_delay_, ph1);
      wdelayf_read(phase_delay_, &ph0);
      float dph = ph1 - ph0;
      if (dph > M_PI)
        dph -= 2*M_PI;
      if (dph < -M_PI)
        dph += 2*M_PI;
      dph = fabs(dph) - M_PI_2;
      std::complex<float> dphc(dph,0),dphc_lpf,sq;

      sq = sample_shaped * sample_shaped;
      //printf("pe:%f,%f\n",1000*real(sq),1000*imag(sq));

      fir_phase_.push(dphc);
      dphc_lpf = fir_phase_.execute();

      int bval = sign(real(dphc_lpf));

      if (clock_phase_ % 16 == 0) {
        unsigned bit = bval;
        bit_buffer_.push_back(bit);
      }

      /*if (bval != prevsign_) {
        printf("rising at %d\n",clock_phase_ % 16);
        if (clock_phase_ > 7)
          clock_phase_ --;
        else
          clock_phase_ ++;
      }*/
      prevsign_ = bval;

      clock_phase_ ++;

    }

    nco_crcf_step(nco_if_);

    numsamples_ ++;

  }

}

int DPSK::getNextBit() {
  while (bit_buffer_.size() < 1 && !isEOF())
    demodulateMoreBits();

  int bit = 0;

  if (bit_buffer_.size() > 0) {
    bit = bit_buffer_.front();
    bit_buffer_.pop_front();
  }

  return bit;
}

bool DPSK::isEOF() const {
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
