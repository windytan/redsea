#ifndef MPX2BITS_H_
#define MPX2BITS_H_

#define DBG_OUT

#include <cassert>
#include <complex>
#include <vector>

#include "liquid/liquid.h"

namespace redsea {

std::vector<float> FIR(float f_cutoff, int len);
float sinc(float);

class BitBuffer {
  public:
    BitBuffer(int len);
    int size() const;
    void append(uint8_t input_element);
    void forward(int n);
    int getTail() const;
    int getFillCount() const;
    uint8_t at(int n) const;
    uint8_t getNext();

  private:
    std::vector<uint8_t> m_data;
    int  m_head;
    int  m_tail;
    int  m_fill_count;
    const int m_len;

};

class BitStream {
  public:
  BitStream();
  ~BitStream();
  int getNextBit();
  bool isEOF() const;

  private:
  void demodulateMoreBits();
  void biphase(float acc);
  void deltaBit(int b);
  double mixer_phi_;
  float clock_offset_;
  float prevclock_;
  float prev_bb_;
  float acc_;
  int   numsamples_;
  float subcarr_freq_;
  float gain_;

  float prev_acc_;
  unsigned counter_;
  std::vector<int>    tot_errs_;
  unsigned reading_frame_;

  int dbit_;

  BitBuffer bit_buffer_;

  std::vector<float> antialias_fir_;
  std::vector<float> phase_fir_;

  bool is_eof_;

  float m_inte;

  modem liq_modem_;
  agc_crcf agc_;
  nco_crcf nco_if_;

  float ph0_;

  wdelayf phase_delay_;

  firfilt_crcf firfilt_;
  firfilt_crcf firfilt_phase_;

};

} // namespace redsea
#endif // MPX2BITS_H_
