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

#include <cstdint>

#include <algorithm>
#include <map>
#include <numeric>
#include <set>
#include <string>
#include <vector>

namespace redsea {

// extract N bits from word, starting at starting_at from the right
template<size_t N>
uint16_t Bits(uint16_t word, size_t starting_at) {
  static_assert(N <= 16, "");
  return (word >> starting_at) & ((1 << N) - 1);
}

// extract N bits from the concatenation of word1 and word2, starting at
// starting_at from the right
template<size_t N>
uint32_t Bits(uint16_t word1, uint16_t word2, size_t starting_at) {
  static_assert(N <= 32, "");
  return (((word1 << 16) + word2) >> starting_at) & ((1 << N) - 1);
}

std::string HoursMinutesString(int hour, int minute);

std::string Join(std::vector<std::string> strings, const std::string& d);
std::string Join(std::vector<uint16_t> nums, const std::string& d);

std::string HexString(uint32_t value, int num_nybbles);

using CSVRow = std::vector<std::string>;

class CSVTable {
 public:
  std::map<std::string, size_t> titles;
  std::vector<CSVRow> rows;
};

std::vector<std::string> splitline(std::string line, char delimiter);
std::vector<std::vector<std::string>> ReadCSV(std::vector<std::string> csvdata,
                                              char delimiter);
std::vector<std::vector<std::string>> ReadCSV(std::string filename,
                                              char delimiter);
CSVTable ReadCSVWithTitles(std::string filename, char delimiter);
CSVTable ReadCSVWithTitles(std::vector<std::string> csvdata,
                                      char delimiter);

class CarrierFrequency {
 public:
  enum class Band {
    LF_MF, FM
  };
 public:
  explicit CarrierFrequency(uint16_t code, Band band = Band::FM);
  bool valid() const;
  int kHz() const;
  std::string str() const;
  friend bool operator== (const CarrierFrequency &f1,
                          const CarrierFrequency &f2);
  friend bool operator< (const CarrierFrequency &f1,
                         const CarrierFrequency &f2);

 private:
  uint16_t code_;
  Band band_ { Band::FM };
};

class AltFreqList {
 public:
  AltFreqList();
  void insert(uint16_t af_code);
  bool complete() const;
  std::set<CarrierFrequency> get() const;
  void clear();

 private:
  std::set<CarrierFrequency> alt_freqs_;
  unsigned num_alt_freqs_ { 0 };
  bool lf_mf_follows_     { false };
};

template<typename T, size_t N>
class RunningSum {
 public:
  RunningSum() = default;
  T sum() const {
    return std::accumulate(history_.cbegin(), history_.cend(), 0);
  }
  void push(int number) {
    history_[pointer_] = number;
    pointer_ = (pointer_ + 1) % history_.size();
  }
  void clear() {
    std::fill(history_.begin(), history_.end(), 0);
  }

 private:
  std::array<T, N> history_ {};
  size_t pointer_ { 0 };
};

template<typename T, size_t N>
class RunningAverage {
 public:
  RunningAverage() = default;
  void push(T value) {
    sum_ -= history_[ptr_];
    history_[ptr_] = value;
    sum_ += value;
    ptr_ = (ptr_ + 1) % history_.size();
  }

  float average() const {
    return 1.0f * sum_ / history_.size();
  }

 private:
  std::array<T, N> history_ {};
  T      sum_ {};
  size_t ptr_ { 0 };
};

std::string rtrim(std::string s);

std::string get_string(const CSVTable& table, const CSVRow& row, const std::string& title);
int get_int(const CSVTable& table, const CSVRow& row, const std::string& title);
uint16_t get_uint16(const CSVTable& table, const CSVRow& row, const std::string& title);
bool row_contains(const CSVTable& table, const CSVRow& row, const std::string& title);

}  // namespace redsea
#endif  // UTIL_H_
