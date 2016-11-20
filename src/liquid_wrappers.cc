#include "src/liquid_wrappers.h"

#include "config.h"
#ifdef HAVE_LIQUID

#include <cassert>
#include <complex>

#include "liquid/liquid.h"

namespace liquid {

AGC::AGC(float bw, float initial_gain) {
  object_ = agc_crcf_create();
  agc_crcf_set_bandwidth(object_, bw);
  agc_crcf_set_gain(object_, initial_gain);
}

AGC::~AGC() {
  agc_crcf_destroy(object_);
}

std::complex<float> AGC::execute(std::complex<float> s) {
  std::complex<float> result;
  agc_crcf_execute(object_, s, &result);
  return result;
}

float AGC::getGain() {
  return agc_crcf_get_gain(object_);
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

NCO::NCO(float freq) : object_(nco_crcf_create(LIQUID_VCO)) {
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

void NCO::setPLLBandwidth(float bw) {
  nco_crcf_pll_set_bandwidth(object_, bw);
}

void NCO::stepPLL(float dphi) {
  nco_crcf_pll_step(object_, dphi);
}

float NCO::getFrequency() {
  return nco_crcf_get_frequency(object_);
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

void SymSync::setOutputRate(unsigned r) {
  symsync_crcf_set_output_rate(object_, r);
}

std::vector<std::complex<float>> SymSync::execute(std::complex<float> s_in) {
  std::complex<float> s_out[8];
  unsigned n_out=0;
  symsync_crcf_execute(object_, &s_in, 1, &s_out[0], &n_out);

  std::vector<std::complex<float>> result(std::begin(s_out), std::end(s_out));
  result.resize(n_out);
  return result;

}


Modem::Modem(modulation_scheme scheme) : object_(modem_create(scheme)) {

}

Modem::~Modem() {
  modem_destroy(object_);
}

unsigned int Modem::demodulate(std::complex<float> sample) {
  unsigned symbol_out;

  modem_demodulate(object_, sample, &symbol_out);

  return symbol_out;
}

float Modem::getPhaseError() {
  return modem_get_demodulator_phase_error(object_);
}

} // namespace liquid

#endif
