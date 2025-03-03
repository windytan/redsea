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

#include <array>
#include <iomanip>
#include <sstream>

namespace redsea {

std::string getHoursMinutesString(int hour, int minute) {
  std::stringstream ss;
  ss << std::setfill('0') << std::setw(2) << hour << ":" << std::setw(2) << minute;
  return ss.str();
}

// Used in RDS2 RFT
uint32_t crc16_ccitt(const uint8_t* data, size_t length) {
  uint32_t crc = 0xFFFF;

  for (size_t i = 0; i < length; ++i) {
    crc = static_cast<uint8_t>(crc >> 8) | (crc << 8);
    crc ^= data[i];
    crc ^= static_cast<uint8_t>(crc & 0xFF) >> 4;
    crc ^= (crc << 8) << 4;
    crc ^= ((crc & 0xFF) << 4) << 1;
  }
  return (crc ^ 0xFFFF) & 0xFFFF;
}

std::string getTimePointString(const std::chrono::time_point<std::chrono::system_clock>& timepoint,
                               const std::string& format) {
  // This is done to ensure we get truncation and not rounding to integer seconds
  const auto seconds_since_epoch(
      std::chrono::duration_cast<std::chrono::seconds>(timepoint.time_since_epoch()));
  const std::time_t t = std::chrono::system_clock::to_time_t(
      std::chrono::system_clock::time_point(seconds_since_epoch));

  std::string format_with_fractional(format);
  const std::size_t found = format_with_fractional.find("%f");
  if (found != std::string::npos) {
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                        timepoint.time_since_epoch() - seconds_since_epoch)
                        .count();
    const auto hundredths = (ms / 10) % 10;
    const auto tenths     = (ms / 100) % 10;

    format_with_fractional.replace(found, 2, std::to_string(tenths) + std::to_string(hundredths));
  }

  std::array<char, 64> buffer{};
  if (std::strftime(buffer.data(), buffer.size(), format_with_fractional.c_str(),
                    std::localtime(&t)) == 0) {
    return "(format error)";
  }

  return {buffer.data()};
}

std::string join(const std::vector<std::string>& strings, const std::string& d) {
  if (strings.empty())
    return "";

  std::string result;
  for (size_t i = 0; i < strings.size(); i++) {
    result += strings[i];
    if (i < strings.size() - 1)
      result += d;
  }
  return result;
}

// 3.2.1.6
CarrierFrequency::CarrierFrequency(uint16_t code, Band band) : code_(code), band_(band) {}

bool CarrierFrequency::isValid() const {
  return (band_ == Band::LF_MF && code_ >= 1 && code_ <= 135) ||
         (band_ == Band::FM && code_ >= 1 && code_ <= 204);
}

int CarrierFrequency::kHz() const {
  int khz = 0;
  if (isValid()) {
    switch (band_) {
      case Band::FM: khz = 87500 + 100 * code_; break;

      case Band::LF_MF:
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
      case Band::FM: {
        ss.precision(1);
        ss << std::fixed << (static_cast<float>(kHz()) * 1e-3f) << " MHz";
        break;
      }

      case Band::LF_MF: {
        ss << kHz() << " kHz";
      }
    }
  } else {
    ss << "N/A";
  }
  return ss.str();
}

bool operator==(const CarrierFrequency& f1, const CarrierFrequency& f2) {
  return (f1.code_ == f2.code_);
}

bool operator<(const CarrierFrequency& f1, const CarrierFrequency& f2) {
  return (f1.kHz() < f2.kHz());
}

void AltFreqList::insert(uint16_t af_code) {
  const CarrierFrequency frequency(
      af_code, lf_mf_follows_ ? CarrierFrequency::Band::LF_MF : CarrierFrequency::Band::FM);
  lf_mf_follows_ = false;

  // AF code encodes a frequency
  if (frequency.isValid() && num_expected_ > 0) {
    if (num_received_ < num_expected_) {
      const int kHz             = frequency.kHz();
      alt_freqs_[num_received_] = kHz;
      num_received_++;

    } else {
      // Error; no space left in the list.
      clear();
    }

  } else if (af_code == 205 ||  // Filler
             af_code == 224) {  // No AF exists

  } else if (af_code >= 225 && af_code <= 249) {
    // Number of AFs
    num_expected_ = af_code - 224;
    num_received_ = 0;
  } else if (af_code == 250) {
    // AM/LF frequency follows
    lf_mf_follows_ = true;
  } else {
    // Error; invalid AF code.
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
  return num_expected_ == num_received_ && num_received_ > 0;
}

// Return the sequence of frequencies as they were received (excluding special AF codes)
std::vector<int> AltFreqList::getRawList() const {
  return {alt_freqs_.begin(), alt_freqs_.begin() + num_received_};
}

void AltFreqList::clear() {
  num_expected_ = num_received_ = 0;
}

std::string rtrim(std::string s) {
  return s.erase(s.find_last_not_of(' ') + 1);
}

}  // namespace redsea
