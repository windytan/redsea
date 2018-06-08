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
#ifndef SUBCARRIER_H_
#define SUBCARRIER_H_

#include <deque>
#include <complex>
#include <utility>
#include <vector>

#include "config.h"

#include "src/common.h"
#include "src/input.h"
#include "src/liquid_wrappers.h"
#include "src/options.h"

#ifdef HAVE_LIQUID

namespace redsea {

class BiphaseDecoder {
 public:
  BiphaseDecoder();
  ~BiphaseDecoder();
  std::pair<bool, std::complex<float>> push(
      const std::complex<float>& psk_symbol);

 private:
  std::complex<float> prev_psk_symbol_;
  std::vector<float> clock_history_;
  unsigned clock_;
  unsigned clock_polarity_;
};

class DeltaDecoder {
 public:
  DeltaDecoder();
  ~DeltaDecoder();
  unsigned Decode(unsigned d);

 private:
  unsigned prev_;
};

class Subcarrier {
 public:
  explicit Subcarrier(const Options& options);
  ~Subcarrier();
  std::vector<bool> PopBits();
  bool eof() const;
  void ProcessChunk(MPXBuffer<>& chunk);
#ifdef DEBUG
  float t() const;
#endif

 private:
  int sample_num_;
  float resample_ratio_;

  std::vector<bool> bit_buffer_;

  liquid::FIRFilter fir_lpf_;
  liquid::AGC agc_;
  liquid::NCO oscillator_;
  liquid::SymSync symsync_;
  liquid::Modem modem_;
  liquid::Resampler resampler_;

  MPXBuffer<> resampled_buffer_;

  bool is_eof_;

  DeltaDecoder delta_decoder_;
  BiphaseDecoder biphase_decoder_;

  std::complex<float> prev_sym_;
};

}  // namespace redsea

#endif  // HAVE_LIQUID
#endif  // SUBCARRIER_H_
