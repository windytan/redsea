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
#ifndef DSP_SUBCARRIER_H_
#define DSP_SUBCARRIER_H_

#include <array>
#include <complex>
#include <cstdint>

#include "src/constants.hh"
#include "src/dsp/liquid_wrappers.hh"
#include "src/io/bitbuffer.hh"
#include "src/io/input.hh"

namespace redsea {

class BiphaseDecoder {
 public:
  BiphaseDecoder() = default;
  Maybe<bool> push(std::complex<float> psk_symbol);

 private:
  std::complex<float> prev_psk_symbol_{0.0f, 0.0f};
  std::array<float, 128> clock_history_{};
  std::uint32_t clock_{};
  std::uint32_t clock_polarity_{};
};

class DeltaDecoder {
 public:
  DeltaDecoder() = default;
  std::uint32_t decode(std::uint32_t d);

 private:
  std::uint32_t prev_{0};
};

// \brief Demodulation context for one subcarrier
struct Demod {
  liquid::AGC agc;
  liquid::FIRFilter fir_lpf;
  liquid::SymSync symsync;
  DeltaDecoder delta_decoder;
  BiphaseDecoder biphase_decoder;
  liquid::NCO oscillator;
  liquid::Modem modem{LIQUID_MODEM_PSK2};
};

// A set of 1 (RDS1) to 4 (RDS2) subcarriers
class SubcarrierSet {
 public:
  explicit SubcarrierSet(float samplerate);
  bool eof() const;
  BitBuffer processChunk(const MPXBuffer& input_chunk, int num_data_streams);
  void reset();
  float getSecondsSinceLastReset() const;

 private:
  const MPXBuffer& resampleChunk(const MPXBuffer& input_chunk);

  static constexpr int kSamplesPerSymbol = 3;
  static constexpr int kDecimateRatio =
      static_cast<int>(kTargetSampleRate_Hz / kBitsPerSecond / 2 / kSamplesPerSymbol);

  // Samples since last reset
  std::uint32_t sample_num_{0};
  const float resample_ratio_;

  liquid::Resampler resampler_;

  std::array<Demod, 4> demods_;

  MPXBuffer resampled_chunk_{};

  bool is_eof_{false};
};

}  // namespace redsea

#endif  // DSP_SUBCARRIER_H_
