#ifndef LIQUID_WRAPPERS_H_
#define LIQUID_WRAPPERS_H_

#include <complex>

#include "liquid/liquid.h"

namespace liquid {

class AGC {

  public:
  AGC(float bw);
  ~AGC();
  std::complex<float> execute(std::complex<float> s);

  private:
  agc_crcf object_;
};

class FIRFilter {

  public:
  FIRFilter(int len, float fc, float As=60.0f, float mu=0.0f);
  ~FIRFilter();
  void push(std::complex<float> s);
  std::complex<float> execute();

  private:
  firfilt_crcf object_;

};

class NCO {

  public:
  NCO(float freq);
  ~NCO();
  std::complex<float> mixDown(std::complex<float> s);
  void step();

  private:
  nco_crcf object_;

};

}

#endif // LIQUID_WRAPPERS_H_
