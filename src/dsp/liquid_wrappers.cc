/*
 * Copyright (c) Oona Räisänen
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 */
#include "src/dsp/liquid_wrappers.h"

#include <complex>
#include <stdexcept>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
// https://github.com/jgaeddert/liquid-dsp/issues/229
#pragma clang diagnostic ignored "-Wreturn-type-c-linkage"
extern "C" {
#include "liquid/liquid.h"
}
#pragma clang diagnostic pop

namespace liquid {

AGC::AGC(float bw, float initial_gain) : object_(agc_crcf_create()) {
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

FIRFilter::FIRFilter(unsigned int len, float fc, float As, float mu)
    : object_(firfilt_crcf_create_kaiser(len, fc, As, mu)) {
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

size_t FIRFilter::length() const {
  return firfilt_crcf_get_length(object_);
}

NCO::NCO(liquid_ncotype type, float freq)
    : object_(nco_crcf_create(type)), initial_frequency_(freq) {
  nco_crcf_set_frequency(object_, freq);
}

NCO::~NCO() {
  nco_crcf_destroy(object_);
}

void NCO::reset() {
  nco_crcf_reset(object_);
  nco_crcf_set_frequency(object_, initial_frequency_);
}

std::complex<float> NCO::mixDown(std::complex<float> s) {
  std::complex<float> result;
  nco_crcf_mix_down(object_, s, &result);
  return result;
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

SymSync::SymSync(liquid_firfilt_type ftype, unsigned k, unsigned m, float beta,
                 unsigned num_filters)
    : object_(symsync_crcf_create_rnyquist(ftype, k, m, beta, num_filters)), out_(8) {}

SymSync::~SymSync() {
  symsync_crcf_destroy(object_);
}

void SymSync::reset() {
  symsync_crcf_reset(object_);
}

void SymSync::setBandwidth(float bw) {
  symsync_crcf_set_lf_bw(object_, bw);
}

void SymSync::setOutputRate(unsigned r) {
  symsync_crcf_set_output_rate(object_, r);
}

Maybe<std::complex<float>> SymSync::execute(std::complex<float>& in) {
  // To be set by liquid-dsp, no need to initialize
  // NOLINTNEXTLINE(cppcoreguidelines-init-variables)
  unsigned n_out;
  symsync_crcf_execute(object_, &in, 1, out_.data(), &n_out);
  return Maybe<std::complex<float>>{out_[0], n_out == 1};
}

Modem::Modem(modulation_scheme scheme)
    : object_(
#ifdef MODEM_IS_MODEMCF
          modemcf_create(scheme)
#else
          modem_create(scheme)
#endif
      ) {
}

Modem::~Modem() {
#ifdef MODEM_IS_MODEMCF
  modemcf_destroy(object_);
#else
  modem_destroy(object_);
#endif
}

unsigned int Modem::demodulate(std::complex<float> sample) {
  // To be set by liquid-dsp, no need to initialize
  // NOLINTNEXTLINE(cppcoreguidelines-init-variables)
  unsigned symbol_out;

#ifdef MODEM_IS_MODEMCF
  modemcf_demodulate(object_, sample, &symbol_out);
#else
  modem_demodulate(object_, sample, &symbol_out);
#endif

  return symbol_out;
}

float Modem::getPhaseError() {
#ifdef MODEM_IS_MODEMCF
  return modemcf_get_demodulator_phase_error(object_);
#else
  return modem_get_demodulator_phase_error(object_);
#endif
}

Resampler::Resampler(float ratio, unsigned int length)
    : object_(resamp_rrrf_create(ratio, length, 0.47f, 60.0f, 32)) {
  if (ratio > 2.f) {
    throw std::runtime_error("error: Can't support this sample rate");
  }
}

Resampler::~Resampler() {
  resamp_rrrf_destroy(object_);
}

unsigned int Resampler::execute(float in, float* out) {
  // To be set by liquid-dsp, no need to initialize
  // NOLINTNEXTLINE(cppcoreguidelines-init-variables)
  unsigned int num_written;
  resamp_rrrf_execute(object_, in, out, &num_written);

  return num_written;
}

}  // namespace liquid
