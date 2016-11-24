#ifndef MPX2BITS_H_
#define MPX2BITS_H_

#include <deque>
#include <complex>
#include <utility>
#include <vector>

#include "config.h"
#include "src/liquid_wrappers.h"

#ifdef HAVE_LIQUID

namespace redsea {

class BiphaseDecoder {
 public:
  BiphaseDecoder();
  ~BiphaseDecoder();
  std::pair<bool,std::complex<float>> push(std::complex<float> psk_symbol);

 private:
  std::complex<float> prev_psk_symbol_;
  std::vector<float> clock_history_;
  unsigned clock_;
  unsigned clock_polarity_;
};

class DeltaDecoder {
 public:
  DeltaDecoder();
  ~DeltaDecoder();
  unsigned decode(unsigned);

 private:
  unsigned prev_;
};

class Subcarrier {
 public:
  Subcarrier(bool feed_thru = false);
  ~Subcarrier();
  int getNextBit();
  bool isEOF() const;
#ifdef DEBUG
  float getT() const;
#endif

 private:
  void demodulateMoreBits();
  int  numsamples_;
  bool feed_thru_;

  std::deque<int> bit_buffer_;

  liquid::FIRFilter fir_lpf_;
  liquid::AGC agc_;
  liquid::NCO nco_approx_;
  liquid::NCO nco_exact_;
  liquid::SymSync symsync_;
  liquid::Modem modem_;

  bool is_eof_;

  DeltaDecoder delta_decoder_;
  BiphaseDecoder biphase_decoder_;

  std::complex<float> prev_sym_;
};

}  // namespace redsea

#endif // HAVE_LIQUID
#endif // MPX2BITS_H_
