#ifndef BITSTREAM_H_
#define BITSTREAM_H_

#include <vector>

#include "wdsp/cirbuffer.h"
#include "wdsp/window.h"

#define FS      250000.0
#define FC_0    57000.0
#define IBUFLEN 4096
#define OBUFLEN 128
#define BITBUFLEN 1024

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
  double subcarr_phi_;
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

  bool is_eof_;

};

} // namespace redsea
#endif // BITSTREAM_H_
