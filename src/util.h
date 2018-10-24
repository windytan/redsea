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

uint16_t Bits(uint16_t word, int starting_at, int len);

std::string HoursMinutesString(int hour, int minute);

std::string Join(std::vector<std::string> strings, const std::string& d);
std::string Join(std::vector<uint16_t> nums, const std::string& d);

std::string HexString(int value, int num_nybbles);

class CSVRow {
 public:
  CSVRow(const std::map<std::string, int>& titles,
         const std::vector<std::string>& values);
  std::string at(std::string title) const;

 private:
  std::map<std::string, int> titles_;
  std::vector<std::string> values_;
};

std::vector<std::string> splitline(std::string line, char delimiter);
std::vector<std::vector<std::string>> ReadCSV(std::vector<std::string> csvdata,
                                              char delimiter);
std::vector<std::vector<std::string>> ReadCSV(std::string filename,
                                              char delimiter);
std::vector<CSVRow> ReadCSVWithTitles(std::string filename, char delimiter);
std::vector<CSVRow> ReadCSVWithTitles(std::vector<std::string> csvdata,
                                      char delimiter);

const bool kFrequencyIsLFMF = true;

class CarrierFrequency {
 public:
  explicit CarrierFrequency(uint16_t code, bool is_lf_mf = false);
  bool valid() const;
  int kHz() const;
  std::string str() const;
  friend bool operator== (const CarrierFrequency &f1,
                          const CarrierFrequency &f2);
  friend bool operator< (const CarrierFrequency &f1,
                         const CarrierFrequency &f2);

 private:
  uint16_t code_;
  bool is_lf_mf_;
};

class AltFreqList {
 public:
  AltFreqList();
  void insert(uint8_t af_code);
  bool complete() const;
  std::set<CarrierFrequency> get() const;
  void clear();

 private:
  std::set<CarrierFrequency> alt_freqs_;
  unsigned num_alt_freqs_ { 0 };
  bool lf_mf_follows_     { false };
};

template<size_t N>
class RunningSum {
 public:
  RunningSum() {};
  int sum() const {
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
  std::array<int, N> history_;
  size_t pointer_ { 0 };
};

template<size_t N>
class RunningAverage {
 public:
  RunningAverage() {};
  void push(int value) {
    sum_ -= history_[ptr_];
    history_[ptr_] = value;
    sum_ += value;
    ptr_ = (ptr_ + 1) % history_.size();
  }

  float average() const {
    return 1.0f * sum_ / history_.size();
  }

 private:
  std::array<int, N> history_;
  int    sum_ { 0 };
  size_t ptr_ { 0 };
};

std::string rtrim(std::string s);

}  // namespace redsea
#endif  // UTIL_H_
