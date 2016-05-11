#ifndef MPX2BITS_H_
#define MPX2BITS_H_

#define DBG_OUT

#include <vector>

#include "wdsp/wdsp.h"
#include "wdsp/cirbuffer.h"

namespace redsea {

class BitStream {
  public:
  BitStream();
  int getNextBit();
  bool isEOF() const;

  private:
  void demodulateMoreBits();
  void biphase(double acc);
  void deltaBit(int b);
  double mixer_phi_;
  double clock_offset_;
  double prevclock_;
  double prev_bb_;
  double acc_;
  int    numsamples_;
  double subcarr_freq_;

  double prev_acc_;
  int    counter_;
  std::vector<int>    tot_errs_;
  int    reading_frame_;

  int dbit_;

  wdsp::CirBuffer<int> bit_buffer_;

  std::vector<double> antialias_fir_;
  std::vector<double> data_shaping_fir_;
  wdsp::CirBuffer<std::complex<double>> subcarr_baseband_;
  wdsp::CirBuffer<std::complex<double>> subcarr_shaped_;

  bool is_eof_;
  wdsp::Delay<std::complex<double>> phase_diff_delay_;

};

} // namespace redsea
#endif // MPX2BITS_H_
