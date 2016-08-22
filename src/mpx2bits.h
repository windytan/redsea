#ifndef MPX2BITS_H_
#define MPX2BITS_H_

#define DBG_OUT

#include <cassert>
#include <deque>
#include <complex>
#include <vector>

#include "liquid_wrappers.h"

namespace redsea {

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
    int   numsamples_;
    float subcarr_freq_;

    std::deque<int> bit_buffer_;

    liquid::FIRFilter fir_lpf_;

    bool is_eof_;

    liquid::AGC agc_;
    liquid::NCO nco_approx_;
    liquid::NCO nco_exact_;

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
