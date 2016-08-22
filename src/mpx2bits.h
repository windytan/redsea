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
    Subcarrier();
    ~Subcarrier();
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

    std::deque<int> bit_buffer_;

    liquid::FIRFilter fir_lpf_;

    bool is_eof_;

    liquid::AGC agc_;
    liquid::NCO nco_if_;
    liquid::NCO nco_carrier_;

    liquid::SymSync symsync_;

    liquid::Modem modem_;

    unsigned prev_sym_;
    unsigned sym_clk_;
    unsigned biphase_;
    unsigned prev_biphase_;

    DeltaDecoder delta_decoder_;

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
