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
#ifndef UTIL_H_
#define UTIL_H_

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <numeric>

namespace redsea {

// extract N-bit integer from word, starting at starting_at from the right
std::uint16_t getBits(std::uint16_t word, std::size_t starting_at, std::size_t length);

// extract N-bit integer from the concatenation of word1 and word2, starting at
// starting_at from the right
std::uint32_t getBits(std::uint16_t word1, std::uint16_t word2, std::size_t starting_at,
                      std::size_t length);

// extract boolean flag at bit position bit_pos
bool getBool(std::uint16_t word, std::size_t bit_pos);
std::uint8_t getUint8(std::uint16_t word, std::size_t bit_pos);

template <typename T>
constexpr T divideRoundingUp(T dividend, T divisor) {
  static_assert(std::is_integral_v<T>);
  return (dividend + divisor - 1) / divisor;
}

template <typename T, std::size_t N>
class RunningSum {
 public:
  RunningSum() {
    clear();
  }
  T getSum() const {
    return std::accumulate(history_.cbegin(), history_.cend(), T{0});
  }
  void push(int number) {
    history_[pointer_] = number;
    pointer_           = (pointer_ + 1) % history_.size();
  }
  void clear() {
    std::fill(history_.begin(), history_.end(), T{0});
  }

 private:
  std::array<T, N> history_{};
  std::size_t pointer_{};
};

template <typename T, std::size_t N>
class RunningAverage {
 public:
  RunningAverage() {
    std::fill(history_.begin(), history_.end(), T{0});
  }
  void push(T value) {
    sum_ -= history_[ptr_];
    history_[ptr_] = value;
    sum_ += value;
    ptr_ = (ptr_ + 1) % history_.size();
  }

  [[nodiscard]] float getAverage() const {
    return history_.empty() ? 0.f : static_cast<float>(sum_) / history_.size();
  }

 private:
  std::array<T, N> history_{};
  T sum_{};
  std::size_t ptr_{};
};

template <typename T, std::size_t Length>
class DelayLine {
 public:
  DelayLine() = default;
  void push(T value) {
    buffer_[ptr_] = value;
    ptr_          = (ptr_ + 1) % buffer_.size();
  }
  T get() const {
    return buffer_[ptr_];
  }

 private:
  std::array<T, Length> buffer_{};
  std::size_t ptr_{};
};

}  // namespace redsea

#endif  // UTIL_H_
