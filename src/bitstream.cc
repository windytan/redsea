#include "bitstream.h"

#include <complex>

#include "wdsp/wdsp.h"

#include "filters.h"

#define FS        250000.0
#define FC_0      57000.0
#define IBUFLEN   4096
#define OBUFLEN   128
#define BITBUFLEN 1024

namespace redsea {

int sign(double a) {
  return (a >= 0 ? 1 : 0);
}

BitStream::BitStream() : tot_errs_(2), reading_frame_(0), counter_(0), subcarr_freq_(FC_0), bit_buffer_(BITBUFLEN), mixer_phi_(0), clock_offset_(0), is_eof_(false), subcarr_lopass_fir_(wdsp::FIR(4000.0 / FS, 127)), subcarr_baseband_(IBUFLEN) {

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

      std::complex<double> sc_sample = subcarr_baseband_.at(0);
      subcarr_baseband_.forward(8);

      double phi1 = arg(sc_sample);
      if (phi1 >= M_PI_2) {
        phi1 -= M_PI;
      } else if (phi1 <= -M_PI_2) {
        phi1 += M_PI;
      }

      mixer_phi_    -= pll_beta * phi1;
      subcarr_freq_ -= .5 * pll_beta * phi1;

      /* 1187.5 Hz clock */

      double clock_phi = mixer_phi_ / 48.0 + clock_offset_;
      double lo_clock  = (fmod(clock_phi, 2*M_PI) < M_PI ? 1 : -1);

      /* Clock phase recovery */

      if (sign(prev_bb_) != sign(real(sc_sample))) {
        double d_cphi = fmod(clock_phi, M_PI);
        if (d_cphi >= M_PI_2) d_cphi -= M_PI;
        clock_offset_ -= 0.005 * d_cphi;
      }

      /* biphase symbol integrate & dump */
      acc_ += real(sc_sample) * lo_clock;

      if (sign(lo_clock) != sign(prevclock_)) {
        biphase(acc_);
        acc_ = 0;
      }

      prevclock_ = lo_clock;
      prev_bb_ = real(sc_sample);
    }

    if (numsamples_ % 1000000 == 0)
      printf(":%.2f Hz\n", subcarr_freq_);

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
