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
#include "src/util/util.hh"

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <ctime>
#include <string>
#include <vector>

namespace redsea {

// \brief Format hours, minutes as HH:MM
std::string getHoursMinutesString(int hour, int minute) {
  return std::to_string(hour).insert(0, 2 - std::to_string(hour).length(), '0') + ":" +
         std::to_string(minute).insert(0, 2 - std::to_string(minute).length(), '0');
}

// \brief Format a system clock timestamp
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

// \brief Join strings with a delimiter
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
CarrierFrequency::CarrierFrequency(std::uint16_t code, Band band) : code_(code), band_(band) {}

bool CarrierFrequency::isValid() const {
  return (band_ == Band::LF_MF && code_ >= 1 && code_ <= 135) ||
         (band_ == Band::FM && code_ >= 1 && code_ <= 204);
}

// \brief Frequency as integer kilohertz
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

// \brief Frequency as a human-readable string, rounded
std::string CarrierFrequency::str() const {
  if (isValid()) {
    switch (band_) {
      case Band::FM: {
        const int khz = kHz();
        return std::to_string(khz / 1000) + "." + std::to_string((khz % 1000) / 100) + " MHz";
      }

      case Band::LF_MF: {
        return std::to_string(kHz()) + " kHz";
      }
    }
  }
  return "N/A";
}

bool operator==(const CarrierFrequency& f1, const CarrierFrequency& f2) {
  return (f1.code_ == f2.code_);
}

bool operator<(const CarrierFrequency& f1, const CarrierFrequency& f2) {
  return (f1.kHz() < f2.kHz());
}

void AltFreqList::insert(std::uint16_t af_code) {
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

// \return True if the transmitter is probably using alt. frequency method B
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

// \return The sequence of frequencies as they were received (excluding special AF codes)
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
