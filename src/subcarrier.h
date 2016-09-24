#ifndef MPX2BITS_H_
#define MPX2BITS_H_

#ifdef HAVE_LIQUID

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
#ifdef DEBUG
    float getT() const;
#endif

  private:
    void demodulateMoreBits();
    int  numsamples_;

    std::deque<int> bit_buffer_;

    liquid::FIRFilter fir_lpf_;
    liquid::AGC agc_;
    liquid::NCO nco_approx_;
    liquid::NCO nco_exact_;
    liquid::SymSync symsync_;
    liquid::Modem modem_;

    bool is_eof_;

    unsigned symbol_clock_;
    unsigned prev_biphase_;

    DeltaDecoder delta_decoder_;

    unsigned num_symbol_errors_;

};

} // namespace redsea

#endif // HAVE_LIQUID
#endif // MPX2BITS_H_
