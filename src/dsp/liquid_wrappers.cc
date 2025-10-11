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
#include "src/dsp/liquid_wrappers.hh"

#include <array>
#include <cassert>
#include <complex>
#include <cstddef>
#include <cstdint>
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

namespace {

float unwrap(float phase) {
  if (phase > kPi)
    return phase - k2Pi;
  if (phase < -kPi)
    return phase + k2Pi;
  return phase;
}

}  // namespace

void AGC::init(float bw, float initial_gain) {
  if (object_ != nullptr)
    agc_crcf_destroy(object_);
  object_ = agc_crcf_create();
  agc_crcf_set_bandwidth(object_, bw);
  agc_crcf_set_gain(object_, initial_gain);
}

AGC::~AGC() {
  if (object_ != nullptr)
    agc_crcf_destroy(object_);
}

std::complex<float> AGC::execute(std::complex<float> s) {
  std::complex<float> result;
  agc_crcf_execute(object_, s, &result);
  return result;
}

void FIRFilter::init(std::uint32_t len, float fc, float As, float mu) {
  if (object_ != nullptr)
    firfilt_crcf_destroy(object_);
  object_ = firfilt_crcf_create_kaiser(len, fc, As, mu);
  firfilt_crcf_set_scale(object_, 2.0f * fc);
}

FIRFilter::~FIRFilter() {
  if (object_ != nullptr)
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

std::size_t FIRFilter::length() const {
  return firfilt_crcf_get_length(object_);
}

float FIRFilter::getGroupDelay() {
  return firfilt_crcf_groupdelay(object_, 0.f);
}

NCO::NCO(liquid_ncotype type, float freq)
    : object_(nco_crcf_create(type)), initial_frequency_(freq) {
  nco_crcf_set_frequency(object_, freq);
}

NCO::~NCO() {
  if (object_ != nullptr)
    nco_crcf_destroy(object_);
}

void NCO::init(liquid_ncotype type, float freq) {
  if (object_ != nullptr)
    nco_crcf_destroy(object_);
  object_            = nco_crcf_create(type);
  initial_frequency_ = freq;
  nco_crcf_set_frequency(object_, freq);
}

void NCO::reset() {
  nco_crcf_reset(object_);
  nco_crcf_set_frequency(object_, initial_frequency_);
}

std::complex<float> NCO::mixDown(std::complex<float> s, int n_stream) {
  return s * std::polar(1.f, -phases_[n_stream]);
}

void NCO::step() {
  nco_crcf_step(object_);

  // Calculate (unwrapped) phase difference
  const float phase_now = nco_crcf_get_phase(object_);
  const float delta     = unwrap(phase_now - prev_f0_phase_);

  prev_f0_phase_ = phase_now;

  constexpr std::array<float, 4> subcarrier_frequencies{57000.f, 66500.f, 71250.f, 76000.f};

  for (size_t i = 0; i < 4; i++) {
    phases_[i] = unwrap(phases_[i] + delta * subcarrier_frequencies[i] / 57000.f);
  }
}

void NCO::setPLLBandwidth(float bw) {
  nco_crcf_pll_set_bandwidth(object_, bw);
}

void NCO::stepPLL(float dphi) {
  nco_crcf_pll_step(object_, dphi);
}

void SymSync::init(liquid_firfilt_type ftype, std::uint32_t k, std::uint32_t m, float beta,
                   std::uint32_t num_filters) {
  if (object_ != nullptr)
    symsync_crcf_destroy(object_);
  object_ = symsync_crcf_create_rnyquist(ftype, k, m, beta, num_filters);
  out_.resize(8);
}

SymSync::~SymSync() {
  if (object_ != nullptr)
    symsync_crcf_destroy(object_);
}

void SymSync::reset() {
  symsync_crcf_reset(object_);
}

void SymSync::setBandwidth(float bw) {
  symsync_crcf_set_lf_bw(object_, bw);
}

void SymSync::setOutputRate(std::uint32_t r) {
  symsync_crcf_set_output_rate(object_, r);
}

redsea::Maybe<std::complex<float>> SymSync::execute(std::complex<float>& in) {
  // To be set by liquid-dsp, no need to initialize
  // NOLINTNEXTLINE(cppcoreguidelines-init-variables)
  unsigned n_out;
  symsync_crcf_execute(object_, &in, 1, out_.data(), &n_out);
  return redsea::Maybe<std::complex<float>>{out_[0], n_out == 1};
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

std::uint32_t Modem::demodulate(std::complex<float> sample) {
  // To be set by liquid-dsp, no need to initialize
  // NOLINTNEXTLINE(cppcoreguidelines-init-variables)
  unsigned symbol_out;

#ifdef MODEM_IS_MODEMCF
  modemcf_demodulate(object_, sample, &symbol_out);
#else
  modem_demodulate(object_, sample, &symbol_out);
#endif

  return static_cast<std::uint32_t>(symbol_out);
}

float Modem::getPhaseError() {
#ifdef MODEM_IS_MODEMCF
  return modemcf_get_demodulator_phase_error(object_);
#else
  return modem_get_demodulator_phase_error(object_);
#endif
}

Resampler::Resampler(std::uint32_t half_length)
    : object_(resamp_rrrf_create(1.f, half_length, 0.47f, 60.0f, 32)) {
  if (object_ == nullptr) {
    throw std::runtime_error("error: Can't initialize resampler");
  }
}

void Resampler::setRatio(float ratio) {
  // Liquid can't take < 0.004, and we only reserved kOutputArraySize in the output buffer.
  if (ratio < 0.005f || ratio > static_cast<float>(kOutputArraySize)) {
    throw std::runtime_error("error: Can't support this sample rate");
  }
  resamp_rrrf_set_rate(object_, ratio);
}

Resampler::~Resampler() {
  if (object_ != nullptr)

    resamp_rrrf_destroy(object_);
}

std::uint32_t Resampler::execute(float in, std::array<float, kOutputArraySize>& out) {
  // To be set by liquid-dsp, no need to initialize
  // NOLINTNEXTLINE(cppcoreguidelines-init-variables)
  unsigned num_written;
  resamp_rrrf_execute(object_, in, out.data(), &num_written);
  assert(num_written <= kOutputArraySize);

  return static_cast<std::uint32_t>(num_written);
}

}  // namespace liquid
