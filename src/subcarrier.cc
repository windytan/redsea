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
#include "src/subcarrier.h"

#ifdef HAVE_LIQUID

#include <cmath>
#include <complex>
#include <cstdio>
#include <deque>
#include <iostream>
#include <tuple>

#include <sndfile.h>

#include "src/common.h"
#include "src/input.h"
#include "src/liquid_wrappers.h"

namespace redsea {

namespace {

constexpr float kCarrierFrequency_Hz  = 57000.0f;
constexpr float kBitsPerSecond        = 1187.5f;
constexpr int   kSamplesPerSymbol     = 3;
constexpr float kAGCBandwidth_Hz      = 500.0f;
constexpr float kAGCInitialGain       = 0.08f;
constexpr float kLowpassCutoff_Hz     = 2600.0f;
constexpr float kSymsyncBandwidth_Hz  = 2300.0f;
constexpr int   kSymsyncDelay         = 3;
constexpr float kSymsyncBeta          = 0.8f;
constexpr float kPLLBandwidth_Hz      = 0.01f;
constexpr float kPLLMultiplier        = 12.0f;

constexpr float hertz2step(float Hz) {
  return Hz * 2.0f * M_PI / kTargetSampleRate_Hz;
}

#ifdef DEBUG
constexpr float step2hertz(float step) {
  return step * kTargetSampleRate_Hz / (2.0f * M_PI);
}
#endif

}  // namespace

BiphaseDecoder::BiphaseDecoder() : prev_psk_symbol_(0.0f),
  clock_history_(128), clock_(0), clock_polarity_(0) {
}

BiphaseDecoder::~BiphaseDecoder() {
}

// Return {is_clock, symbol}
//   is_clock: true if symbol valid
//   symbol:   binary symbol in constellation {-1,0} => 0, {1,0} => 1
std::pair<bool, std::complex<float>> BiphaseDecoder::push(
    const std::complex<float>& psk_symbol) {

  std::complex<float> biphase = (psk_symbol - prev_psk_symbol_) * 0.5f;
  bool is_clock = (clock_ % 2 == clock_polarity_);

  clock_history_[clock_] = std::fabs(biphase.real());

  // Periodically evaluate validity of the chosen biphase clock polarity
  if (++clock_ == clock_history_.size()) {
    float a = 0;
    float b = 0;

    for (size_t i = 0; i < clock_history_.size(); i++) {
      if (i % 2 == 0)
        a += clock_history_[i];
      else
        b += clock_history_[i];
      clock_history_[i] = 0.f;
    }

    if      (a > b) clock_polarity_ = 0;
    else if (b > a) clock_polarity_ = 1;

    clock_ = 0;
  }

  prev_psk_symbol_ = psk_symbol;

  return {is_clock, biphase};
}

DeltaDecoder::DeltaDecoder() : prev_(0) {
}

DeltaDecoder::~DeltaDecoder() {
}

unsigned DeltaDecoder::Decode(unsigned d) {
  unsigned bit = (d != prev_);
  prev_ = d;
  return bit;
}

Subcarrier::Subcarrier(const Options& options) :
    resample_ratio_(kTargetSampleRate_Hz / options.samplerate),
    fir_lpf_(255, kLowpassCutoff_Hz / kTargetSampleRate_Hz),
    agc_(kAGCBandwidth_Hz / kTargetSampleRate_Hz, kAGCInitialGain),
    oscillator_(LIQUID_NCO, hertz2step(kCarrierFrequency_Hz)),
    symsync_(LIQUID_FIRFILT_RRC, kSamplesPerSymbol, kSymsyncDelay,
             kSymsyncBeta, 32),
    modem_(LIQUID_MODEM_PSK2),
    resampler_(resample_ratio_, 13) {
  symsync_.set_bandwidth(kSymsyncBandwidth_Hz / kTargetSampleRate_Hz);
  symsync_.set_output_rate(1);
  oscillator_.set_pll_bandwidth(kPLLBandwidth_Hz / kTargetSampleRate_Hz);
}

/** MPX to bits
 */
std::vector<bool> Subcarrier::ProcessChunk(MPXBuffer<>& chunk) {
  if (resample_ratio_ != 1.0f) {
    int i_resampled = 0;
    for (size_t i = 0; i < chunk.used_size; i++) {
      static float buf[4];
      int num_resampled = resampler_.execute(chunk.data[i], buf);

      for (int j = 0; j < num_resampled; j++) {
        resampled_buffer_.data[i_resampled] = buf[j];
        i_resampled++;
      }
    }
    resampled_buffer_.used_size = i_resampled;
  }

  MPXBuffer<>& buf = (resample_ratio_ == 1.0f ? chunk : resampled_buffer_);

  constexpr int decimate_ratio = kTargetSampleRate_Hz / kBitsPerSecond / 2 /
                                 kSamplesPerSymbol;

  std::vector<bool> bits;

  for (size_t i = 0; i < buf.used_size; i++) {
    // Mix RDS to baseband for filtering purposes
    std::complex<float> sample_baseband =
        oscillator_.MixDown(std::complex<float>(buf.data[i]));

    fir_lpf_.push(sample_baseband);

    if (sample_num_ % decimate_ratio == 0) {
      std::complex<float> sample_lopass = agc_.execute(fir_lpf_.execute());

      auto symbol_optional = symsync_.execute(&sample_lopass);

      if (symbol_optional.first) {
#ifdef DEBUG
        printf("sy:%f,%f,%f\n",
            sample_num_ / kTargetSampleRate_Hz,
            symbol.real(),
            symbol.imag());
#endif

        // Modem here is only used to track PLL phase error
        modem_.Demodulate(symbol_optional.second);
        oscillator_.StepPLL(modem_.phase_error() * kPLLMultiplier);

        bool is_clock;
        std::complex<float> biphase;
        std::tie(is_clock, biphase) = biphase_decoder_.push(symbol_optional.second);

        // One biphase symbol received for every 2 PSK symbols
        if (is_clock) {
          bit_buffer_.push_back(delta_decoder_.Decode(
                                biphase.real() >= 0));
#ifdef DEBUG
          printf("bi:%f,%f,%f\n",
              sample_num_ / kTargetSampleRate_Hz,
              biphase.real(),
              biphase.imag());
#endif
        }
      }
    }

    oscillator_.Step();

    sample_num_++;
  }

  return bits;
}

bool Subcarrier::eof() const {
  return is_eof_;
}

}  // namespace redsea

#endif  // HAVE_LIQUID
