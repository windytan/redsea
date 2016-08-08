#include "liquid_wrappers.h"

#include <cassert>
#include <complex>

#include "liquid/liquid.h"

namespace liquid {

AGC::AGC(float bw) {
  object_ = agc_crcf_create();
  agc_crcf_set_bandwidth(object_, bw);
}

AGC::~AGC() {
  agc_crcf_destroy(object_);
}

std::complex<float> AGC::execute(std::complex<float> s) {
  std::complex<float> result;
  agc_crcf_execute(object_, s, &result);
  return result;
}

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

NCO::NCO(float freq) {
  object_ = nco_crcf_create(LIQUID_VCO);
  nco_crcf_set_frequency(object_, freq);

}

NCO::~NCO() {
  nco_crcf_destroy(object_);
}

std::complex<float> NCO::mixDown(std::complex<float> s) {
  std::complex<float> result;
  nco_crcf_mix_down(object_, s, &result);
  return result;
}

std::complex<float> NCO::mixUp(std::complex<float> s) {
  std::complex<float> result;
  nco_crcf_mix_up(object_, s, &result);
  return result;
}

void NCO::mixBlockDown(std::complex<float>* x, std::complex<float>* y,
    int n) {
  nco_crcf_mix_block_down(object_, x, y, n);
}

void NCO::step() {
  nco_crcf_step(object_);
}

SymSync::SymSync(liquid_firfilt_type ftype, unsigned k, unsigned m,
    float beta, unsigned num_filters) :
  object_(symsync_crcf_create_rnyquist(ftype, k, m, beta, num_filters)) {

}

SymSync::~SymSync() {
  symsync_crcf_destroy(object_);
}

void SymSync::setBandwidth(float bw) {
  symsync_crcf_set_lf_bw(object_, bw);
}

} // namespace liquid
