#ifndef MPX2BITS_H_
#define MPX2BITS_H_

#define DBG_OUT

#include <complex>
#include <vector>

#include "liquid/liquid.h"
#include "wdsp/wdsp.h"
#include "wdsp/cirbuffer.h"

namespace redsea {

class LiquidPLL {
  public:
  LiquidPLL();
  ~LiquidPLL();
  float inOut(float err);

  private:
  float phase_offset_;
  float frequency_offset_;
  float phase_error_;
  float phi_hat_;
  float wn_;
  float zeta_;
  float K_;

  iirfilt_rrrf loopfilter_;

  float b_[3];
  float a_[3];

};

class LoopFilter {
  public:
  LoopFilter();
  std::complex<float> output() const;
  float phiHat() const;
  void input(float dphi);

  private:
  float m_bw;
  float m_zeta;
  float m_K;
  float m_t1;
  float m_t2;
  float m_b0;
  float m_b1;
  float m_b2;
  float m_a1;
  float m_a2;
  float m_v0;
  float m_v1;
  float m_v2;

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
  int    numsamples_;
  float subcarr_freq_;
  float gain_;

  float prev_acc_;
  unsigned counter_;
  std::vector<int>    tot_errs_;
  unsigned reading_frame_;

  int dbit_;

  wdsp::CirBuffer<int> bit_buffer_;

  std::vector<float> antialias_fir_;
  std::vector<float> data_shaping_fir_;
  wdsp::CirBuffer<std::complex<float>> subcarr_baseband_;
  wdsp::CirBuffer<std::complex<float>> subcarr_shaped_;

  bool is_eof_;
  wdsp::Delay<std::complex<float>> phase_diff_delay_;

  //LoopFilter loop_filter_;

  float m_inte;

  modem liq_modem_;
  //LiquidPLL pll_;
  agc_crcf agc_;
  nco_crcf nco_if_;

  float ph0_;


};

} // namespace redsea
#endif // MPX2BITS_H_
