#ifndef LIQUID_WRAPPERS_H_
#define LIQUID_WRAPPERS_H_

#include "config.h"
#ifdef HAVE_LIQUID

#include <complex>
#include <vector>

#include "liquid/liquid.h"

namespace liquid {

class AGC {
 public:
  AGC(float bw, float initial_gain);
  ~AGC();
  std::complex<float> execute(std::complex<float> s);
  float gain();

 private:
  agc_crcf object_;
};

class FIRFilter {
 public:
  FIRFilter(int len, float fc, float As = 60.0f, float mu = 0.0f);
  ~FIRFilter();
  void push(std::complex<float> s);
  std::complex<float> execute();

 private:
  firfilt_crcf object_;
};

class NCO {
 public:
  explicit NCO(float freq);
  ~NCO();
  std::complex<float> MixDown(std::complex<float> s);
  std::complex<float> MixUp(std::complex<float> s);
  void MixBlockDown(std::complex<float>* x, std::complex<float>* y,
      int n);
  void Step();
  void set_pll_bandwidth(float);
  void StepPLL(float dphi);
  float frequency();

 private:
  nco_crcf object_;
};

class SymSync {
 public:
  SymSync(liquid_firfilt_type ftype, unsigned k, unsigned m,
          float beta, unsigned num_filters);
  ~SymSync();
  void set_bandwidth(float);
  void set_output_rate(unsigned);
  std::vector<std::complex<float>> execute(std::complex<float> in);

 private:
  symsync_crcf object_;
};

class Modem {
 public:
  explicit Modem(modulation_scheme scheme);
  ~Modem();
  unsigned int Demodulate(std::complex<float> sample);
  float phase_error();

 private:
  modem object_;
};

class Resampler {
 public:
  explicit Resampler(float ratio, int length);
  unsigned int execute(std::complex<float> in, std::complex<float>* out);
  ~Resampler();

 private:
  resamp_crcf object_;
  std::complex<float> outbuffer_[2];
};

}  // namespace liquid

#endif // HAVE_LIQUID

#endif // LIQUID_WRAPPERS_H_
