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
#ifndef DSP_LIQUID_WRAPPERS_H_
#define DSP_LIQUID_WRAPPERS_H_

#include <array>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <vector>

constexpr float kPi{3.14159265358979323846f};
constexpr float k2Pi{2.f * kPi};

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
// https://github.com/jgaeddert/liquid-dsp/issues/229
#pragma clang diagnostic ignored "-Wreturn-type-c-linkage"
extern "C" {
#include "liquid/liquid.h"
}
#pragma clang diagnostic pop

namespace redsea {

// Hertz to radians per sample
inline float angularFreq(float hertz, float samplerate) {
  return hertz * k2Pi / samplerate;
}

template <typename T>
struct Maybe {
  T data;
  bool valid;
};

}  // namespace redsea

namespace liquid {

class AGC {
 public:
  AGC() = default;
  void init(float bw, float initial_gain);
  AGC(const AGC&)             = delete;
  AGC& operator=(const AGC&)  = delete;
  AGC(AGC&& other)            = delete;
  AGC& operator=(AGC&& other) = delete;

  ~AGC();
  std::complex<float> execute(std::complex<float> s);

 private:
  agc_crcf object_{nullptr};
};

class FIRFilter {
 public:
  FIRFilter()                             = default;
  FIRFilter(const FIRFilter&)             = delete;
  FIRFilter& operator=(const FIRFilter&)  = delete;
  FIRFilter(FIRFilter&& other)            = delete;
  FIRFilter& operator=(FIRFilter&& other) = delete;

  ~FIRFilter();
  void init(std::uint32_t len, float fc, float As = 60.0f, float mu = 0.0f);

  void push(std::complex<float> s);
  std::complex<float> execute();
  std::size_t length() const;

 private:
  firfilt_crcf object_{nullptr};
};

class NCO {
 public:
  NCO() = default;
  NCO(liquid_ncotype type, float freq);
  NCO(const NCO&)             = delete;
  NCO& operator=(const NCO&)  = delete;
  NCO(NCO&& other)            = delete;
  NCO& operator=(NCO&& other) = delete;
  ~NCO();
  void init(liquid_ncotype type, float freq);
  std::complex<float> mixDown(std::complex<float> s, int n_stream = 0);
  void step();
  void setPLLBandwidth(float);
  void stepPLL(float dphi);
  void reset();
  std::complex<float> get(int n_stream) const {
    return std::polar(1.f, -phases_[n_stream]);
  }

 private:
  nco_crcf object_{nullptr};
  float initial_frequency_{};
  float prev_f0_phase_{};
  std::array<float, 4> phases_{};
};

class SymSync {
 public:
  SymSync() = default;
  void init(liquid_firfilt_type ftype, std::uint32_t k, std::uint32_t m, float beta,
            std::uint32_t num_filters);
  SymSync(const SymSync&)             = delete;
  SymSync& operator=(const SymSync&)  = delete;
  SymSync(SymSync&& other)            = delete;
  SymSync& operator=(SymSync&& other) = delete;
  ~SymSync();
  void reset();
  void setBandwidth(float);
  void setOutputRate(std::uint32_t);
  redsea::Maybe<std::complex<float>> execute(std::complex<float>& in);

 private:
  symsync_crcf object_{nullptr};
  std::vector<std::complex<float>> out_;
};

class Modem {
 public:
  explicit Modem(modulation_scheme scheme);
  Modem(const Modem&)             = delete;
  Modem& operator=(const Modem&)  = delete;
  Modem(Modem&& other)            = delete;
  Modem& operator=(Modem&& other) = delete;
  ~Modem();
  std::uint32_t demodulate(std::complex<float> sample);
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
  static constexpr std::size_t kOutputArraySize{2ULL};

  explicit Resampler(std::uint32_t length);
  Resampler(const Resampler&)             = delete;
  Resampler& operator=(const Resampler&)  = delete;
  Resampler(Resampler&& other)            = delete;
  Resampler& operator=(Resampler&& other) = delete;
  ~Resampler();
  void setRatio(float ratio);

  std::uint32_t execute(float in, std::array<float, kOutputArraySize>& out);

 private:
  resamp_rrrf object_;
};

}  // namespace liquid

#endif  // DSP_LIQUID_WRAPPERS_H_
