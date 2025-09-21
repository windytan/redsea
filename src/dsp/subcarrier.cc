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
#include "src/dsp/subcarrier.hh"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <complex>
#include <cstdint>
#include <cstdio>
#include <tuple>

#include "src/constants.hh"
#include "src/dsp/liquid_wrappers.hh"
#include "src/io/bitbuffer.hh"
#include "src/io/input.hh"
#include "src/maybe.hh"

namespace redsea {

namespace {

constexpr float kAGCBandwidth_Hz     = 500.0f;
constexpr float kAGCInitialGain      = 0.08f;
constexpr int kLowpassOrder          = 255;
constexpr float kLowpassCutoff_Hz    = 2400.0f;
constexpr float kSymsyncBandwidth_Hz = 2200.0f;
constexpr int kSymsyncDelay          = 3;
constexpr int kResamplerDelay        = 13;
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

std::uint32_t DeltaDecoder::decode(std::uint32_t d) {
  const std::uint32_t bit = (d != prev_);
  prev_                   = d;
  return bit;
}

SubcarrierSet::SubcarrierSet(float samplerate)
    : resample_ratio_(kTargetSampleRate_Hz / samplerate), resampler_(kResamplerDelay) {
  for (auto& demod : demods_) {
    demod.agc.init(kAGCBandwidth_Hz / kTargetSampleRate_Hz, kAGCInitialGain);
    demod.fir_lpf.init(kLowpassOrder, kLowpassCutoff_Hz / kTargetSampleRate_Hz);
    demod.symsync.initRRC(kSamplesPerSymbol, kSymsyncDelay, kSymsyncBeta, 32);
    demod.symsync.setBandwidth(kSymsyncBandwidth_Hz / kTargetSampleRate_Hz);
    demod.symsync.setOutputRate(1);
    demod.oscillator.initNCO(angularFreq(57000.f, kTargetSampleRate_Hz));
    demod.oscillator.setPLLBandwidth(kPLLBandwidth_Hz / kTargetSampleRate_Hz);
  }
  resampler_.setRatio(resample_ratio_);
}

void SubcarrierSet::reset() {
  for (auto& demod : demods_) {
    demod.symsync.reset();
    demod.oscillator.reset();
  }
  sample_num_since_reset_ = 0;
}

// \return Reference to possibly resampled data; no resampling happens if resampling ratio is 1
const MPXBuffer& SubcarrierSet::resampleChunk(const MPXBuffer& input_chunk) {
  if (resample_ratio_ == 1.0f) {
    return input_chunk;
  }

  // ceil(resample_ratio) is enough, as per liquid-dsp's API, but std::ceil is not constexpr in
  // C++17
  constexpr std::size_t kMaxResamplerOutputSize = static_cast<std::size_t>(kMaxResampleRatio) + 1;
  std::array<float, kMaxResamplerOutputSize> resamp_output{};

  std::size_t i_resampled{};
  for (size_t i_input{}; i_input < input_chunk.used_size; i_input++) {
    const auto num_resampled = resampler_.execute(input_chunk.data[i_input], resamp_output);

    // Always true as per liquid-dsp API
    assert(num_resampled <= resamp_output.size());

    for (std::uint32_t i_buf{}; i_buf < num_resampled; i_buf++) {
      // Must always be true due to our selection of maximum resampler ratio and extra room in
      // the chunk
      assert(i_resampled < resampled_chunk_.data.size());

      resampled_chunk_.data[i_resampled] = resamp_output[i_buf];
      i_resampled++;
    }
  }
  resampled_chunk_.used_size = i_resampled;
  assert(resampled_chunk_.used_size <= resampled_chunk_.data.size());

  return resampled_chunk_;
}

// \brief Process a chunk of MPX into bits
// \param input_chunk MPX data (any sample rate)
// \param num_data_streams Number of RDS data streams to process (1 to 4)
// \return Raw bits without any block synchronization
BitBuffer SubcarrierSet::processChunk(const MPXBuffer& input_chunk, int num_data_streams) {
  const MPXBuffer& chunk = resampleChunk(input_chunk);

  BitBuffer bitbuffer;
  bitbuffer.time_received         = input_chunk.time_received;
  bitbuffer.chunk_time_from_start = static_cast<double>(sample_num_) / kTargetSampleRate_Hz;
  bitbuffer.n_streams             = num_data_streams;

  // Pre-allocate the bit buffers
  constexpr float over_reserve = 1.1f;
  const auto expected_num_bits = static_cast<std::size_t>(
      static_cast<float>(chunk.used_size) * kBitsPerSecond / kTargetSampleRate_Hz * over_reserve);
  for (int n_stream{0}; n_stream < num_data_streams; n_stream++) {
    bitbuffer.bits[n_stream].reserve(expected_num_bits);
  }

  // This is for timestamping bits (groups); the whole processing delay at 171 kHz
  const auto processing_delay_samples =
      std::lround(kResamplerDelay * resample_ratio_ + demods_[0].fir_lpf.getGroupDelay() +
                  1.5 * kSymsyncDelay * kDecimateRatio);

  for (size_t i_sample = 0; i_sample < chunk.used_size; i_sample++) {
    for (int n_stream{0}; n_stream < num_data_streams; n_stream++) {
      // Running at 171 kHz (receiver's clock)

      auto& subcarrier_context = demods_[n_stream];

      // Mix down to baseband
      const std::complex<float> sample_baseband = subcarrier_context.oscillator.mixDown(
          std::complex<float>(chunk.data[i_sample]), n_stream);

      demods_[n_stream].fir_lpf.push(sample_baseband);

      if (sample_num_since_reset_ % kDecimateRatio == 0) {
        // Running at 7.125 kHz (receiver's clock)

        std::complex<float> sample_lopass =
            demods_[n_stream].agc.execute(demods_[n_stream].fir_lpf.execute());

        // Synchronize to transmitter's biphase data clock
        const auto symbol = subcarrier_context.symsync.execute(sample_lopass);

        if (symbol.valid) {
          // Running at 2.375 kHz (transmitter's clock)

          // The symbol from liquid's modem is ignored; we only need the phase error.
          std::ignore = subcarrier_context.modem.demodulate(symbol.data);

          const float phase_error =
              std::min(std::max(subcarrier_context.modem.getPhaseError(), -kPi), kPi);
          subcarrier_context.oscillator.stepPLL(phase_error * kPLLMultiplier);

          const auto biphase = subcarrier_context.biphase_decoder.push(symbol.data);

          // One biphase symbol received for every 2 PSK symbols
          if (biphase.valid) {
            // Running at 1.1875 kHz (transmitter's clock)
            bitbuffer.bits[n_stream].push_back(
                TimedBit{static_cast<int>(subcarrier_context.delta_decoder.decode(biphase.data)),
                         static_cast<float>(static_cast<int>(i_sample) - processing_delay_samples) /
                             kTargetSampleRate_Hz});
          }
        }
      }  // decimate
      subcarrier_context.oscillator.step();
    }  // for n_stream

    // Overflows every 7 hours* which resets the time_from_start to zero.
    sample_num_++;

    // Overflows every 7 hours*. Two things will happen:
    //   1) The symbol synchronizer sees a sudden 75° phase jump**.
    //   2) There's a 5-second interval where we'll have to wait a little longer for a reset if
    //      one is needed at that exact time (unlikely and inconsequential)
    // Is it worth checking 171,000 times per second? We think not.
    //
    //   *) (2^32) / (171000 Hz) ≈ 6 h 58 min
    //  **) ((2^32) % decimate_ratio) / (decimate_ratio * kSamplesPerSymbol) * 360° =
    //      ((2^32) %        24     ) / (      24       *         3        ) * 360° = 75°
    sample_num_since_reset_++;
  }

  return bitbuffer;
}

bool SubcarrierSet::eof() const {
  return is_eof_;
}

// Seconds of audio processed since last reset.
/// \note Not to be used for measurements, since it will lose precision as the counter grows.
float SubcarrierSet::getSecondsSinceLastReset() const {
  return static_cast<float>(sample_num_since_reset_) / kTargetSampleRate_Hz;
}

}  // namespace redsea
