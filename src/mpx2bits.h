#ifndef MPX2BITS_H_
#define MPX2BITS_H_

#define DBG_OUT

#include <cassert>
#include <deque>
#include <complex>
#include <vector>

#include "liquid/liquid.h"

namespace redsea {

std::vector<float> FIR(float f_cutoff, int len);
float sinc(float);

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

    std::vector<float> antialias_fir_;
    std::vector<float> phase_fir_;

    bool is_eof_;

    float m_inte;

    agc_crcf agc_;
    nco_crcf nco_if_;

    float ph0_;

    wdelayf phase_delay_;

    firfilt_crcf firfilt_;
    firfilt_crcf firfilt_phase_;

    int clock_shift_;
    int clock_phase_;
    int prevsign_;
    int last_rising_at_;
    int lastbit_;

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
