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
#include <chrono>
#include <cstdint>
#include <numeric>
#include <string>
#include <vector>

namespace redsea {

// extract N-bit integer from word, starting at starting_at from the right
template <size_t N>
uint16_t getBits(uint16_t word, size_t starting_at) {
  static_assert(N > 0 && N <= 16, "");
  return (word >> starting_at) & ((1 << N) - 1);
}

// extract N-bit integer from the concatenation of word1 and word2, starting at
// starting_at from the right
template <size_t N>
uint32_t getBits(uint16_t word1, uint16_t word2, size_t starting_at) {
  static_assert(N > 0 && N <= 32, "");
  return (((word1 << 16) + word2) >> starting_at) & ((1 << N) - 1);
}

// extract boolean flag at bit position bit_pos
inline bool getBool(uint16_t word, size_t bit_pos) {
  return static_cast<bool>(getBits<1>(word, bit_pos));
}
inline uint8_t getUint8(uint16_t word, size_t bit_pos) {
  return static_cast<uint8_t>(getBits<8>(word, bit_pos));
}

std::string getHoursMinutesString(int hour, int minute);
std::string getTimePointString(const std::chrono::time_point<std::chrono::system_clock>& timepoint,
                               const std::string& format);

std::string join(const std::vector<std::string>& strings, const std::string& d);

std::string getHexString(uint32_t value, int num_nybbles);
std::string getPrefixedHexString(uint32_t value, int num_nybbles);

class CarrierFrequency {
 public:
  enum class Band { LF_MF, FM };

 public:
  explicit CarrierFrequency(uint16_t code, Band band = Band::FM);
  bool isValid() const;
  int kHz() const;
  std::string str() const;
  friend bool operator==(const CarrierFrequency& f1, const CarrierFrequency& f2);
  friend bool operator<(const CarrierFrequency& f1, const CarrierFrequency& f2);

 private:
  uint16_t code_{};
  Band band_{Band::FM};
};

class AltFreqList {
 public:
  AltFreqList() = default;
  void insert(uint16_t af_code);
  bool isComplete() const;
  bool isMethodB() const;
  std::vector<int> getRawList() const;
  void clear();

 private:
  std::array<int, 25> alt_freqs_{};
  size_t num_expected_{0};
  size_t num_received_{0};
  bool lf_mf_follows_{false};
};

template <typename T, size_t N>
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
  size_t pointer_{};
};

template <typename T, size_t N>
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
    return static_cast<float>(sum_) / history_.size();
  }

 private:
  std::array<T, N> history_{};
  T sum_{};
  size_t ptr_{};
};

std::string rtrim(std::string s);

}  // namespace redsea
#endif  // UTIL_H_
