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
#include "src/station.hh"

#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <exception>
#include <initializer_list>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "src/io/output.hh"
#include "src/options.hh"
#include "src/tables.hh"
#include "src/text/radiotext.hh"
#include "src/text/rdsstring.hh"
#include "src/util/tree.hh"
#include "src/util/util.hh"

namespace redsea {

namespace {

// Programme Item Number (IEC 62106:2015, section 6.1.5.2)
// TODO: Removed in RDS2
bool decodePIN(std::uint16_t pin, ObjectTree& out) {
  const std::uint16_t day    = getBits<5>(pin, 11);
  const std::uint16_t hour   = getBits<5>(pin, 6);
  const std::uint16_t minute = getBits<6>(pin, 0);
  if (day >= 1 && hour <= 24 && minute <= 59) {
    out["prog_item_number"]          = pin;
    out["prog_item_started"]["day"]  = day;
    out["prog_item_started"]["time"] = getHoursMinutesString(hour, minute);
    return true;
  } else {
    return false;
  }
}

}  // namespace

/*
 * A Station represents a single broadcast carrier identified by its RDS PI
 * code.
 *
 * @param which_channel Which PCM channel the station is on
 */
Station::Station(const Options& options, int which_channel)
    : options_(options), which_channel_(which_channel), tmc_(options) {}

Station::Station(const Options& options, int which_channel, std::uint16_t pi)
    : Station(options, which_channel) {
  // A delegating constructor can't have other mem-initializers
  // NOLINTBEGIN(cppcoreguidelines-prefer-member-initializer)
  pi_     = pi;
  has_pi_ = true;
  // NOLINTEND(cppcoreguidelines-prefer-member-initializer)
}

// @param stream The stream to print to (not to be confused with RDS2 data streams)
void Station::updateAndPrint(const Group& group, std::ostream& stream) {
  if (!has_pi_) {
    return;
  }

  ObjectTree out;

  if (options_.streams) {
    out["stream"] = group.getDataStream();
  }

  if (group.getType().value.version != GroupType::Version::C) {
    // Allow 1 group with missed PI. For subsequent misses, don't process at all.
    if (group.hasPI()) {
      last_group_had_pi_ = true;
    } else if (last_group_had_pi_) {
      last_group_had_pi_ = false;
    } else {
      return;
    }

    if (group.isEmpty()) {
      return;
    }

    out["pi"] = getPrefixedHexString<4>(getPI());
    if (options_.rbds) {
      const std::string callsign{getCallsignFromPI(getPI())};
      if (!callsign.empty()) {
        if ((getPI() & 0xF000U) == 0x1000U) {
          out["callsign_uncertain"] = callsign;
        } else {
          out["callsign"] = callsign;
        }
      }
    }
  }  // if not C

  if (options_.timestamp) {
    out["rx_time"] = getTimePointString(group.getRxTime().value, options_.time_format);
  }

  if (group.getBLER().has_value) {
    out["bler"] = std::lround(group.getBLER().value);
  }

  if (options_.num_channels > 1) {
    out["channel"] = which_channel_;
  }

  if (options_.show_raw) {
    out["raw_data"] = group.asHex();
  }

  decodeBasics(group, out);

  if (group.getType().has_value) {
    const GroupType& type = group.getType().value;

    if (type.version == GroupType::Version::C) {
      // Any Version C group (RDS2 streams only)
      decodeC(group, out);

    } else if (type.number == 0) {
      // Basic tuning and switching information, PS name
      decodeType0(group, out);
    } else if (type.number == 1) {
      // Slow labelling codes
      decodeType1(group, out);
    } else if (type.number == 2) {
      // RadioText
      decodeType2(group, out);
    } else if (type.number == 3 && type.version == GroupType::Version::A) {
      // ODA identification
      decodeType3A(group, out);
    } else if (type.number == 4 && type.version == GroupType::Version::A) {
      // Clock-time and date
      decodeType4A(group, out);
    } else if (type.number == 10 && type.version == GroupType::Version::A) {
      // PTY name
      decodeType10A(group, out);
    } else if (type.number == 14) {
      // EON
      decodeType14(group, out);
    } else if (type.number == 15 && type.version == GroupType::Version::B) {
      // Fast switching information
      decodeType15B(group, out);

      // Other groups can be reassigned for ODA by a 3A group
    } else if (oda_app_for_group_.find(type) != oda_app_for_group_.end()) {
      decodeODAGroup(group, out);

      // Below: Groups that could optionally be used for ODA but have
      // another primary function
    } else if (type.number == 5) {
      // Transparent Data (pre-2021)
      decodeType5(group, out);
    } else if (type.number == 6) {
      // In-house Applications (pre-2021)
      decodeType6(group, out);
    } else if (type.number == 8 && type.version == GroupType::Version::A) {
      if (group.has(BLOCK2) && group.has(BLOCK3) && group.has(BLOCK4))
        tmc_.receiveUserGroup(getBits<5>(group.get(BLOCK2), 0), group.get(BLOCK3),
                              group.get(BLOCK4), out);
    } else if (type.number == 9 && type.version == GroupType::Version::A) {
      // Emergency Warning System (pre-2021)
      decodeType9A(group, out);

    } else if (type.number == 15 && type.version == GroupType::Version::A) {
      // Long PS (RDS2)
      decodeType15A(group, out);

    } else {
      // ODA-only groups
      // 3B, 4B, 7A, 7B, 8B, 9B, 10B, 11A, 11B, 12A, 12B, 13B
      decodeODAGroup(group, out);
    }
  }

  if (options_.time_from_start && group.getTimeFromStart().has_value) {
    out["time_from_start"] = group.getTimeFromStart().value;
  }

  printAsJson(out, stream);
}

std::uint16_t Station::getPI() const {
  return pi_;
}

// Decode basic information common to (almost) all groups
void Station::decodeBasics(const Group& group, ObjectTree& out) {
  if (group.getType().value.version == GroupType::Version::C) {
    out["group"] = "C";
  } else if (group.has(BLOCK2)) {
    const std::uint16_t pty = getBits<5>(group.get(BLOCK2), 5);

    if (group.getType().has_value)
      out["group"] = group.getType().value.str();

    out["tp"] = getBool(group.get(BLOCK2), 10);

    out["prog_type"] = options_.rbds ? getPTYNameStringRBDS(pty) : getPTYNameString(pty);
  } else if (group.getType().value.number == 15 &&
             group.getType().value.version == GroupType::Version::B && group.has(BLOCK4)) {
    const std::uint16_t pty = getBits<5>(group.get(BLOCK4), 5);

    out["group"] = group.getType().value.str();

    out["tp"]        = getBool(group.get(BLOCK4), 10);
    out["prog_type"] = options_.rbds ? getPTYNameStringRBDS(pty) : getPTYNameString(pty);
  }
}

// Group 0: Basic tuning and switching information
void Station::decodeType0(const Group& group, ObjectTree& out) {
  // Block 2: Flags
  const std::uint16_t segment_address         = getBits<2>(group.get(BLOCK2), 0);
  const bool is_di                            = getBool(group.get(BLOCK2), 2);
  out["di"][getDICodeString(segment_address)] = is_di;
  out["ta"]                                   = getBool(group.get(BLOCK2), 4);
  out["is_music"]                             = getBool(group.get(BLOCK2), 3);

  if (!group.has(BLOCK3)) {
    // Reset a Method B list to prevent mixing up different lists
    if (alt_freq_list_.isMethodB())
      alt_freq_list_.clear();
    return;
  }

  // Block 3: Alternative frequencies
  if (group.getType().value.version == GroupType::Version::A) {
    alt_freq_list_.insert(getUint8(group.get(BLOCK3), 8));
    alt_freq_list_.insert(getUint8(group.get(BLOCK3), 0));

    if (alt_freq_list_.isComplete()) {
      const auto raw_frequencies = alt_freq_list_.getRawList();

      // AF Method B sends longer lists with possible regional variants
      if (alt_freq_list_.isMethodB()) {
        const int tuned_frequency = raw_frequencies[0];

        // We use std::sets for detecting duplicates
        std::set<int> unique_alternative_frequencies;
        std::set<int> unique_regional_variants;
        std::vector<int> alternative_frequencies;
        std::vector<int> regional_variants;

        // Frequency pairs
        for (size_t i = 1; i < raw_frequencies.size(); i += 2) {
          const int freq1 = raw_frequencies[i];

          // No buffer over-read; checked by isMethodB()
          assert(i + 1 < raw_frequencies.size());
          const int freq2 = raw_frequencies[i + 1];

          const int non_tuned_frequency = (freq1 == tuned_frequency ? freq2 : freq1);

          if (freq1 < freq2) {
            // "General case"
            alternative_frequencies.push_back(non_tuned_frequency);
            unique_alternative_frequencies.insert(non_tuned_frequency);

          } else {
            // "Special case": Non-tuned frequency is a regional variant
            regional_variants.push_back(non_tuned_frequency);
            unique_regional_variants.insert(non_tuned_frequency);
          }
        }

        // In noisy conditions we may miss a lot of 0A groups. This check catches
        // the case where there's multiple copies of some frequencies.
        const std::size_t expected_number_of_afs = raw_frequencies.size() / 2;
        const std::size_t number_of_unique_afs =
            unique_alternative_frequencies.size() + unique_regional_variants.size();
        if (number_of_unique_afs == expected_number_of_afs) {
          out["alt_frequencies_b"]["tuned_frequency"] = tuned_frequency;

          for (const int frequency : alternative_frequencies)
            out["alt_frequencies_b"]["same_programme"].push_back(frequency);

          for (const int frequency : regional_variants)
            out["alt_frequencies_b"]["regional_variants"].push_back(frequency);
        }
      } else {
        // AF Method A is a simple list
        for (const int frequency : raw_frequencies) out["alt_frequencies_a"].push_back(frequency);
      }

      alt_freq_list_.clear();

    } else if (options_.show_partial) {
      // If partial list is requested we'll print the raw list and not attempt to
      // deduce whether it's Method A or B
      for (const int f : alt_freq_list_.getRawList()) out["partial_alt_frequencies"].push_back(f);
    }
  }

  if (!group.has(BLOCK4))
    return;

  // Block 4: Program Service Name
  ps_.update(segment_address * 2, getUint8(group.get(BLOCK4), 8), getUint8(group.get(BLOCK4), 0));

  if (ps_.text.isComplete()) {
    out["ps"] = ps_.text.getLastCompleteString();
  } else if (options_.show_partial) {
    try {
      out["partial_ps"] = ps_.text.str();
    } catch (const std::exception& e) {
      out["debug"].push_back(e.what());
      return;
    }
  }
}

// Group 1: Programme Item Number and slow labelling codes
void Station::decodeType1(const Group& group, ObjectTree& out) {
  if (!(group.has(BLOCK3) && group.has(BLOCK4)))
    return;

  // TODO: Removed in RDS2 -> now RFU
  pin_ = group.get(BLOCK4);

  if (pin_ != 0x0000 && !decodePIN(pin_, out)) {
    out["debug"].push_back("invalid PIN");
  }

  if (group.getType().value.version == GroupType::Version::A) {
    // TODO: Removed in RDS2 -> now RFU
    linkage_la_        = getBool(group.get(BLOCK3), 15);
    out["has_linkage"] = linkage_la_;

    const int slow_label_variant = getBits<3>(group.get(BLOCK3), 12);

    switch (slow_label_variant) {
      case 0:
        ecc_ = getUint8(group.get(BLOCK3), 0);
        cc_  = getBits<4>(pi_, 12);

        if (ecc_ != 0x00) {
          has_country_   = true;
          out["country"] = getCountryString(cc_, ecc_);
        }
        break;

      // TODO: Removed in RDS2 -> now RFU
      case 1:
        tmc_id_       = getBits<12>(group.get(BLOCK3), 0);
        out["tmc_id"] = tmc_id_;
        break;

      case 2:
        // Pager is deprecated
        break;

      // TODO: Removed in RDS2 -> now RFU
      case 3:
        out["language"] = getLanguageString(getUint8(group.get(BLOCK3), 0));
        break;

        // SLC variants 4, 5 are not assigned

      case 6:
        out["slc_broadcaster_bits"] = "0x" + getHexString<3>(getBits<11>(group.get(BLOCK3), 0));
        break;

      case 7: out["ews"] = getBits<12>(group.get(BLOCK3), 0); break;

      default:
        out["debug"].push_back("TODO: SLC variant " + std::to_string(slow_label_variant));
        break;
    }
  }
}

// Group 2: RadioText
// Regarding the length of the message, at least three different practices are seen in the wild:
//   Case (1): The end of the message is marked with a string terminator (0x0D). It's simple to
//             convert this to a string.
//   Case (2): The message is always 64 characters long, and is padded with blank spaces. Simple
//             to decode, and we can remove the spaces.
//   Case (3): There is no string terminator and the message is of random length. Harder to decode
//             reliably in noisy conditions.
// See https://github.com/windytan/redsea/wiki/Some-RadioText-research
void Station::decodeType2(const Group& group, ObjectTree& out) {
  if (!(group.has(BLOCK3) && group.has(BLOCK4)))
    return;

  const size_t radiotext_position =
      getBits<4>(group.get(BLOCK2), 0) *
      (group.getType().value.version == GroupType::Version::A ? 4UL : 2UL);

  const bool ab            = getBool(group.get(BLOCK2), 4);
  const bool is_ab_changed = radiotext_.isABChanged(ab);

  if (options_.show_partial) {
    out["rt_ab"] = ab ? "B" : "A";
  }

  // If these heuristics match it's possible that we just received a full random-length message
  // with no string terminator (method 3 above).
  std::string potentially_complete_message;
  bool has_potentially_complete_message =
      radiotext_position == 0 && radiotext_.text.getReceivedLength() > 1 &&
      not radiotext_.text.isComplete() && not radiotext_.text.hasPreviouslyReceivedTerminators();

  if (has_potentially_complete_message) {
    try {
      potentially_complete_message = rtrim(radiotext_.text.str());
    } catch (const std::exception& e) {
      out["debug"].push_back(e.what());
      return;
    }

    // No, perhaps we just lost the terminator in noise [could we use the actual BLER figure?],
    // or maybe the message got interrupted by an A/B change. Let's wait for a repeat.
    if (potentially_complete_message != radiotext_.previous_potentially_complete_message) {
      has_potentially_complete_message = false;
    }
    radiotext_.previous_potentially_complete_message = potentially_complete_message;
  }

  // The transmitter requests us to clear the buffer (message contents will change).
  // Note: This is sometimes overused in the wild.
  if (is_ab_changed) {
    radiotext_.text.clear();
  }

  if (group.getType().value.version == GroupType::Version::A) {
    radiotext_.text.resize(64);
    radiotext_.update(radiotext_position, getUint8(group.get(BLOCK3), 8),
                      getUint8(group.get(BLOCK3), 0));
  } else {
    radiotext_.text.resize(32);
  }

  if (group.has(BLOCK4)) {
    radiotext_.update(
        radiotext_position + (group.getType().value.version == GroupType::Version::A ? 2 : 0),
        getUint8(group.get(BLOCK4), 8), getUint8(group.get(BLOCK4), 0));
  }

  // Transmitter used Method 1 or 2 convey the length of the string.
  if (radiotext_.text.isComplete()) {
    out["radiotext"] = rtrim(radiotext_.text.getLastCompleteString());

    // Method 3 was used instead (and was confirmed by a repeat).
  } else if (has_potentially_complete_message) {
    out["radiotext"] = rtrim(std::move(potentially_complete_message));

    // The string is not complete yet, but user wants to see it anyway.
  } else if (options_.show_partial) {
    try {
      if (rtrim(radiotext_.text.str()).length() > 0) {
        out["partial_radiotext"] = radiotext_.text.str();
      }
    } catch (const std::exception& e) {
      out["debug"].push_back(e.what());
      return;
    }
  }
}

// Group 3A: Application identification for Open Data
void Station::decodeType3A(const Group& group, ObjectTree& out) {
  if (!(group.has(BLOCK3) && group.has(BLOCK4)))
    return;

  if (group.getType().value.version != GroupType::Version::A)
    return;

  const GroupType oda_group_type(getBits<5>(group.get(BLOCK2), 0));
  const std::uint16_t oda_message{group.get(BLOCK3)};
  const std::uint16_t oda_app_id{group.get(BLOCK4)};

  oda_app_for_group_[oda_group_type] = oda_app_id;

  out["open_data_app"]["oda_group"] = oda_group_type.str();
  out["open_data_app"]["app_name"]  = getAppNameString(oda_app_id);

  switch (oda_app_id) {
    // DAB cross-referencing
    case 0x0093:
      // Message bits are not used
      break;

    // RT+
    case 0x4BD7:
      radiotext_.plus.exists       = true;
      radiotext_.plus.cb           = getBool(oda_message, 12);
      radiotext_.plus.scb          = getBits<4>(oda_message, 8);
      radiotext_.plus.template_num = getUint8(oda_message, 0);
      break;

    // RT+ for Enhanced RadioText
    case 0x4BD8:
      ert_.plus.exists       = true;
      ert_.plus.cb           = getBool(oda_message, 12);
      ert_.plus.scb          = getBits<4>(oda_message, 8);
      ert_.plus.template_num = getUint8(oda_message, 0);
      break;

    // Enhanced RadioText (eRT)
    case 0x6552:
      ert_.text.setEncoding(getBool(oda_message, 0) ? RDSString::Encoding::UTF8
                                                    : RDSString::Encoding::UCS2);
      ert_.text.setDirection(getBool(oda_message, 1) ? RDSString::Direction::RTL
                                                     : RDSString::Direction::LTR);
      ert_uses_chartable_e3_ = getBits<4>(oda_message, 2) == 0;
      break;

    // RDS-TMC
    case 0xCD46:
    case 0xCD47: tmc_.receiveSystemGroup(oda_message, out); break;

    default:
      out["debug"].push_back("TODO: Unimplemented ODA app " + getHexString<4>(oda_app_id));
      out["open_data_app"]["message"] = oda_message;
      break;
  }
}

// Group 4A: Clock-time and date
void Station::decodeType4A(const Group& group, ObjectTree& out) {
  if (!(group.has(BLOCK3) && group.has(BLOCK4)))
    return;

  const double modified_julian_date{
      static_cast<double>(getBits<17>(group.get(BLOCK2), group.get(BLOCK3), 1))};

  // Would result in negative years/months
  if (modified_julian_date < 15079.0) {
    out["debug"].push_back("invalid date/time");
    return;
  }

  int year_utc = static_cast<int>((modified_julian_date - 15078.2) / 365.25);
  int month_utc =
      static_cast<int>((modified_julian_date - 14956.1 - std::trunc(year_utc * 365.25)) / 30.6001);
  const int day_utc =
      static_cast<int>(modified_julian_date - 14956.0 - std::trunc(year_utc * 365.25) -
                       std::trunc(month_utc * 30.6001));
  if (month_utc == 14 || month_utc == 15) {
    year_utc += 1;
    month_utc -= 12;
  }
  year_utc += 1900;
  month_utc -= 1;

  const auto hour_utc   = getBits<5>(group.get(BLOCK3), group.get(BLOCK4), 12);
  const auto minute_utc = getBits<6>(group.get(BLOCK4), 6);

  const double local_offset =
      (getBool(group.get(BLOCK4), 5) ? -1.0 : 1.0) * getBits<5>(group.get(BLOCK4), 0) / 2.0;

  std::tm utc_plus_offset_tm{};
  utc_plus_offset_tm.tm_year  = year_utc - 1900;
  utc_plus_offset_tm.tm_mon   = month_utc - 1;
  utc_plus_offset_tm.tm_mday  = day_utc;
  utc_plus_offset_tm.tm_isdst = -1;
  utc_plus_offset_tm.tm_hour  = static_cast<int>(hour_utc);
  utc_plus_offset_tm.tm_min   = static_cast<int>(minute_utc);
  utc_plus_offset_tm.tm_sec   = static_cast<int>(local_offset * 3600.0);

  const std::time_t local_t     = std::mktime(&utc_plus_offset_tm);
  const std::tm* const local_tm = std::localtime(&local_t);

  const bool is_date_valid =
      hour_utc <= 23 && minute_utc <= 59 && std::fabs(std::trunc(local_offset)) <= 14.0;
  if (is_date_valid) {
    std::array<char, 100> buffer{};
    const int local_offset_hour = static_cast<int>(std::fabs(std::trunc(local_offset)));
    const int local_offset_min = static_cast<int>((local_offset - std::trunc(local_offset)) * 60.0);

    if (local_offset_hour == 0 && local_offset_min == 0) {
      static_cast<void>(std::snprintf(buffer.data(), buffer.size(), "%04d-%02d-%02dT%02d:%02d:00Z",
                                      local_tm->tm_year + 1900, local_tm->tm_mon + 1,
                                      local_tm->tm_mday, local_tm->tm_hour, local_tm->tm_min));
    } else {
      static_cast<void>(
          std::snprintf(buffer.data(), buffer.size(), "%04d-%02d-%02dT%02d:%02d:00%s%02d:%02d",
                        local_tm->tm_year + 1900, local_tm->tm_mon + 1, local_tm->tm_mday,
                        local_tm->tm_hour, local_tm->tm_min, local_offset > 0 ? "+" : "-",
                        local_offset_hour, std::abs(local_offset_min)));
    }
    clock_time_       = std::string(buffer.data());
    out["clock_time"] = clock_time_;
  } else {
    out["debug"].push_back("invalid date/time");
  }
}

// Group 5: Transparent data channels
void Station::decodeType5(const Group& group, ObjectTree& out) {
  const auto address                 = getBits<5>(group.get(BLOCK2), 0);
  out["transparent_data"]["address"] = address;

  if (group.getType().value.version == GroupType::Version::A) {
    const std::array<std::uint8_t, 4> data{
        getUint8(group.get(BLOCK3), 8), getUint8(group.get(BLOCK3), 0),
        getUint8(group.get(BLOCK4), 8), getUint8(group.get(BLOCK4), 0)};

    out["transparent_data"]["raw"] = getHexString<2>(data[0]) + " " + getHexString<2>(data[1]) +
                                     " " + getHexString<2>(data[2]) + " " +
                                     getHexString<2>(data[3]);

    RDSString decoded_text(4);
    decoded_text.set(0, data[0], data[1]);
    decoded_text.set(2, data[2], data[3]);

    full_tdc_.set(address * 4U, data[0], data[1]);
    full_tdc_.set(address * 4U + 2U, data[2], data[3]);
    if (full_tdc_.isComplete()) {
      out["transparent_data"]["full_text"] = full_tdc_.str();

      std::vector<std::string> full_raw;
      full_raw.reserve(full_tdc_.getData().size());
      for (const auto b : full_tdc_.getData()) {
        full_raw.push_back(getHexString<2>(b));
      }
      out["transparent_data"]["full_raw"] = join(full_raw, " ");
    }

    out["transparent_data"]["as_text"] = decoded_text.str();
  } else {
    const std::array<std::uint8_t, 2> data{getUint8(group.get(BLOCK4), 8),
                                           getUint8(group.get(BLOCK4), 0)};

    out["transparent_data"]["raw"] = getHexString<2>(data[0]) + " " + getHexString<2>(data[1]);

    RDSString decoded_text(2);
    decoded_text.set(0, data[0], data[1]);
    out["transparent_data"]["as_text"] = decoded_text.str();
  }
}

// Group 6: In-house applications
void Station::decodeType6(const Group& group, ObjectTree& out) {
  out["in_house_data"].push_back(getBits<5>(group.get(BLOCK2), 0));

  if (group.getType().value.version == GroupType::Version::A) {
    if (group.has(BLOCK3)) {
      out["in_house_data"].push_back(getBits<16>(group.get(BLOCK3), 0));
      if (group.has(BLOCK4)) {
        out["in_house_data"].push_back(getBits<16>(group.get(BLOCK4), 0));
      }
    }
  } else {
    if (group.has(BLOCK4)) {
      out["in_house_data"].push_back(getBits<16>(group.get(BLOCK4), 0));
    }
  }
}

// Group 9A: Emergency warning systems
void Station::decodeType9A(const Group& group, ObjectTree& out) {
  static_cast<void>(group);
  out["debug"].push_back("TODO: 9A");
}

// Group 10A: Programme Type Name
void Station::decodeType10A(const Group& group, ObjectTree& out) {
  if (!group.has(BLOCK3) || !group.has(BLOCK4))
    return;

  const std::uint16_t segment_address = getBits<1>(group.get(BLOCK2), 0);

  if (ptyname_.isABChanged(getBits<1>(group.get(BLOCK2), 4)))
    ptyname_.text.clear();

  ptyname_.update(segment_address * 4U, getUint8(group.get(BLOCK3), 8),
                  getUint8(group.get(BLOCK3), 0), getUint8(group.get(BLOCK4), 8),
                  getUint8(group.get(BLOCK4), 0));

  if (ptyname_.text.isComplete()) {
    out["pty_name"] = ptyname_.text.getLastCompleteString();
  }
}

// Group 14: Enhanced Other Networks information
void Station::decodeType14(const Group& group, ObjectTree& out) {
  if (!(group.has(BLOCK4)))
    return;

  const std::uint16_t on_pi  = group.get(BLOCK4);
  out["other_network"]["pi"] = getPrefixedHexString<4>(on_pi);
  out["other_network"]["tp"] = getBool(group.get(BLOCK2), 4);

  if (group.getType().value.version == GroupType::Version::B) {
    out["other_network"]["ta"] = getBool(group.get(BLOCK2), 3);
    return;
  }

  if (!group.has(BLOCK3))
    return;

  const std::uint16_t eon_variant = getBits<4>(group.get(BLOCK2), 0);
  switch (eon_variant) {
    case 0:
    case 1:
    case 2:
    case 3:
      if (eon_ps_names_.find(on_pi) == eon_ps_names_.end())
        eon_ps_names_.emplace(on_pi, RDSString(8));

      eon_ps_names_[on_pi].set(2U * eon_variant, getUint8(group.get(BLOCK3), 8));
      eon_ps_names_[on_pi].set(2U * eon_variant + 1, getUint8(group.get(BLOCK3), 0));

      if (eon_ps_names_[on_pi].isComplete())
        out["other_network"]["ps"] = eon_ps_names_[on_pi].getLastCompleteString();
      break;

    case 4:
      eon_alt_freqs_[on_pi].insert(getUint8(group.get(BLOCK3), 8));
      eon_alt_freqs_[on_pi].insert(getUint8(group.get(BLOCK3), 0));

      if (eon_alt_freqs_[on_pi].isComplete()) {
        for (const int freq : eon_alt_freqs_[on_pi].getRawList())
          out["other_network"]["alt_frequencies"].push_back(freq);
        eon_alt_freqs_[on_pi].clear();
      }
      break;

    case 5:
    case 6:
    case 7:
    case 8:
    case 9: {
      const CarrierFrequency freq_other(getUint8(group.get(BLOCK3), 0));

      if (freq_other.isValid())
        out["other_network"]["kilohertz"] = freq_other.kHz();

      break;
    }

      // 10, 11 unallocated

    case 12: {
      const bool has_linkage              = getBool(group.get(BLOCK3), 15);
      const std::uint16_t lsn             = getBits<12>(group.get(BLOCK3), 0);
      out["other_network"]["has_linkage"] = has_linkage;
      if (has_linkage && lsn != 0)
        out["other_network"]["linkage_set"] = lsn;
      break;
    }

    case 13: {
      const std::uint16_t pty = getBits<5>(group.get(BLOCK3), 11);
      const bool ta           = getBool(group.get(BLOCK3), 0);
      out["other_network"]["prog_type"] =
          options_.rbds ? getPTYNameStringRBDS(pty) : getPTYNameString(pty);
      out["other_network"]["ta"] = ta;
      break;
    }

    case 14: {
      const std::uint16_t pin = group.get(BLOCK3);

      if (pin != 0x0000)
        decodePIN(pin, out["other_network"]);
      break;
    }

    case 15: {
      out["other_network"]["broadcaster_data"] = getHexString<4>(group.get(BLOCK3));
      break;
    }

    default:
      out["debug"].push_back("TODO: EON variant " +
                             std::to_string(getBits<4>(group.get(BLOCK2), 0)));
      break;
  }
}

/* Group 15A: Long PS or ODA */
// @note Based on captures and https://www.rds.org.uk/2010/Glossary-Of-Terms.htm
void Station::decodeType15A(const Group& group, ObjectTree& out) {
  const std::uint16_t segment_address = getBits<3>(group.get(BLOCK2), 0);

  if (group.has(BLOCK3)) {
    long_ps_.update(segment_address * 4U, getUint8(group.get(BLOCK3), 8),
                    getUint8(group.get(BLOCK3), 0));
  }
  if (group.has(BLOCK4)) {
    long_ps_.update(segment_address * 4U + 2U, getUint8(group.get(BLOCK4), 8),
                    getUint8(group.get(BLOCK4), 0));
  }

  if ((group.has(BLOCK3) || group.has(BLOCK4)) && long_ps_.text.isComplete()) {
    out["long_ps"] = rtrim(long_ps_.text.getLastCompleteString());
  } else if (options_.show_partial) {
    try {
      // May throw if UCS-2
      out["partial_long_ps"] = long_ps_.text.str();
    } catch (const std::exception& e) {
      out["debug"].push_back(e.what());
      return;
    }
  }
}

/* Group 15B: Fast basic tuning and switching information */
void Station::decodeType15B(const Group& group, ObjectTree& out) {
  const auto block_num                = group.has(BLOCK2) ? BLOCK2 : BLOCK4;
  const std::uint16_t segment_address = getBits<2>(group.get(BLOCK2), 0);
  const bool is_di                    = getBool(group.get(BLOCK2), 2);

  out["di"][getDICodeString(segment_address)] = is_di;
  out["ta"]                                   = getBool(group.get(block_num), 4);
  out["is_music"]                             = getBool(group.get(block_num), 3);
}

/* Open Data Application */
void Station::decodeODAGroup(const Group& group, ObjectTree& out) {
  if (oda_app_for_group_.find(group.getType().value) == oda_app_for_group_.end()) {
    out["unknown_oda"]["raw_data"] =
        getHexString<2>(group.get(BLOCK2) & 0b11111U) + " " +
        (group.has(BLOCK3) ? getHexString<4>(group.get(BLOCK3)) : "----") + " " +
        (group.has(BLOCK4) ? getHexString<4>(group.get(BLOCK4)) : "----");

    return;
  }

  const std::uint16_t oda_app_id = oda_app_for_group_[group.getType().value];

  switch (oda_app_id) {
    // DAB cross-referencing
    case 0x0093: parseDAB(group, out); break;

    // RT+
    case 0x4BD7: parseRadioTextPlus(group, radiotext_, out["radiotext_plus"]); break;

    // RT+ for Enhanced RadioText
    case 0x4BD8: parseRadioTextPlus(group, ert_, out["ert_plus"]); break;

    // Enhanced RadioText (eRT)
    case 0x6552: parseEnhancedRT(group, out); break;

    // RDS-TMC
    case 0xCD46:
    case 0xCD47:
      if (group.has(BLOCK2) && group.has(BLOCK3) && group.has(BLOCK4))
        tmc_.receiveUserGroup(getBits<5>(group.get(BLOCK2), 0), group.get(BLOCK3),
                              group.get(BLOCK4), out);
      break;

    default:
      out["unknown_oda"]["app_id"]   = getHexString<4>(oda_app_id);
      out["unknown_oda"]["app_name"] = getAppNameString(oda_app_id);
      out["unknown_oda"]["raw_data"] =
          getHexString<2>(group.get(BLOCK2) & 0b11111U) + " " +
          (group.has(BLOCK3) ? getHexString<4>(group.get(BLOCK3)) : "----") + " " +
          (group.has(BLOCK4) ? getHexString<4>(group.get(BLOCK4)) : "----");
  }
}

// Type C groups (only transmitted on data streams 1-3)
void Station::decodeC(const Group& group, ObjectTree& out) {
  if (!group.has(BLOCK1) || !group.has(BLOCK2) || !group.has(BLOCK3) || !group.has(BLOCK4))
    return;

  const std::uint32_t fid = getBits<2>(group.get(BLOCK1), 14);
  const std::uint32_t fn  = getBits<6>(group.get(BLOCK1), 8);

  if (fid == 0 && fn == 0) {
    // Page 47: Legacy type A & B transmission
    out["debug"].push_back("TODO: Tunnelling A & B over type C");
  } else if (fid == 0 && (fn & 0b11'0000U) == 0b10'0000U) {
    // Page 82: RFT data group for ODA pipe (fn & 0b1111)
    const auto pipe            = fn & 0b1111U;
    const auto toggle_bit      = getBits<1>(group.get(BLOCK1), 7);
    const auto segment_address = getBits<15>(group.get(BLOCK1), group.get(BLOCK2), 8);
    if (oda_app_for_pipe_.find(pipe) != oda_app_for_pipe_.end()) {
      out["open_data_app"]["app_name"] = getAppNameString(oda_app_for_pipe_[pipe]);
    }
    out["rft"]["data"]["pipe"]         = pipe;
    out["rft"]["data"]["toggle"]       = toggle_bit;
    out["rft"]["data"]["byte_address"] = segment_address * 5;

    rft_file_[pipe].receive(toggle_bit, segment_address, group.get(BLOCK2), group.get(BLOCK3),
                            group.get(BLOCK4));

    out["rft"]["data"]["segment_data"].push_back(getBits<8>(group.get(BLOCK2), 0));
    out["rft"]["data"]["segment_data"].push_back(getBits<8>(group.get(BLOCK3), 8));
    out["rft"]["data"]["segment_data"].push_back(getBits<8>(group.get(BLOCK3), 0));
    out["rft"]["data"]["segment_data"].push_back(getBits<8>(group.get(BLOCK4), 8));
    out["rft"]["data"]["segment_data"].push_back(getBits<8>(group.get(BLOCK4), 0));

    if (rft_file_[pipe].hasNewCompleteFile()) {
      out["rft"]["data"]["file_contents"] = rft_file_[pipe].getBase64Data();
      if (rft_file_[pipe].isCRCExpected()) {
        // TODO Let's test and enable this when we have real-world data
        // out["rft"]["data"]["crc_ok"] = rft_file_[pipe].checkCRC();
      }
      rft_file_[pipe].clear();
    }

  } else if (fid == 1) {
    // Page 47
    // Group type C ODA channel fn
    // Channels 0-15 are reserved for providing additional data
    out["open_data_app"]["channel"] = fn;
    out["open_data_app"]["app_data"].push_back(getBits<8>(group.get(BLOCK1), 0));
    out["open_data_app"]["app_data"].push_back(getBits<8>(group.get(BLOCK2), 8));
    out["open_data_app"]["app_data"].push_back(getBits<8>(group.get(BLOCK2), 0));
    out["open_data_app"]["app_data"].push_back(getBits<8>(group.get(BLOCK3), 8));
    out["open_data_app"]["app_data"].push_back(getBits<8>(group.get(BLOCK3), 0));
    out["open_data_app"]["app_data"].push_back(getBits<8>(group.get(BLOCK4), 8));
    out["open_data_app"]["app_data"].push_back(getBits<8>(group.get(BLOCK4), 0));
  } else if (fid == 0b10 && fn == 0b000000) {
    // Page 48
    // AID and channel number assignment for group type C ODAs

    const int ass_method = getBits<2>(group.get(BLOCK1), 6) + 1;
    const int channel_id = getBits<6>(group.get(BLOCK1), 0);
    if (ass_method == 1) {
      out["open_data_app"]["channel"]  = channel_id;
      out["open_data_app"]["oda_aid"]  = group.get(BLOCK2);
      oda_app_for_pipe_[channel_id]    = group.get(BLOCK2);
      out["open_data_app"]["app_name"] = getAppNameString(group.get(BLOCK2));

      const bool is_rft = channel_id < 16;

      if (is_rft) {
        // RFT: Page 79
        const int variant = getBits<4>(group.get(BLOCK3), 12);
        if (variant == 0) {
          const bool crc_flag            = getBool(group.get(BLOCK3), 11);
          const auto file_version        = getBits<3>(group.get(BLOCK3), 8);
          const auto file_identification = getBits<6>(group.get(BLOCK3), 2);
          const auto file_size_bytes     = getBits<18>(group.get(BLOCK3), group.get(BLOCK4), 0);

          rft_file_[channel_id].setSize(file_size_bytes);
          rft_file_[channel_id].setCRCFlag(crc_flag);

          out["rft"]["file_info"]["version"] = file_version;
          out["rft"]["file_info"]["id"]      = file_identification;
          out["rft"]["file_info"]["size"]    = file_size_bytes;
          out["rft"]["file_info"]["has_crc"] = crc_flag;
        } else if (variant == 1) {
          // CRC (Page 80)
          const auto crc_mode      = getBits<3>(group.get(BLOCK3), 9);
          const auto chunk_address = getBits<9>(group.get(BLOCK3), 0);
          const auto crc           = group.get(BLOCK4);

          const ChunkCRC chunk_crc{crc_mode, chunk_address, crc};
          rft_file_[channel_id].receiveCRC(chunk_crc);

          switch (crc_mode) {
            case 0: out["rft"]["crc_info"]["file_crc16"] = crc; break;
            case 1:
            case 2:
            case 3:
            case 4:
            case 5:
            case 7:
              out["rft"]["crc_info"]["chunk_crc16"]   = chunk_crc.crc;
              out["rft"]["crc_info"]["chunk_address"] = chunk_crc.address_raw;
              out["rft"]["crc_info"]["crc_mode"]      = chunk_crc.mode;
              break;
            default: out["debug"].push_back("TODO: CRC mode " + std::to_string(crc_mode)); break;
          }

        } else if (variant >= 8) {
          // 8..15: Non-file-related ODA data
          out["open_data_app"]["non_file_oda_data"] =
              getHexString<7>(getBits<28>(group.get(BLOCK3), group.get(BLOCK4), 0));
        } else if (variant >= 2) {
          // File-related ODA data
          out["open_data_app"]["file_oda_data"] =
              getHexString<7>(getBits<28>(group.get(BLOCK3), group.get(BLOCK4), 0));
        }
      } else {
        out["open_data_app"]["app_data"].push_back(getBits<8>(group.get(BLOCK3), 8));
        out["open_data_app"]["app_data"].push_back(getBits<8>(group.get(BLOCK3), 0));
        out["open_data_app"]["app_data"].push_back(getBits<8>(group.get(BLOCK4), 8));
        out["open_data_app"]["app_data"].push_back(getBits<8>(group.get(BLOCK4), 0));
      }

    } else {
      out["debug"].push_back("TODO: assignment method " + std::to_string(ass_method));
    }
  } else {
    out["debug"].push_back("TODO: FID " + std::to_string(fid) + " FN " + std::to_string(fn));
    out["open_data_app"]["data"].push_back(getBits<8>(group.get(BLOCK2), 8));
    out["open_data_app"]["data"].push_back(getBits<8>(group.get(BLOCK2), 0));
    out["open_data_app"]["data"].push_back(getBits<8>(group.get(BLOCK3), 8));
    out["open_data_app"]["data"].push_back(getBits<8>(group.get(BLOCK3), 0));
    out["open_data_app"]["data"].push_back(getBits<8>(group.get(BLOCK4), 8));
    out["open_data_app"]["data"].push_back(getBits<8>(group.get(BLOCK4), 0));
  }
}

// RadioText Plus (content-type tagging for RadioText)
void parseRadioTextPlus(const Group& group, RadioText& rt, ObjectTree& out) {
  const bool item_toggle  = getBool(group.get(BLOCK2), 4);
  const bool item_running = getBool(group.get(BLOCK2), 3);

  if (item_toggle != rt.plus.toggle || item_running != rt.plus.item_running) {
    rt.text.clear();
    rt.plus.toggle       = item_toggle;
    rt.plus.item_running = item_running;
  }

  out["item_running"] = item_running;
  out["item_toggle"]  = item_toggle ? 1 : 0;

  const std::size_t num_tags = group.has(BLOCK3) ? (group.has(BLOCK4) ? 2 : 1) : 0;
  std::vector<RadioText::Plus::Tag> tags(num_tags);

  if (num_tags > 0) {
    tags[0].content_type = getBits<6>(group.get(BLOCK2), group.get(BLOCK3), 13);
    tags[0].start        = getBits<6>(group.get(BLOCK3), 7);
    tags[0].length       = getBits<6>(group.get(BLOCK3), 1) + 1;

    if (num_tags == 2) {
      tags[1].content_type = getBits<6>(group.get(BLOCK3), group.get(BLOCK4), 11);
      tags[1].start        = getBits<6>(group.get(BLOCK4), 5);
      tags[1].length       = getBits<5>(group.get(BLOCK4), 0) + 1;
    }
  }

  for (const auto& tag : tags) {
    const std::string text = rt.text.getLastCompleteString(tag.start, tag.length);

    if (text.length() > 0 && tag.content_type != 0) {
      ObjectTree tag_tree;
      tag_tree["content-type"] = getRTPlusContentTypeString(tag.content_type);
      tag_tree["data"]         = rtrim(text);
      out["tags"].push_back(tag_tree);
    }
  }
}

// RDS2 Enhanced RadioText (eRT)
void Station::parseEnhancedRT(const Group& group, ObjectTree& out) {
  const std::size_t position = getBits<5>(group.get(BLOCK2), 0) * 4U;

  ert_.update(position, getUint8(group.get(BLOCK3), 8), getUint8(group.get(BLOCK3), 0));

  if (group.has(BLOCK4)) {
    ert_.update(position + 2U, getUint8(group.get(BLOCK4), 8), getUint8(group.get(BLOCK4), 0));
  }

  if (ert_.text.isComplete()) {
    out["enhanced_radiotext"] = rtrim(ert_.text.getLastCompleteString());
  }
}

// ETSI EN 301 700 V1.1.1 (2000-03)
void Station::parseDAB(const Group& group, ObjectTree& out) {
  const bool es_flag = getBool(group.get(BLOCK2), 4);

  if (es_flag) {
    // Service table
    out["debug"].push_back("TODO: DAB service table");

  } else {
    // Ensemble table

    const auto mode = getBits<2>(group.get(BLOCK2), 2);
    const std::array<std::string, 4> modes{"unspecified", "I", "II or III", "IV"};
    out["dab"]["mode"] = modes[mode];

    const std::uint32_t freq = 16 * getBits<18>(group.get(BLOCK2), group.get(BLOCK3), 0);

    out["dab"]["kilohertz"] = freq;

    const std::map<std::uint32_t, std::string> dab_channels({
        // clang-format off
        { 174'928,  "5A"}, { 176'640,  "5B"}, { 178'352,  "5C"}, { 180'064,  "5D"},
        { 181'936,  "6A"}, { 183'648,  "6B"}, { 185'360,  "6C"}, { 187'072,  "6D"},
        { 188'928,  "7A"}, { 190'640,  "7B"}, { 192'352,  "7C"}, { 194'064,  "7D"},
        { 195'936,  "8A"}, { 197'648,  "8B"}, { 199'360,  "8C"}, { 201'072,  "8D"},
        { 202'928,  "9A"}, { 204'640,  "9B"}, { 206'352,  "9C"}, { 208'064,  "9D"},
        { 209'936, "10A"}, { 211'648, "10B"}, { 213'360, "10C"}, { 215'072, "10D"},
        { 216'928, "11A"}, { 218'640, "11B"}, { 220'352, "11C"}, { 222'064, "11D"},
        { 223'936, "12A"}, { 225'648, "12B"}, { 227'360, "12C"}, { 229'072, "12D"},
        { 230'784, "13A"}, { 232'496, "13B"}, { 234'208, "13C"}, { 235'776, "13D"},
        { 237'488, "13E"}, { 239'200, "13F"}, {1452'960,  "LA"}, {1454'672,  "LB"},
        {1456'384,  "LC"}, {1458'096,  "LD"}, {1459'808,  "LE"}, {1461'520,  "LF"},
        {1463'232,  "LG"}, {1464'944,  "LH"}, {1466'656,  "LI"}, {1468'368,  "LJ"},
        {1470'080,  "LK"}, {1471'792,  "LL"}, {1473'504,  "LM"}, {1475'216,  "LN"},
        {1476'928,  "LO"}, {1478'640,  "LP"}, {1480'352,  "LQ"}, {1482'064,  "LR"},
        {1483'776,  "LS"}, {1485'488,  "LT"}, {1487'200,  "LU"}, {1488'912,  "LV"},
        {1490'624,  "LW"}  // clang-format on
    });

    if (dab_channels.find(freq) != dab_channels.end()) {
      out["dab"]["channel"] = dab_channels.at(freq);
    }

    out["dab"]["ensemble_id"] = getPrefixedHexString<4>(group.get(BLOCK4));
  }
}

}  // namespace redsea
