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

#ifdef HAVE_SNDFILE
#include <sndfile.h>
#endif

#include "src/common.h"
#include "src/liquid_wrappers.h"

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
  unsigned Decode(unsigned);

 private:
  unsigned prev_;
};

class MPXReader {
 public:
  bool eof() const;
  virtual std::vector<float> ReadBlock() = 0;
  virtual float samplerate() const = 0;

 protected:
  bool is_eof_;
};

class StdinReader : public MPXReader {
 public:
  explicit StdinReader(const Options& options);
  ~StdinReader();
  std::vector<float> ReadBlock() override;
  float samplerate() const override;

 private:
  float samplerate_;
  int16_t* buffer_;
  bool feed_thru_;
};

#ifdef HAVE_SNDFILE
class SndfileReader : public MPXReader {
 public:
  explicit SndfileReader(const Options& options);
  ~SndfileReader();
  std::vector<float> ReadBlock() override;
  float samplerate() const override;

 private:
  SF_INFO info_;
  SNDFILE* file_;
  float* buffer_;
};
#endif

class Subcarrier {
 public:
  explicit Subcarrier(const Options& options);
  ~Subcarrier();
  int NextBit();
  bool eof() const;
#ifdef DEBUG
  float t() const;
#endif

 private:
  void DemodulateMoreBits();
  int  numsamples_;
  float resample_ratio_;

  std::deque<int> bit_buffer_;

  liquid::FIRFilter fir_lpf_;
  liquid::AGC agc_;
  liquid::NCO oscillator_approx_;
  liquid::NCO oscillator_exact_;
  liquid::SymSync symsync_;
  liquid::Modem modem_;
  liquid::Resampler resampler_;

  bool is_eof_;

  DeltaDecoder delta_decoder_;
  BiphaseDecoder biphase_decoder_;

  std::complex<float> prev_sym_;

  MPXReader* mpx_;
};

}  // namespace redsea

#endif  // HAVE_LIQUID
#endif  // SUBCARRIER_H_
