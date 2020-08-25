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

std::string join(std::vector<std::string> strings, const std::string& d) {
  std::string result("");
  for (size_t i = 0; i < strings.size(); i++) {
    result += strings[i];
    if (i < strings.size() - 1)
      result += d;
  }
  return result;
}

std::string join(std::vector<uint16_t> nums, const std::string& d) {
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
        float num = kHz() / 1000.0f;
        ss.precision(1);
        ss << std::fixed << num << " MHz";
        break;
      }

      case Band::LF_MF : {
        float num = kHz();
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
  return (f1.kHz() == f2.kHz());
}

bool operator< (const CarrierFrequency &f1,
                const CarrierFrequency &f2) {
  return (f1.kHz() < f2.kHz());
}

void AltFreqList::insert(uint16_t af_code) {
  CarrierFrequency frequency(af_code, lf_mf_follows_ ? CarrierFrequency::Band::LF_MF :
                                                       CarrierFrequency::Band::FM);
  lf_mf_follows_ = false;

  if (frequency.isValid()) {
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

bool AltFreqList::isComplete() const {
  return (alt_freqs_.size() == num_alt_freqs_ &&
          num_alt_freqs_ > 0);
}

std::set<CarrierFrequency> AltFreqList::get() const {
  return alt_freqs_;
}

void AltFreqList::clear() {
  alt_freqs_.clear();
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

std::vector<std::vector<std::string>> readCSV(std::vector<std::string> csvdata,
                                              char delimiter) {
  std::vector<std::vector<std::string>> lines;

  std::transform(csvdata.cbegin(), csvdata.cend(), std::back_inserter(lines),
                 [&](const std::string& line) {
                   return splitLine(line, delimiter);
                 });

  return lines;
}

std::vector<std::vector<std::string>> readCSV(std::string filename,
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

    std::vector<std::string> fields = splitLine(line, delimiter);
    lines.push_back(fields);
  }

  in.close();

  return lines;
}

CSVTable readCSVWithTitles(std::vector<std::string> csvdata,
                           char delimiter) {
  CSVTable table;

  bool is_title_row = true;

  for (std::string line : csvdata) {
    line.erase(std::remove(line.begin(), line.end(), '\r'), line.end());
    line.erase(std::remove(line.begin(), line.end(), '\n'), line.end());

    std::vector<std::string> fields = splitLine(line, delimiter);
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

CSVTable readCSVWithTitles(std::string filename, char delimiter) {
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

  return readCSVWithTitles(lines, delimiter);
}

std::string rtrim(std::string s) {
  return s.erase(s.find_last_not_of(' ') + 1);
}

std::string get_string(const CSVTable& table, const CSVRow& row, const std::string& title) {
  return row.at(table.titles.at(title));
}

int get_int(const CSVTable& table, const CSVRow& row, const std::string& title) {
  return std::stoi(row.at(table.titles.at(title)));
}

uint16_t get_uint16(const CSVTable& table, const CSVRow& row, const std::string& title) {
  return uint16_t(get_int(table, row, title));
}

bool row_contains(const CSVTable& table, const CSVRow& row, const std::string& title) {
  return !row.at(table.titles.at(title)).empty();
}

}  // namespace redsea
