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

std::string HoursMinutesString(int hour, int minute) {
  std::stringstream ss;
  ss << std::setfill('0') <<
        std::setw(2) << hour << ":" <<
        std::setw(2) << minute;
  return ss.str();
}

std::string Join(std::vector<std::string> strings, const std::string& d) {
  std::string result("");
  for (size_t i = 0; i < strings.size(); i++) {
    result += strings[i];
    if (i < strings.size() - 1)
      result += d;
  }
  return result;
}

std::string Join(std::vector<uint16_t> nums, const std::string& d) {
  std::string result("");
  for (size_t i = 0; i < nums.size(); i++) {
    result += std::to_string(nums[i]);
    if (i < nums.size() - 1)
      result += d;
  }
  return result;
}

std::string HexString(int value, int num_nybbles) {
  std::stringstream ss;

  ss.fill('0');
  ss.setf(std::ios_base::uppercase);

  ss << std::hex << std::setw(num_nybbles) << value;

  return ss.str();
}

// 3.2.1.6
CarrierFrequency::CarrierFrequency(uint16_t code, bool is_lf_mf) :
    code_(code), is_lf_mf_(is_lf_mf) {
}

bool CarrierFrequency::valid() const {
  return ((is_lf_mf_ && code_ >= 1 && code_ <= 135) ||
         (!is_lf_mf_ && code_ >= 1 && code_ <= 204));
}

int CarrierFrequency::kHz() const {
  int khz = 0;
  if (valid()) {
    if (!is_lf_mf_)
      khz = 87500 + 100 * code_;
    else if (code_ <= 15)
      khz = 144 + 9 * code_;
    else
      khz = 522 + (9 * (code_ - 15));
  }

  return khz;
}

std::string CarrierFrequency::str() const {
  std::stringstream ss;
  if (valid()) {
    float num = (is_lf_mf_ ? kHz() : kHz() / 1000.0f);
    ss.precision(is_lf_mf_ ? 0 : 1);
    ss << std::fixed << num << (is_lf_mf_ ? " kHz" : " MHz");
  } else {
    ss << "N/A";
  }
  return ss.str();
}

bool operator== (const CarrierFrequency &f1,
                 const CarrierFrequency &f2) {
  return (f1.kHz() == f2.kHz());
}

bool operator< (const CarrierFrequency &f1,
                const CarrierFrequency &f2) {
  return (f1.kHz() < f2.kHz());
}

AltFreqList::AltFreqList() {}

void AltFreqList::insert(uint8_t af_code) {
  CarrierFrequency frequency(af_code, lf_mf_follows_);
  lf_mf_follows_ = false;

  if (frequency.valid()) {
    alt_freqs_.insert(frequency);
  } else if (af_code == 205) {
    // filler
  } else if (af_code == 224) {
    // no AF exists
  } else if (af_code >= 225 && af_code <= 249) {
    num_alt_freqs_ = af_code - 224;
  } else if (af_code == 250) {
    // AM/LF freq follows
    lf_mf_follows_ = true;
  }
}

bool AltFreqList::complete() const {
  return (alt_freqs_.size() == num_alt_freqs_ &&
          num_alt_freqs_ > 0);
}

std::set<CarrierFrequency> AltFreqList::get() const {
  return alt_freqs_;
}

void AltFreqList::clear() {
  alt_freqs_.clear();
}

std::vector<std::string> SplitLine(const std::string& line, char delimiter) {
  std::stringstream ss(line);
  std::vector<std::string> result;

  while (ss.good()) {
    std::string val;
    std::getline(ss, val, delimiter);
    result.push_back(val);
  }

  return result;
}

std::vector<std::vector<std::string>> ReadCSV(std::vector<std::string> csvdata,
                                              char delimiter) {
  std::vector<std::vector<std::string>> lines;

  std::transform(csvdata.cbegin(), csvdata.cend(), std::back_inserter(lines),
                 [&](const std::string& line) {
                   return SplitLine(line, delimiter);
                 });

  return lines;
}

std::vector<std::vector<std::string>> ReadCSV(std::string filename,
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

    std::vector<std::string> fields = SplitLine(line, delimiter);
    lines.push_back(fields);
  }

  in.close();

  return lines;
}

CSVTable ReadCSVWithTitles(std::vector<std::string> csvdata,
                           char delimiter) {
  CSVTable table;

  bool is_title_row = true;

  for (std::string line : csvdata) {
    line.erase(std::remove(line.begin(), line.end(), '\r'), line.end());
    line.erase(std::remove(line.begin(), line.end(), '\n'), line.end());

    std::vector<std::string> fields = SplitLine(line, delimiter);
    if (is_title_row) {
      for (size_t i = 0; i < fields.size(); i++)
        table.titles[fields[i]] = i;
      is_title_row = false;
    } else {
      if (fields.size() <= table.titles.size())
        table.rows.push_back(fields);
    }
  }

  return table;
}

CSVTable ReadCSVWithTitles(std::string filename, char delimiter) {
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

  return ReadCSVWithTitles(lines, delimiter);
}

std::string rtrim(std::string s) {
  return s.erase(s.find_last_not_of(' ') + 1);
}

}  // namespace redsea
