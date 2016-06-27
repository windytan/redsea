#include "liquid_wrappers.h"

#include <cassert>
#include <complex>

#include "liquid/liquid.h"

namespace liquid {

FIRFilter::FIRFilter(int len, float fc, float As, float mu) {

  assert (fc >= 0.0f && fc <= 0.5f);
  assert (As > 0.0f);
  assert (mu >= -0.5f && mu <= 0.5f);

  object_ = firfilt_crcf_create_kaiser(len, fc, As, mu);
  firfilt_crcf_set_scale(object_, 2.0f * fc);

}

FIRFilter::~FIRFilter() {
  firfilt_crcf_destroy(object_);
}

void FIRFilter::push(std::complex<float> s) {
  firfilt_crcf_push(object_, s);
}

std::complex<float> FIRFilter::execute() {
  std::complex<float> result;
  firfilt_crcf_execute(object_, &result);
  return result;
}

} // namespace liquid
