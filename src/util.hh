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
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <numeric>
#include <sstream>
#include <string>
#include <vector>

namespace redsea {

// extract N-bit integer from word, starting at starting_at from the right
template <std::size_t N>
std::uint16_t getBits(std::uint16_t word, std::size_t starting_at) {
  static_assert(N > 0U && N <= 16U, "");
  assert(starting_at + N <= 16U);
  return static_cast<std::uint16_t>(word >> starting_at) &
         (static_cast<std::uint16_t>(1U << N) - 1U);
}

// extract N-bit integer from the concatenation of word1 and word2, starting at
// starting_at from the right
template <std::size_t N>
std::uint32_t getBits(std::uint16_t word1, std::uint16_t word2, std::size_t starting_at) {
  static_assert(N > 0U && N <= 32U, "");
  assert(starting_at + N <= 32U);
  const auto concat =
      static_cast<std::uint32_t>(word2 + (static_cast<std::uint32_t>(word1) << 16U));
  return static_cast<std::uint32_t>(concat >> starting_at) &
         (static_cast<std::uint32_t>(1U << N) - 1U);
}

// extract boolean flag at bit position bit_pos
inline bool getBool(std::uint16_t word, std::size_t bit_pos) {
  assert(bit_pos < 16);
  return static_cast<bool>(getBits<1>(word, bit_pos));
}
inline std::uint8_t getUint8(std::uint16_t word, std::size_t bit_pos) {
  assert(bit_pos < 16);
  return static_cast<std::uint8_t>(getBits<8>(word, bit_pos));
}

template <typename T>
constexpr T divideRoundingUp(T dividend, T divisor) {
  static_assert(std::is_integral<T>::value, "");
  return (dividend + divisor - 1) / divisor;
}

std::string getHoursMinutesString(int hour, int minute);
std::string getTimePointString(const std::chrono::time_point<std::chrono::system_clock>& timepoint,
                               const std::string& format);

std::string join(const std::vector<std::string>& strings, const std::string& d);

template <int N>
std::string getHexString(std::uint32_t value) {
  static_assert(N > 0, "");
  std::stringstream ss;

  ss.fill('0');
  ss.setf(std::ios_base::uppercase);

  ss << std::hex << std::setw(N) << value;

  return ss.str();
}

template <int N>
std::string getPrefixedHexString(std::uint32_t value) {
  return "0x" + getHexString<N>(value);
}

class CarrierFrequency {
 public:
  enum class Band : std::uint8_t { LF_MF, FM };

  explicit CarrierFrequency(std::uint16_t code, Band band = Band::FM);
  bool isValid() const;
  int kHz() const;
  std::string str() const;
  friend bool operator==(const CarrierFrequency& f1, const CarrierFrequency& f2);
  friend bool operator<(const CarrierFrequency& f1, const CarrierFrequency& f2);

 private:
  std::uint16_t code_{};
  Band band_{Band::FM};
};

class AltFreqList {
 public:
  AltFreqList() = default;
  void insert(std::uint16_t af_code);
  bool isComplete() const;
  bool isMethodB() const;
  std::vector<int> getRawList() const;
  void clear();

 private:
  std::array<int, 25> alt_freqs_{};
  std::size_t num_expected_{0};
  std::size_t num_received_{0};
  bool lf_mf_follows_{false};
};

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

  float getAverage() const {
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

std::string rtrim(std::string s);

}  // namespace redsea
#endif  // UTIL_H_
