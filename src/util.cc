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
#include "src/util.h"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace redsea {

std::string getHoursMinutesString(int hour, int minute) {
  std::stringstream ss;
  ss << std::setfill('0') <<
        std::setw(2) << hour << ":" <<
        std::setw(2) << minute;
  return ss.str();
}

std::string join(const std::vector<std::string>& strings, const std::string& d) {
  std::string result("");
  for (size_t i = 0; i < strings.size(); i++) {
    result += strings[i];
    if (i < strings.size() - 1)
      result += d;
  }
  return result;
}

std::string join(const std::vector<uint16_t>& nums, const std::string& d) {
  std::string result("");
  for (size_t i = 0; i < nums.size(); i++) {
    result += std::to_string(nums[i]);
    if (i < nums.size() - 1)
      result += d;
  }
  return result;
}

std::string getHexString(uint32_t value, int num_nybbles) {
  std::stringstream ss;

  ss.fill('0');
  ss.setf(std::ios_base::uppercase);

  ss << std::hex << std::setw(num_nybbles) << value;

  return ss.str();
}

std::string getPrefixedHexString(uint32_t value, int num_nybbles) {
  return "0x" + getHexString(value, num_nybbles);
}

// 3.2.1.6
CarrierFrequency::CarrierFrequency(uint16_t code, Band band) :
    code_(code), band_(band) {
}

bool CarrierFrequency::isValid() const {
  return (band_ == Band::LF_MF && code_ >= 1 && code_ <= 135) ||
         (band_ == Band::FM    && code_ >= 1 && code_ <= 204);
}

int CarrierFrequency::kHz() const {
  int khz = 0;
  if (isValid()) {
    switch (band_) {
      case Band::FM :
        khz = 87500 + 100 * code_;
        break;

      case Band::LF_MF :
        if (code_ <= 15)
          khz = 144 + 9 * code_;
        else
          khz = 522 + (9 * (code_ - 15));
        break;
    }
  }

  return khz;
}

std::string CarrierFrequency::str() const {
  std::stringstream ss;
  if (isValid()) {
    switch (band_) {
      case Band::FM : {
        const float num = kHz() / 1000.0f;
        ss.precision(1);
        ss << std::fixed << num << " MHz";
        break;
      }

      case Band::LF_MF : {
        const float num = kHz();
        ss.precision(0);
        ss << std::fixed << num << " kHz";
      }
    }
  } else {
    ss << "N/A";
  }
  return ss.str();
}

bool operator== (const CarrierFrequency &f1,
                 const CarrierFrequency &f2) {
  return (f1.code_ == f2.code_);
}

bool operator< (const CarrierFrequency &f1,
                const CarrierFrequency &f2) {
  return (f1.kHz() < f2.kHz());
}

void AltFreqList::insert(uint16_t af_code) {
  const CarrierFrequency frequency(af_code, lf_mf_follows_ ? CarrierFrequency::Band::LF_MF :
                                                             CarrierFrequency::Band::FM);
  lf_mf_follows_ = false;

  // AF code encodes a frequency
  if (frequency.isValid() && num_expected_ > 0) {
    if (num_received_ < num_expected_) {
      const int kHz = frequency.kHz();
      alt_freqs_[num_received_] = kHz;
      num_received_++;

    // Error; no space left in the list.
    } else {
      clear();
    }

  // Filler
  } else if (af_code == 205) {

  // No AF exists
  } else if (af_code == 224) {

  // Number of AFs
  } else if (af_code >= 225 && af_code <= 249) {
    num_expected_ = af_code - 224;
    num_received_ = 0;

  // AM/LF freq follows
  } else if (af_code == 250) {
    lf_mf_follows_ = true;

  // Error; invalid AF code.
  } else {
    clear();
  }
}

bool AltFreqList::isMethodB() const {
  // Method B has an odd number of elements, at least 3
  if (num_expected_ % 2 != 1 || num_received_ < 3)
    return false;

  // Method B is composed of pairs where one is always the tuned frequency
  const int tuned_frequency = alt_freqs_[0];
  for (size_t i = 1; i < num_received_; i += 2) {
    const int freq1 = alt_freqs_[i];
    const int freq2 = alt_freqs_[i + 1];
    if (freq1 != tuned_frequency && freq2 != tuned_frequency)
      return false;
  }

  return true;
}

bool AltFreqList::isComplete() const {
  return num_expected_ == num_received_ &&
         num_received_ > 0;
}

// Return the sequence of frequencies as they were received (excluding special AF codes)
std::vector<int> AltFreqList::getRawList() const {
  return std::vector<int>(alt_freqs_.begin(), alt_freqs_.begin() + num_received_);
}

void AltFreqList::clear() {
  num_expected_ = num_received_ = 0;
}

std::vector<std::string> splitLine(const std::string& line, char delimiter) {
  std::stringstream ss(line);
  std::vector<std::string> result;

  while (ss.good()) {
    std::string val;
    std::getline(ss, val, delimiter);
    result.push_back(val);
  }

  return result;
}

std::vector<std::vector<std::string>> readCSV(const std::string& filename,
                                              char delimiter) {
  std::vector<std::vector<std::string>> lines;

  std::ifstream in(filename);
  if (!in.is_open())
    return lines;

  for (std::string line; std::getline(in, line); ) {
    if (!in.good())
      break;

    line.erase(std::remove(line.begin(), line.end(), '\r'), line.end());
    line.erase(std::remove(line.begin(), line.end(), '\n'), line.end());

    lines.push_back(splitLine(line, delimiter));
  }

  in.close();

  return lines;
}

CSVTable readCSVWithTitles(const std::string& filename, char delimiter) {
  std::vector<std::string> lines;

  std::ifstream in(filename);
  if (in.is_open()) {
    for (std::string line; std::getline(in, line); ) {
      if (!in.good())
        break;

      lines.push_back(line);
    }

    in.close();
  }

  return readCSVContainerWithTitles(lines, delimiter);
}

std::string rtrim(std::string s) {
  return s.erase(s.find_last_not_of(' ') + 1);
}

std::string get_string(const CSVTable& table, const CSVRow& row, const std::string& title) {
  return row.at(table.titles.at(title));
}

// @throws exceptions from std::stoi
int get_int(const CSVTable& table, const CSVRow& row, const std::string& title) {
  return std::stoi(row.at(table.titles.at(title)));
}

// @throws exceptions from std::stoi
uint16_t get_uint16(const CSVTable& table, const CSVRow& row, const std::string& title) {
  return static_cast<uint16_t>(get_int(table, row, title));
}

bool row_contains(const CSVTable& table, const CSVRow& row, const std::string& title) {
  return !row.at(table.titles.at(title)).empty();
}

}  // namespace redsea
