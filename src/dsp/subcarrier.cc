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
#include "src/dsp/subcarrier.h"

#include <cassert>
#include <cmath>
#include <complex>
#include <cstdio>

#include "src/common.h"
#include "src/dsp/liquid_wrappers.h"
#include "src/input.h"

namespace redsea {

namespace {

constexpr int kSamplesPerSymbol      = 3;
constexpr float kAGCBandwidth_Hz     = 500.0f;
constexpr float kAGCInitialGain      = 0.08f;
constexpr float kLowpassCutoff_Hz    = 2400.0f;
constexpr float kSymsyncBandwidth_Hz = 2200.0f;
constexpr int kSymsyncDelay          = 3;
constexpr float kSymsyncBeta         = 0.8f;
constexpr float kPLLBandwidth_Hz     = 0.03f;
constexpr float kPLLMultiplier       = 12.0f;

}  // namespace

// At correct clock phase, evaluate symbol in
// constellation {-1,0} => 0, {1,0} => 1
Maybe<bool> BiphaseDecoder::push(std::complex<float> psk_symbol) {
  Maybe<bool> result{};

  const auto biphase_symbol = (psk_symbol - prev_psk_symbol_) * 0.5f;
  result.data               = biphase_symbol.real() >= 0.0f;
  result.valid              = (clock_ % 2 == clock_polarity_);
  prev_psk_symbol_          = psk_symbol;

  clock_history_[clock_] = std::fabs(biphase_symbol.real());
  clock_++;

  // Periodically evaluate validity of the chosen biphase clock polarity
  if (clock_ == clock_history_.size()) {
    float a{};
    float b{};

    for (size_t i = 0; i < clock_history_.size(); i++) {
      if (i % 2 == 0)
        a += clock_history_[i];
      else
        b += clock_history_[i];
      clock_history_[i] = 0.f;
    }

    if (a > b)
      clock_polarity_ = 0;
    else if (b > a)
      clock_polarity_ = 1;

    clock_ = 0;
  }

  return result;
}

unsigned DeltaDecoder::decode(unsigned d) {
  const unsigned bit = (d != prev_);
  prev_              = d;
  return bit;
}

Subcarrier::Subcarrier(float carrier_frequency, float samplerate)
    : resample_ratio_(kTargetSampleRate_Hz / samplerate),
      fir_lpf_(255, kLowpassCutoff_Hz / kTargetSampleRate_Hz),
      agc_(kAGCBandwidth_Hz / kTargetSampleRate_Hz, kAGCInitialGain),
      oscillator_(LIQUID_NCO, angularFreq(carrier_frequency, kTargetSampleRate_Hz)),
      symsync_(LIQUID_FIRFILT_RRC, kSamplesPerSymbol, kSymsyncDelay, kSymsyncBeta, 32),
      modem_(LIQUID_MODEM_PSK2),
      resampler_(13) {
  symsync_.setBandwidth(kSymsyncBandwidth_Hz / kTargetSampleRate_Hz);
  symsync_.setOutputRate(1);
  oscillator_.setPLLBandwidth(kPLLBandwidth_Hz / kTargetSampleRate_Hz);
  resampler_.setRatio(resample_ratio_);
}

void Subcarrier::reset() {
  symsync_.reset();
  oscillator_.reset();
  sample_num_ = 0;
}

/** MPX to bits
 */
BitBuffer Subcarrier::processChunk(MPXBuffer& input_chunk) {
  if (resample_ratio_ != 1.0f) {
    std::size_t i_resampled{};

    // ceil(resample_ratio) is enough, as per liquid-dsp's API, but std::ceil is not constexpr in
    // C++14
    constexpr std::size_t kMaxResamplerOutputSize = static_cast<std::size_t>(kMaxResampleRatio) + 1;
    std::array<float, kMaxResamplerOutputSize> resamp_output{};

    for (size_t i_input{}; i_input < input_chunk.used_size; i_input++) {
      const auto num_resampled = resampler_.execute(input_chunk.data[i_input], resamp_output);

      // Always true as per liquid-dsp API
      assert(num_resampled <= resamp_output.size());

      for (unsigned int i_buf{}; i_buf < num_resampled; i_buf++) {
        // Must always be true due to our selection of maximum resampler ratio and extra room in
        // the chunk
        assert(i_resampled < resampled_chunk_.data.size());

        resampled_chunk_.data[i_resampled] = resamp_output[i_buf];
        i_resampled++;
      }
    }
    resampled_chunk_.used_size = i_resampled;
    assert(resampled_chunk_.used_size <= resampled_chunk_.data.size());
  }

  MPXBuffer& chunk = (resample_ratio_ == 1.0f ? input_chunk : resampled_chunk_);

  constexpr int decimate_ratio =
      static_cast<int>(kTargetSampleRate_Hz / kBitsPerSecond / 2 / kSamplesPerSymbol);

  BitBuffer bitbuffer;
  bitbuffer.time_received = input_chunk.time_received;

  for (size_t i = 0; i < chunk.used_size; i++) {
    // Mix RDS to baseband for filtering purposes
    const std::complex<float> sample_baseband =
        oscillator_.mixDown(std::complex<float>(chunk.data[i]));

    fir_lpf_.push(sample_baseband);

    if (sample_num_ % decimate_ratio == 0) {
      std::complex<float> sample_lopass = agc_.execute(fir_lpf_.execute());

      const auto symbol = symsync_.execute(sample_lopass);

      if (symbol.valid) {
        // Modem here is only used to track PLL phase error
        modem_.demodulate(symbol.data);

        const float phase_error = std::min(
            std::max(modem_.getPhaseError(), -static_cast<float>(M_PI)), static_cast<float>(M_PI));
        oscillator_.stepPLL(phase_error * kPLLMultiplier);

        const auto biphase = biphase_decoder_.push(symbol.data);

        // One biphase symbol received for every 2 PSK symbols
        if (biphase.valid) {
          bitbuffer.bits.push_back(delta_decoder_.decode(biphase.data));
        }
      }
    }

    oscillator_.step();

    // Overflows every 7 hours*. Two things will happen:
    //   1) The symbol synchronizer sees a sudden 75° phase jump**.
    //   2) There's a 5-second interval where we'll have to wait a little longer for a reset if
    //      one is needed at that exact time (unlikely and inconsequential)
    // Is it worth checking 171,000 times per second? We think not.
    //
    //   *) (2^32) / (171000 Hz) ≈ 6 h 58 min
    //  **) ((2^32) % decimate_ratio) / (decimate_ratio * kSamplesPerSymbol) * 360° =
    //      ((2^32) %        24     ) / (      24       *         3        ) * 360° = 75°
    sample_num_++;
  }

  return bitbuffer;
}

bool Subcarrier::eof() const {
  return is_eof_;
}

// Seconds of audio processed since last reset.
/// \note Not to be used for measurements, since it will lose precision as the counter grows.
float Subcarrier::getSecondsSinceLastReset() const {
  return static_cast<float>(sample_num_) / kTargetSampleRate_Hz;
}

}  // namespace redsea
