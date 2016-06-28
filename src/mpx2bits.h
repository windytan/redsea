#ifndef MPX2BITS_H_
#define MPX2BITS_H_

#define DBG_OUT

#include <cassert>
#include <deque>
#include <complex>
#include <vector>

#include "liquid_wrappers.h"

namespace redsea {

class RunningSum {
  public:
    RunningSum(int len);
    ~RunningSum();
    float pushAndRead(float);
    int lastMaxIndex() const;
  private:
    std::vector<float> values_;
    int len_;
    float sum_;
    int i_;
    int max_i_;
    int last_max_i_;
    float max_sum_;
};

class DPSK {
  public:
    DPSK();
    ~DPSK();
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
    std::vector<int> tot_errs_;
    unsigned reading_frame_;

    int dbit_;

    std::deque<int> bit_buffer_;

    liquid::FIRFilter fir_lpf_;
    liquid::FIRFilter fir_phase_;

    bool is_eof_;

    liquid::AGC agc_;
    liquid::NCO nco_if_;

    float ph0_;

    wdelaycf sym_delay_;

    int clock_shift_;
    int clock_phase_;
    int last_rising_at_;
    int lastbit_;

    RunningSum running_sum_;

};

class AsciiBits {

  public:
    AsciiBits();
    ~AsciiBits();
    int getNextBit();
    bool isEOF() const;

  private:
    bool is_eof_;

};

} // namespace redsea
#endif // MPX2BITS_H_
