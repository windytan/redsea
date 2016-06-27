#ifndef LIQUID_WRAPPERS_H_
#define LIQUID_WRAPPERS_H_

#include <complex>

#include "liquid/liquid.h"

namespace liquid {

class FIRFilter {

  public:
  FIRFilter(int len, float fc, float As=60.0f, float mu=0.0f);
  ~FIRFilter();
  void push(std::complex<float> s);
  std::complex<float> execute();

  private:
  firfilt_crcf object_;

};

}

#endif // LIQUID_WRAPPERS_H_
