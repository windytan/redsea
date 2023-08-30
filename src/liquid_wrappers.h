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
#ifndef LIQUID_WRAPPERS_H_
#define LIQUID_WRAPPERS_H_

#include <complex>
#include <utility>
#include <vector>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
// https://github.com/jgaeddert/liquid-dsp/issues/229
#pragma clang diagnostic ignored "-Wreturn-type-c-linkage"
extern "C" {
#include "liquid/liquid.h"
}
#pragma clang diagnostic pop

template<typename T>
struct Maybe {
  T data;
  bool valid;
};

namespace liquid {

class AGC {
 public:
  AGC(float bw, float initial_gain);
  AGC(const AGC&) = delete;
  ~AGC();
  std::complex<float> execute(std::complex<float> s);

 private:
  agc_crcf object_;
};

class FIRFilter {
 public:
  FIRFilter(unsigned int len, float fc, float As = 60.0f, float mu = 0.0f);
  FIRFilter(const FIRFilter&) = delete;
  ~FIRFilter();
  void push(std::complex<float> s);
  std::complex<float> execute();
  size_t length() const;

 private:
  firfilt_crcf object_;
};

class NCO {
 public:
  NCO(liquid_ncotype type, float freq);
  NCO(const NCO&) = delete;
  ~NCO();
  std::complex<float> mixDown(std::complex<float> s);
  void step();
  void setPLLBandwidth(float);
  void stepPLL(float dphi);
  void reset();
  float getFrequency();

 private:
  nco_crcf object_;
  float initial_frequency_;
};

class SymSync {
 public:
  SymSync(liquid_firfilt_type ftype, unsigned k, unsigned m,
          float beta, unsigned num_filters);
  SymSync(const SymSync&) = delete;
  ~SymSync();
  void reset();
  void setBandwidth(float);
  void setOutputRate(unsigned);
  Maybe<std::complex<float>> execute(std::complex<float>& in);

 private:
  symsync_crcf object_;
  std::vector<std::complex<float>> out_;
};

class Modem {
 public:
  explicit Modem(modulation_scheme scheme);
  Modem(const Modem&) = delete;
  ~Modem();
  unsigned int demodulate(std::complex<float> sample);
  float getPhaseError();

 private:
#ifdef MODEM_IS_MODEMCF
  modemcf object_;
#else
  modem object_;
#endif
};

class Resampler {
 public:
  explicit Resampler(float ratio, unsigned int length);
  Resampler(const Resampler&) = delete;
  ~Resampler();
  unsigned int execute(float in, float* out);

 private:
  resamp_rrrf object_;
};

}  // namespace liquid

#endif // LIQUID_WRAPPERS_H_
