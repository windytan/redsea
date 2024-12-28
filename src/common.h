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
#ifndef COMMON_H_
#define COMMON_H_

#include <chrono>
#include <vector>

namespace redsea {

constexpr float kBitsPerSecond        = 1187.5f;
// Minimum sensible rate to still have RDS below Nyquist
constexpr float kMinimumSampleRate_Hz = 128'000.f;
constexpr int kNumBlerAverageGroups   = 12;
constexpr float kTargetSampleRate_Hz  = 171'000.f;

// Limit of the resamp_rrrf object
constexpr float kLiquidMinimumResamplerRatio = 0.004f;
constexpr float kMaximumSampleRate_Hz        = 40'000'000.f;
static_assert(kMaximumSampleRate_Hz < kTargetSampleRate_Hz / kLiquidMinimumResamplerRatio, "");

constexpr float kMaxResampleRatio = kTargetSampleRate_Hz / kMinimumSampleRate_Hz;

struct BitBuffer {
  std::chrono::time_point<std::chrono::system_clock> time_received;
  std::vector<bool> bits;
};

}  // namespace redsea
#endif  // COMMON_H_
