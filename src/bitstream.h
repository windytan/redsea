#ifndef BITSTREAM_H_
#define BITSTREAM_H_

#define DBG_OUT

#include <vector>

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
  void bit(int b);
  double mixer_phi_;
  double clock_offset_;
  double prevclock_;
  double prev_bb_;
  double acc_;
  int    numsamples_;
  double fsc_;

  double prev_acc_;
  int    counter_;
  std::vector<int>    tot_errs_;
  int    reading_frame_;

  int dbit_;

  wdsp::CirBuffer<int> bit_buffer_;

  std::vector<double> subcarr_lopass_fir_;
  wdsp::CirBuffer<std::complex<double>> subcarr_baseband_;
  wdsp::CirBuffer<std::complex<double>> mixer_lagged_;

  bool is_eof_;

};

} // namespace redsea
#endif // BITSTREAM_H_
