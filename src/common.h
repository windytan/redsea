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
#include <string>
#include <vector>

namespace redsea {

constexpr float kBitsPerSecond        = 1187.5f;
constexpr float kMinimumSampleRate_Hz = 128000.0f;
constexpr int   kNumBlerAverageGroups = 12;
constexpr float kTargetSampleRate_Hz  = 171000.0f;

struct BitBuffer {
  std::chrono::time_point<std::chrono::system_clock> time_received;
  std::vector<bool> bits;
};

std::string getTimePointString(
    const std::chrono::time_point<std::chrono::system_clock>& timepoint,
    const std::string& format);

}  // namespace redsea
#endif  // COMMON_H_
