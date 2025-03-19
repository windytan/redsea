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

#include "src/constants.hh"
#include "src/dsp/liquid_wrappers.hh"
#include "src/io/bitbuffer.hh"
#include "src/io/input.hh"

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

std::uint32_t DeltaDecoder::decode(std::uint32_t d) {
  const std::uint32_t bit = (d != prev_);
  prev_                   = d;
  return bit;
}

Subcarriers::Subcarriers(float samplerate)
    : resample_ratio_(kTargetSampleRate_Hz / samplerate),
      oscillator_(LIQUID_NCO, angularFreq(57000.f, kTargetSampleRate_Hz)),
      modem_(LIQUID_MODEM_PSK2),
      resampler_(13) {
  for (auto& demod : demods_) {
    demod.agc.init(kAGCBandwidth_Hz / kTargetSampleRate_Hz, kAGCInitialGain);
    demod.fir_lpf.init(255, kLowpassCutoff_Hz / kTargetSampleRate_Hz);
    demod.symsync.init(LIQUID_FIRFILT_RRC, kSamplesPerSymbol, kSymsyncDelay, kSymsyncBeta, 32);
    demod.symsync.setBandwidth(kSymsyncBandwidth_Hz / kTargetSampleRate_Hz);
    demod.symsync.setOutputRate(1);
  }
  oscillator_.setPLLBandwidth(kPLLBandwidth_Hz / kTargetSampleRate_Hz);
  resampler_.setRatio(resample_ratio_);
}

void Subcarriers::reset() {
  for (auto& demod : demods_) {
    demod.symsync.reset();
  }
  oscillator_.reset();
  sample_num_ = 0;
}

// \return Reference to possibly resampled data; no resampling happens if resampling ratio is 1
const MPXBuffer& Subcarriers::resampleChunk(const MPXBuffer& input_chunk) {
  if (resample_ratio_ == 1.0f) {
    return input_chunk;
  }

  // ceil(resample_ratio) is enough, as per liquid-dsp's API, but std::ceil is not constexpr in
  // C++14
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

// \brief Process a chunk of MPX
// \param input_chunk MPX data
// \param num_data_streams Number of RDS data streams to process (1 to 4)
// \return Unsynchronized bits
BitBuffer Subcarriers::processChunk(const MPXBuffer& input_chunk, int num_data_streams) {
  const MPXBuffer& chunk = resampleChunk(input_chunk);

  constexpr int decimate_ratio =
      static_cast<int>(kTargetSampleRate_Hz / kBitsPerSecond / 2 / kSamplesPerSymbol);

  BitBuffer bitbuffer;
  bitbuffer.time_received = input_chunk.time_received;

  // Pre-allocate the bit buffers
  const auto expected_num_bits = static_cast<std::size_t>(
      static_cast<float>(chunk.used_size) * kBitsPerSecond / kTargetSampleRate_Hz * 1.1f);
  for (int n_stream{0}; n_stream < num_data_streams; n_stream++) {
    bitbuffer.bits[n_stream].reserve(expected_num_bits);
  }

  for (size_t i = 0; i < chunk.used_size; i++) {
    for (int n_stream{0}; n_stream < num_data_streams; n_stream++) {
      // Mix RDS to baseband for filtering purposes

      const std::complex<float> sample_baseband =
          oscillator_.mixDown(std::complex<float>(chunk.data[i]), n_stream);

      demods_[n_stream].fir_lpf.push(sample_baseband);

      if (sample_num_ % decimate_ratio == 0) {
        std::complex<float> sample_lopass =
            demods_[n_stream].agc.execute(demods_[n_stream].fir_lpf.execute());

        const auto symbol = demods_[n_stream].symsync.execute(sample_lopass);

        if (symbol.valid) {
          // Since all streams are phase-locked we'll only track stream 0 with the PLL
          if (n_stream == 0) {
            modem_.demodulate(symbol.data);

            const float phase_error = std::min(std::max(modem_.getPhaseError(), -kPi), kPi);
            oscillator_.stepPLL(phase_error * kPLLMultiplier);
          }

          const auto biphase = demods_[n_stream].biphase_decoder.push(symbol.data);

          // One biphase symbol received for every 2 PSK symbols
          if (biphase.valid) {
            bitbuffer.bits[n_stream].push_back(
                static_cast<int>(demods_[n_stream].delta_decoder.decode(biphase.data)));
          }
        }
      }
    }  // for n_stream

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

bool Subcarriers::eof() const {
  return is_eof_;
}

// Seconds of audio processed since last reset.
/// \note Not to be used for measurements, since it will lose precision as the counter grows.
float Subcarriers::getSecondsSinceLastReset() const {
  return static_cast<float>(sample_num_) / kTargetSampleRate_Hz;
}

}  // namespace redsea
