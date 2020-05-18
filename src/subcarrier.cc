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
constexpr float kLowpassCutoff_Hz     = 2400.0f;
constexpr float kSymsyncBandwidth_Hz  = 2200.0f;
constexpr int   kSymsyncDelay         = 3;
constexpr float kSymsyncBeta          = 0.8f;
constexpr float kPLLBandwidth_Hz      = 0.01f;
constexpr float kPLLMultiplier        = 12.0f;

constexpr float hertz2step(float Hz) {
  return Hz * 2.0f * float(M_PI) / kTargetSampleRate_Hz;
}

}  // namespace

BiphaseDecoder::BiphaseDecoder() : clock_history_(128) {
}

// At correct clock phase, return binary symbol in
// constellation {-1,0} => 0, {1,0} => 1
Maybe<std::complex<float>> BiphaseDecoder::push(
    std::complex<float> psk_symbol) {

  Maybe<std::complex<float>> result;

  result.data = (psk_symbol - prev_psk_symbol_) * 0.5f;
  result.valid = (clock_ % 2 == clock_polarity_);
  prev_psk_symbol_ = psk_symbol;

  clock_history_[clock_] = std::fabs(result.data.real());
  clock_++;

  // Periodically evaluate validity of the chosen biphase clock polarity
  if (clock_ == clock_history_.size()) {
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

  return result;
}

unsigned DeltaDecoder::decode(unsigned d) {
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
  symsync_.setBandwidth(kSymsyncBandwidth_Hz / kTargetSampleRate_Hz);
  symsync_.setOutputRate(1);
  oscillator_.setPLLBandwidth(kPLLBandwidth_Hz / kTargetSampleRate_Hz);
}

/** MPX to bits
 */
BitBuffer Subcarrier::processChunk(MPXBuffer<>& chunk) {
  if (resample_ratio_ != 1.0f) {
    unsigned int i_resampled = 0;
    for (size_t i = 0; i < chunk.used_size; i++) {
      static float buf[4];
      unsigned int num_resampled = resampler_.execute(chunk.data[i], buf);

      for (unsigned int j = 0; j < num_resampled; j++) {
        resampled_buffer_.data[i_resampled] = buf[j];
        i_resampled++;
      }
    }
    resampled_buffer_.used_size = i_resampled;
  }

  MPXBuffer<>& buf = (resample_ratio_ == 1.0f ? chunk : resampled_buffer_);

  constexpr int decimate_ratio = int(kTargetSampleRate_Hz / kBitsPerSecond / 2 /
                                     kSamplesPerSymbol);

  BitBuffer bitbuffer;
  bitbuffer.time_received = chunk.time_received;

  for (size_t i = 0; i < buf.used_size; i++) {
    // Mix RDS to baseband for filtering purposes
    std::complex<float> sample_baseband =
        oscillator_.mixDown(std::complex<float>(buf.data[i]));

    fir_lpf_.push(sample_baseband);

    if (sample_num_ % decimate_ratio == 0) {
      std::complex<float> sample_lopass = agc_.execute(fir_lpf_.execute());

      auto symbol = symsync_.execute(&sample_lopass);

      if (symbol.valid) {
        // Modem here is only used to track PLL phase error
        modem_.demodulate(symbol.data);
        oscillator_.stepPLL(modem_.getPhaseError() * kPLLMultiplier);

        auto biphase = biphase_decoder_.push(symbol.data);

        // One biphase symbol received for every 2 PSK symbols
        if (biphase.valid) {
          bitbuffer.bits.push_back(delta_decoder_.decode(
                                   biphase.data.real() >= 0.0f));
        }
      }
    }

    oscillator_.step();

    sample_num_++;
  }

  return bitbuffer;
}

bool Subcarrier::eof() const {
  return is_eof_;
}

}  // namespace redsea

#endif  // HAVE_LIQUID
