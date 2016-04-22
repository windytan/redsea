#include "mpx2bits.h"

#include <complex>

#include "wdsp/wdsp.h"

#define FS        228000.0
#define FC_0      57000.0
#define IBUFLEN   4096
#define OBUFLEN   128
#define BITBUFLEN 1024

namespace redsea {

int sign(double a) {
  return (a >= 0 ? 1 : 0);
}

BitStream::BitStream() : subcarr_freq_(FC_0), counter_(0), tot_errs_(2), reading_frame_(0),
  bit_buffer_(BITBUFLEN), subcarr_lopass_fir_(wdsp::FIR(4000.0 / FS, 64)),
  data_shaping_fir_(wdsp::FIR(1500.0 / (FS/8), 64)),
  subcarr_baseband_(IBUFLEN), subcarr_shaped_(IBUFLEN/8), is_eof_(false) {

}

void BitStream::deltaBit(int b) {
  bit_buffer_.append(b ^ dbit_);
  dbit_ = b;
}

void BitStream::biphase(double acc) {

  if (sign(acc) != sign(prev_acc_)) {
    tot_errs_[counter_ % 2] ++;
  }

  if (counter_ % 2 == reading_frame_) {
    deltaBit(sign(acc + prev_acc_));
  }
  if (counter_ == 0) {
    if (tot_errs_[1 - reading_frame_] < tot_errs_[reading_frame_]) {
      reading_frame_ = 1 - reading_frame_;
    }
    tot_errs_[0] = 0;
    tot_errs_[1] = 0;
  }

  prev_acc_ = acc;
  counter_ = (counter_ + 1) % 800;
}


void BitStream::demodulateMoreBits() {

  int16_t sample[IBUFLEN];
  int bytesread = fread(sample, sizeof(int16_t), IBUFLEN, stdin);
  if (bytesread < IBUFLEN) {
    is_eof_ = true;
    return;
  }

  for (int i = 0; i < bytesread; i++) {

    /* Subcarrier downmix & phase recovery */

    mixer_phi_ += 2 * M_PI * subcarr_freq_ * (1.0/FS);
    subcarr_baseband_.appendOverlapFiltered(wdsp::mix(sample[i] / 32768.0,
        mixer_phi_), subcarr_lopass_fir_);

    double pll_beta = 16e-3;

    /* Decimate band-limited signal */
    if (numsamples_ % 8 == 0) {

      std::complex<double> baseband_sample = subcarr_baseband_.at(0);
      subcarr_baseband_.forward(8);

      subcarr_shaped_.appendOverlapFiltered(baseband_sample, data_shaping_fir_);
      std::complex<double> shaped_sample = subcarr_shaped_.getNext();

      double phase_error = arg(shaped_sample);
      if (phase_error >= M_PI_2) {
        phase_error -= M_PI;
      } else if (phase_error <= -M_PI_2) {
        phase_error += M_PI;
      }

      mixer_phi_    -= pll_beta * phase_error;
      subcarr_freq_ -= .5 * pll_beta * phase_error;

      /* 1187.5 Hz clock */

      double clock_phi = mixer_phi_ / 48.0 + clock_offset_;
      double lo_clock  = (fmod(clock_phi, 2*M_PI) < M_PI ? 1 : -1);

      /* Clock phase recovery */

      if (sign(prev_bb_) != sign(real(shaped_sample))) {
        double d_cphi = fmod(clock_phi, M_PI);
        if (d_cphi >= M_PI_2) d_cphi -= M_PI;
        clock_offset_ -= 0.005 * d_cphi;
      }

      /* biphase symbol integrate & dump */
      acc_ += real(shaped_sample) * lo_clock;

      if (sign(lo_clock) != sign(prevclock_)) {
        biphase(acc_);
        acc_ = 0;
      }

      //printf("dd %f,%f\n",real(baseband_sample), imag(baseband_sample));

      prevclock_ = lo_clock;
      prev_bb_ = real(shaped_sample);
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
