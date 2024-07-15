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
#include "src/groups.h"

#include <array>
#include <cmath>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <map>
#include <numeric>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "config.h"
#include "src/common.h"
#include "src/rdsstring.h"
#include "src/tables.h"
#include "src/util.h"

namespace redsea {

// Programme Item Number (IEC 62106:2015, section 6.1.5.2)
bool decodePIN(uint16_t pin, nlohmann::ordered_json* json) {
  const uint16_t day    = getBits<5>(pin, 11);
  const uint16_t hour   = getBits<5>(pin, 6);
  const uint16_t minute = getBits<6>(pin, 0);
  if (day >= 1 && hour <= 24 && minute <= 59) {
    (*json)["prog_item_number"]          = pin;
    (*json)["prog_item_started"]["day"]  = day;
    (*json)["prog_item_started"]["time"] = getHoursMinutesString(hour, minute);
    return true;
  } else {
    return false;
  }
}

GroupType::GroupType(uint16_t type_code)
    : number((type_code >> 1) & 0xF),
      version((type_code & 0x1) == 0 ? GroupType::Version::A : GroupType::Version::B) {}

std::string GroupType::str() const {
  return std::string(std::to_string(number) + (version == GroupType::Version::A ? "A" : "B"));
}

bool operator==(const GroupType& type1, const GroupType& type2) {
  return type1.number == type2.number && type1.version == type2.version;
}

bool operator<(const GroupType& type1, const GroupType& type2) {
  return (type1.number < type2.number) ||
         (type1.number == type2.number && type1.version < type2.version);
}

uint16_t Group::get(eBlockNumber block_num) const {
  return blocks_[block_num].data;
}

bool Group::has(eBlockNumber block_num) const {
  return blocks_[block_num].is_received;
}

bool Group::isEmpty() const {
  return !(has(BLOCK1) || has(BLOCK2) || has(BLOCK3) || has(BLOCK4));
}

// Remember to check if hasPI()
uint16_t Group::getPI() const {
  if (blocks_[BLOCK1].is_received)
    return blocks_[BLOCK1].data;
  else if (blocks_[BLOCK3].is_received && blocks_[BLOCK3].offset == Offset::Cprime)
    return blocks_[BLOCK3].data;
  else
    return 0x0000;
}

float Group::getBLER() const {
  return bler_;
}

int Group::getNumErrors() const {
  return std::accumulate(blocks_.cbegin(), blocks_.cend(), 0, [](int a, Block b) {
    return a + ((b.had_errors || !b.is_received) ? 1 : 0);
  });
}

bool Group::hasPI() const {
  return blocks_[BLOCK1].is_received ||
         (blocks_[BLOCK3].is_received && blocks_[BLOCK3].offset == Offset::Cprime);
}

GroupType Group::getType() const {
  return type_;
}

bool Group::hasType() const {
  return has_type_;
}

bool Group::hasBLER() const {
  return has_bler_;
}

bool Group::hasTime() const {
  return has_time_;
}

std::chrono::time_point<std::chrono::system_clock> Group::getRxTime() const {
  return time_received_;
}

void Group::disableOffsets() {
  no_offsets_ = true;
}

void Group::setBlock(eBlockNumber block_num, Block block) {
  blocks_[block_num] = block;

  if (block_num == BLOCK2) {
    type_ = GroupType(getBits<5>(block.data, 11));
    if (type_.version == GroupType::Version::A)
      has_type_ = true;
    else
      has_type_ = (has_c_prime_ || no_offsets_);

  } else if (block_num == BLOCK4) {
    if (has_c_prime_ && !has_type_) {
      const GroupType potential_type(getBits<5>(block.data, 11));
      if (potential_type.number == 15 && potential_type.version == GroupType::Version::B) {
        type_     = potential_type;
        has_type_ = true;
      }
    }
  }

  if (block.offset == Offset::Cprime && has(BLOCK2))
    has_type_ = (type_.version == GroupType::Version::B);
}

void Group::setTime(std::chrono::time_point<std::chrono::system_clock> t) {
  time_received_ = t;
  has_time_      = true;
}

void Group::setAverageBLER(float bler) {
  bler_     = bler;
  has_bler_ = true;
}

/*
 * Print the raw group data into a stream, encoded as hex, like in RDS Spy.
 * Invalid blocks are replaced with "----".
 *
 */
void Group::printHex(std::ostream& stream) const {
  std::ios old_stream_state(nullptr);
  old_stream_state.copyfmt(stream);

  stream.fill('0');
  stream.setf(std::ios_base::uppercase);

  for (eBlockNumber block_num : {BLOCK1, BLOCK2, BLOCK3, BLOCK4}) {
    const Block& block = blocks_[block_num];
    if (block.is_received)
      stream << std::hex << std::setw(4) << block.data;
    else
      stream << "----";

    if (block_num != BLOCK4)
      stream << " ";
  }

  // Restore ostream format
  stream.copyfmt(old_stream_state);
}

/*
 * A Station represents a single broadcast carrier identified by its RDS PI
 * code.
 *
 */
Station::Station(const Options& options, int which_channel)
    : options_(options), which_channel_(which_channel), tmc_(options) {}

Station::Station(const Options& options, int which_channel, uint16_t _pi)
    : Station(options, which_channel) {
  pi_     = _pi;
  has_pi_ = true;
}

void Station::updateAndPrint(const Group& group, std::ostream& stream) {
  if (!has_pi_)
    return;

  // Allow 1 group with missed PI. For subsequent misses, don't process at all.
  if (group.hasPI())
    last_group_had_pi_ = true;
  else if (last_group_had_pi_)
    last_group_had_pi_ = false;
  else
    return;

  if (group.isEmpty())
    return;

  json_.clear();
  json_["pi"] = getPrefixedHexString(getPI(), 4);
  if (options_.rbds) {
    const std::string callsign{getCallsignFromPI(getPI())};
    if (!callsign.empty()) {
      if ((getPI() & 0xF000) == 0x1000)
        json_["callsign_uncertain"] = callsign;
      else
        json_["callsign"] = callsign;
    }
  }

  if (options_.timestamp)
    json_["rx_time"] = getTimePointString(group.getRxTime(), options_.time_format);

  if (group.hasBLER())
    json_["bler"] = static_cast<int>(group.getBLER() + .5f);

  if (options_.num_channels > 1)
    json_["channel"] = which_channel_;

  if (options_.show_raw) {
    std::stringstream ss;
    group.printHex(ss);
    json_["raw_data"] = ss.str();
  }

  decodeBasics(group);

  // ODA support in groups
  // ---------------------
  //
  // -  can't be used for ODA
  // o  can be used for ODA
  // O  ODA only
  //
  //             111111
  //   0123456789012345
  // A -----ooooo-OOo--
  // B ---OOooOOOOOOO--

  if (group.hasType()) {
    const GroupType& type = group.getType();

    // These groups can't be used for ODA
    if (type.number == 0) {
      decodeType0(group);
    } else if (type.number == 1) {
      decodeType1(group);
    } else if (type.number == 2) {
      decodeType2(group);
    } else if (type.number == 3 && type.version == GroupType::Version::A) {
      decodeType3A(group);
    } else if (type.number == 4 && type.version == GroupType::Version::A) {
      decodeType4A(group);
    } else if (type.number == 10 && type.version == GroupType::Version::A) {
      decodeType10A(group);
    } else if (type.number == 14) {
      decodeType14(group);
    } else if (type.number == 15 && type.version == GroupType::Version::B) {
      decodeType15B(group);

      // Other groups can be reassigned for ODA by a 3A group
    } else if (oda_app_for_group_.find(type) != oda_app_for_group_.end()) {
      decodeODAGroup(group);

      // Below: Groups that could optionally be used for ODA but have
      // another primary function
    } else if (type.number == 5) {
      decodeType5(group);
    } else if (type.number == 6) {
      decodeType6(group);
    } else if (type.number == 7 && type.version == GroupType::Version::A) {
      decodeType7A(group);
    } else if (type.number == 8 && type.version == GroupType::Version::A) {
      if (group.has(BLOCK2) && group.has(BLOCK3) && group.has(BLOCK4))
        tmc_.receiveUserGroup(getBits<5>(group.get(BLOCK2), 0), group.get(BLOCK3),
                              group.get(BLOCK4), &json_);
    } else if (type.number == 9 && type.version == GroupType::Version::A) {
      decodeType9A(group);

    } else if (type.number == 15 && type.version == GroupType::Version::A) {
      decodeType15A(group);

      // ODA-only groups
      // 3B, 4B, 7B, 8B, 9B, 10B, 11A, 11B, 12A, 12B, 13B
    } else {
      decodeODAGroup(group);
    }
  }

  try {
    // nlohmann::operator<< throws if a string contains non-UTF8 data.
    // It's better to throw while writing to a stringstream; otherwise
    // incomplete JSON objects could get printed.
    std::stringstream output_proxy_stream;
    output_proxy_stream << json_;
    stream << output_proxy_stream.str() << std::endl << std::flush;
  } catch (const std::exception& e) {
    nlohmann::ordered_json json_from_exception;
    json_from_exception["debug"] = std::string(e.what());
    stream << json_from_exception << std::endl << std::flush;
  }
}

uint16_t Station::getPI() const {
  return pi_;
}

void Station::decodeBasics(const Group& group) {
  if (group.has(BLOCK2)) {
    const uint16_t pty = getBits<5>(group.get(BLOCK2), 5);

    if (group.hasType())
      json_["group"] = group.getType().str();

    json_["tp"] = getBool(group.get(BLOCK2), 10);

    json_["prog_type"] = options_.rbds ? getPTYNameStringRBDS(pty) : getPTYNameString(pty);
  } else if (group.getType().number == 15 && group.getType().version == GroupType::Version::B &&
             group.has(BLOCK4)) {
    const uint16_t pty = getBits<5>(group.get(BLOCK4), 5);

    json_["group"] = group.getType().str();

    json_["tp"]        = getBool(group.get(BLOCK4), 10);
    json_["prog_type"] = options_.rbds ? getPTYNameStringRBDS(pty) : getPTYNameString(pty);
  }
}

// Group 0: Basic tuning and switching information
void Station::decodeType0(const Group& group) {
  // Block 2: Flags
  const uint16_t segment_address                = getBits<2>(group.get(BLOCK2), 0);
  const bool is_di                              = getBool(group.get(BLOCK2), 2);
  json_["di"][getDICodeString(segment_address)] = is_di;
  json_["ta"]                                   = getBool(group.get(BLOCK2), 4);
  json_["is_music"]                             = getBool(group.get(BLOCK2), 3);

  if (!group.has(BLOCK3)) {
    // Reset a Method B list to prevent mixing up different lists
    if (alt_freq_list_.isMethodB())
      alt_freq_list_.clear();
    return;
  }

  // Block 3: Alternative frequencies
  if (group.getType().version == GroupType::Version::A) {
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
        const size_t expected_number_of_afs = raw_frequencies.size() / 2;
        const size_t number_of_unique_afs =
            unique_alternative_frequencies.size() + unique_regional_variants.size();
        if (number_of_unique_afs == expected_number_of_afs) {
          json_["alt_frequencies_b"]["tuned_frequency"] = tuned_frequency;

          for (const int frequency : alternative_frequencies)
            json_["alt_frequencies_b"]["same_programme"].push_back(frequency);

          for (const int frequency : regional_variants)
            json_["alt_frequencies_b"]["regional_variants"].push_back(frequency);
        }
      } else {
        // AF Method A is a simple list
        for (const int frequency : raw_frequencies) json_["alt_frequencies_a"].push_back(frequency);
      }

      alt_freq_list_.clear();

    } else if (options_.show_partial) {
      // If partial list is requested we'll print the raw list and not attempt to
      // deduce whether it's Method A or B
      for (const int f : alt_freq_list_.getRawList()) json_["partial_alt_frequencies"].push_back(f);
    }
  }

  if (!group.has(BLOCK4))
    return;

  // Block 4: Program Service Name
  ps_.update(segment_address * 2, getUint8(group.get(BLOCK4), 8), getUint8(group.get(BLOCK4), 0));

  if (ps_.text.isComplete())
    json_["ps"] = ps_.text.getLastCompleteString();
  else if (options_.show_partial)
    json_["partial_ps"] = ps_.text.str();
}

// Group 1: Programme Item Number and slow labelling codes
void Station::decodeType1(const Group& group) {
  if (!(group.has(BLOCK3) && group.has(BLOCK4)))
    return;

  pin_ = group.get(BLOCK4);

  if (pin_ != 0x0000)
    if (!decodePIN(pin_, &json_))
      json_["debug"].push_back("invalid PIN");

  if (group.getType().version == GroupType::Version::A) {
    pager_.paging_code = getBits<3>(group.get(BLOCK2), 2);
    if (pager_.paging_code != 0)
      pager_.interval = getBits<2>(group.get(BLOCK2), 0);
    linkage_la_          = getBool(group.get(BLOCK3), 15);
    json_["has_linkage"] = linkage_la_;

    const int slow_label_variant = getBits<3>(group.get(BLOCK3), 12);

    switch (slow_label_variant) {
      case 0:
        if (pager_.paging_code != 0) {
          pager_.opc = getBits<4>(group.get(BLOCK3), 8);

          // No PIN (IEC 62106:2015, section M.3.2.5.3)
          if (group.has(BLOCK4) && getBits<5>(group.get(BLOCK4), 11) == 0)
            pager_.decode1ABlock4(group.get(BLOCK4));
        }

        ecc_ = getUint8(group.get(BLOCK3), 0);
        cc_  = getBits<4>(pi_, 12);

        if (ecc_ != 0x00) {
          has_country_     = true;
          json_["country"] = getCountryString(cc_, ecc_);
        }
        break;

      case 1:
        tmc_id_         = getBits<12>(group.get(BLOCK3), 0);
        json_["tmc_id"] = tmc_id_;
        break;

      case 2:
        if (pager_.paging_code != 0) {
          pager_.pac = getBits<6>(group.get(BLOCK3), 0);
          pager_.opc = getBits<4>(group.get(BLOCK3), 8);

          // No PIN (IEC 62105:2015, section M.3.2.5.3)
          if (group.has(BLOCK4) && getBits<5>(group.get(BLOCK4), 11) == 0)
            pager_.decode1ABlock4(group.get(BLOCK4));
        }
        break;

      case 3: json_["language"] = getLanguageString(getUint8(group.get(BLOCK3), 0)); break;

      case 7: json_["ews"] = getBits<12>(group.get(BLOCK3), 0); break;

      default:
        json_["debug"].push_back("TODO: SLC variant " + std::to_string(slow_label_variant));
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
void Station::decodeType2(const Group& group) {
  if (!(group.has(BLOCK3) && group.has(BLOCK4)))
    return;

  const size_t radiotext_position = getBits<4>(group.get(BLOCK2), 0) *
                                    (group.getType().version == GroupType::Version::A ? 4UL : 2UL);

  const bool is_ab_changed = radiotext_.isABChanged(getBits<1>(group.get(BLOCK2), 4));

  // If these heuristics match it's possible that we just received a full random-length message
  // with no string terminator (method 3 above).
  std::string potentially_complete_message;
  bool has_potentially_complete_message =
      radiotext_position == 0 && radiotext_.text.getReceivedLength() > 1 &&
      not radiotext_.text.isComplete() && not radiotext_.text.hasPreviouslyReceivedTerminators();

  if (has_potentially_complete_message) {
    potentially_complete_message = rtrim(radiotext_.text.str());

    // No, perhaps we just lost the terminator in noise [could we use the actual BLER figure?],
    // or maybe the message got interrupted by an A/B change. Let's wait for a repeat.
    if (potentially_complete_message != radiotext_.previous_potentially_complete_message) {
      has_potentially_complete_message = false;
    }
    radiotext_.previous_potentially_complete_message = potentially_complete_message;
  }

  // The transmitter requests us to clear the buffer (message contents will change).
  // Note: This is sometimes overused in the wild.
  if (is_ab_changed)
    radiotext_.text.clear();

  if (group.getType().version == GroupType::Version::A) {
    radiotext_.text.resize(64);
    radiotext_.update(radiotext_position, getUint8(group.get(BLOCK3), 8),
                      getUint8(group.get(BLOCK3), 0));
  } else {
    radiotext_.text.resize(32);
  }

  if (group.has(BLOCK4)) {
    radiotext_.update(
        radiotext_position + (group.getType().version == GroupType::Version::A ? 2 : 0),
        getUint8(group.get(BLOCK4), 8), getUint8(group.get(BLOCK4), 0));
  }

  // Transmitter used Method 1 or 2 convey the length of the string.
  if (radiotext_.text.isComplete()) {
    json_["radiotext"] = rtrim(radiotext_.text.getLastCompleteString());

    // Method 3 was used instead (and was confirmed by a repeat).
  } else if (has_potentially_complete_message) {
    json_["radiotext"] = rtrim(std::move(potentially_complete_message));

    // The string is not complete yet, but user wants to see it anyway.
  } else if (options_.show_partial && rtrim(radiotext_.text.str()).length() > 0) {
    json_["partial_radiotext"] = radiotext_.text.str();
  }
}

// Group 3A: Application identification for Open Data
void Station::decodeType3A(const Group& group) {
  if (!(group.has(BLOCK3) && group.has(BLOCK4)))
    return;

  if (group.getType().version != GroupType::Version::A)
    return;

  const GroupType oda_group_type(getBits<5>(group.get(BLOCK2), 0));
  const uint16_t oda_message{group.get(BLOCK3)};
  const uint16_t oda_app_id{group.get(BLOCK4)};

  oda_app_for_group_[oda_group_type] = oda_app_id;

  json_["open_data_app"]["oda_group"] = oda_group_type.str();
  json_["open_data_app"]["app_name"]  = getAppNameString(oda_app_id);

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
    case 0xCD47: tmc_.receiveSystemGroup(oda_message, &json_); break;

    default:
      json_["debug"].push_back("TODO: Unimplemented ODA app " + getHexString(oda_app_id, 4));
      json_["open_data_app"]["message"] = oda_message;
      break;
  }
}

// Group 4A: Clock-time and date
void Station::decodeType4A(const Group& group) {
  if (!(group.has(BLOCK3) && group.has(BLOCK4)))
    return;

  const double modified_julian_date{
      static_cast<double>(getBits<17>(group.get(BLOCK2), group.get(BLOCK3), 1))};

  // Would result in negative years/months
  if (modified_julian_date < 15079.0) {
    json_["debug"].push_back("invalid date/time");
    return;
  }

  int year_utc = static_cast<int>((modified_julian_date - 15078.2) / 365.25);
  int month_utc =
      static_cast<int>((modified_julian_date - 14956.1 - std::trunc(year_utc * 365.25)) / 30.6001);
  int day_utc = static_cast<int>(modified_julian_date - 14956.0 - std::trunc(year_utc * 365.25) -
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

  struct tm utc_plus_offset_tm;
  utc_plus_offset_tm.tm_year  = year_utc - 1900;
  utc_plus_offset_tm.tm_mon   = month_utc - 1;
  utc_plus_offset_tm.tm_mday  = day_utc;
  utc_plus_offset_tm.tm_isdst = -1;
  utc_plus_offset_tm.tm_hour  = static_cast<int>(hour_utc);
  utc_plus_offset_tm.tm_min   = static_cast<int>(minute_utc);
  utc_plus_offset_tm.tm_sec   = static_cast<int>(local_offset * 3600.0);

  const time_t local_t            = mktime(&utc_plus_offset_tm);
  const struct tm* const local_tm = localtime(&local_t);

  const bool is_date_valid =
      hour_utc <= 23 && minute_utc <= 59 && fabs(std::trunc(local_offset)) <= 14.0;
  if (is_date_valid) {
    char buffer[100];
    const int local_offset_hour = static_cast<int>(std::fabs(std::trunc(local_offset)));
    const int local_offset_min = static_cast<int>((local_offset - std::trunc(local_offset)) * 60.0);

    if (local_offset_hour == 0 && local_offset_min == 0) {
      snprintf(buffer, sizeof(buffer), "%04d-%02d-%02dT%02d:%02d:00Z", local_tm->tm_year + 1900,
               local_tm->tm_mon + 1, local_tm->tm_mday, local_tm->tm_hour, local_tm->tm_min);
    } else {
      snprintf(buffer, sizeof(buffer), "%04d-%02d-%02dT%02d:%02d:00%s%02d:%02d",
               local_tm->tm_year + 1900, local_tm->tm_mon + 1, local_tm->tm_mday, local_tm->tm_hour,
               local_tm->tm_min, local_offset > 0 ? "+" : "-", local_offset_hour,
               abs(local_offset_min));
    }
    clock_time_         = std::string(buffer);
    json_["clock_time"] = clock_time_;
  } else {
    json_["debug"].push_back("invalid date/time");
  }
}

// Group 5: Transparent data channels
void Station::decodeType5(const Group& group) {
  const auto address                   = getBits<5>(group.get(BLOCK2), 0);
  json_["transparent_data"]["address"] = address;

  if (group.getType().version == GroupType::Version::A) {
    const std::array<uint8_t, 4> data{
        getUint8(group.get(BLOCK3), 8), getUint8(group.get(BLOCK3), 0),
        getUint8(group.get(BLOCK4), 8), getUint8(group.get(BLOCK4), 0)};

    json_["transparent_data"]["raw"] = getHexString(data[0], 2) + " " + getHexString(data[1], 2) +
                                       " " + getHexString(data[2], 2) + " " +
                                       getHexString(data[3], 2);

    RDSString decoded_text(4);
    decoded_text.set(0, data[0], data[1]);
    decoded_text.set(2, data[2], data[3]);

    full_tdc_.set(address * 4, data[0], data[1]);
    full_tdc_.set(address * 4 + 2, data[2], data[3]);
    if (full_tdc_.isComplete()) {
      json_["transparent_data"]["full_text"] = full_tdc_.str();

      std::string full_raw;
      for (const auto b : full_tdc_.getData()) {
        full_raw += getHexString(b, 2) + " ";
      }
      json_["transparent_data"]["full_raw"] = full_raw;
    }

    json_["transparent_data"]["as_text"] = decoded_text.str();
  } else {
    const std::array<uint8_t, 2> data{getUint8(group.get(BLOCK4), 8),
                                      getUint8(group.get(BLOCK4), 0)};

    json_["transparent_data"]["raw"] = getHexString(data[0], 2) + " " + getHexString(data[1], 2);

    RDSString decoded_text(2);
    decoded_text.set(0, data[0], data[1]);
    json_["transparent_data"]["as_text"] = decoded_text.str();
  }
}

// Group 6: In-house applications
void Station::decodeType6(const Group& group) {
  json_["in_house_data"].push_back(getBits<5>(group.get(BLOCK2), 0));

  if (group.getType().version == GroupType::Version::A) {
    if (group.has(BLOCK3)) {
      json_["in_house_data"].push_back(getBits<16>(group.get(BLOCK3), 0));
      if (group.has(BLOCK4)) {
        json_["in_house_data"].push_back(getBits<16>(group.get(BLOCK4), 0));
      }
    }
  } else {
    if (group.has(BLOCK4)) {
      json_["in_house_data"].push_back(getBits<16>(group.get(BLOCK4), 0));
    }
  }
}

// Group 7A: Radio Paging
void Station::decodeType7A(const Group& group) {
  static_cast<void>(group);
  json_["debug"].push_back("TODO: 7A");
}

// Group 9A: Emergency warning systems
void Station::decodeType9A(const Group& group) {
  static_cast<void>(group);
  json_["debug"].push_back("TODO: 9A");
}

// Group 10A: Programme Type Name
void Station::decodeType10A(const Group& group) {
  if (!group.has(BLOCK3) || !group.has(BLOCK4))
    return;

  const uint16_t segment_address = getBits<1>(group.get(BLOCK2), 0);

  if (ptyname_.isABChanged(getBits<1>(group.get(BLOCK2), 4)))
    ptyname_.text.clear();

  ptyname_.update(segment_address * 4, getUint8(group.get(BLOCK3), 8),
                  getUint8(group.get(BLOCK3), 0), getUint8(group.get(BLOCK4), 8),
                  getUint8(group.get(BLOCK4), 0));

  if (ptyname_.text.isComplete()) {
    json_["pty_name"] = ptyname_.text.getLastCompleteString();
  }
}

// Group 14: Enhanced Other Networks information
void Station::decodeType14(const Group& group) {
  if (!(group.has(BLOCK4)))
    return;

  const uint16_t on_pi         = group.get(BLOCK4);
  json_["other_network"]["pi"] = getPrefixedHexString(on_pi, 4);
  json_["other_network"]["tp"] = getBool(group.get(BLOCK2), 4);

  if (group.getType().version == GroupType::Version::B) {
    json_["other_network"]["ta"] = getBool(group.get(BLOCK2), 3);
    return;
  }

  if (!group.has(BLOCK3))
    return;

  const uint16_t eon_variant = getBits<4>(group.get(BLOCK2), 0);
  switch (eon_variant) {
    case 0:
    case 1:
    case 2:
    case 3:
      if (eon_ps_names_.find(on_pi) == eon_ps_names_.end())
        eon_ps_names_.emplace(on_pi, RDSString(8));

      eon_ps_names_[on_pi].set(2 * eon_variant, getUint8(group.get(BLOCK3), 8));
      eon_ps_names_[on_pi].set(2 * eon_variant + 1, getUint8(group.get(BLOCK3), 0));

      if (eon_ps_names_[on_pi].isComplete())
        json_["other_network"]["ps"] = eon_ps_names_[on_pi].getLastCompleteString();
      break;

    case 4:
      eon_alt_freqs_[on_pi].insert(getUint8(group.get(BLOCK3), 8));
      eon_alt_freqs_[on_pi].insert(getUint8(group.get(BLOCK3), 0));

      if (eon_alt_freqs_[on_pi].isComplete()) {
        for (const int freq : eon_alt_freqs_[on_pi].getRawList())
          json_["other_network"]["alt_frequencies"].push_back(freq);
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
        json_["other_network"]["kilohertz"] = freq_other.kHz();

      break;
    }

      // 10, 11 unallocated

    case 12: {
      const bool has_linkage                = getBool(group.get(BLOCK3), 15);
      const uint16_t lsn                    = getBits<12>(group.get(BLOCK3), 0);
      json_["other_network"]["has_linkage"] = has_linkage;
      if (has_linkage && lsn != 0)
        json_["other_network"]["linkage_set"] = lsn;
      break;
    }

    case 13: {
      const uint16_t pty = getBits<5>(group.get(BLOCK3), 11);
      const bool ta      = getBool(group.get(BLOCK3), 0);
      json_["other_network"]["prog_type"] =
          options_.rbds ? getPTYNameStringRBDS(pty) : getPTYNameString(pty);
      json_["other_network"]["ta"] = ta;
      break;
    }

    case 14: {
      const uint16_t pin = group.get(BLOCK3);

      if (pin != 0x0000)
        decodePIN(pin, &(json_["other_network"]));
      break;
    }

    case 15: {
      json_["other_network"]["broadcaster_data"] = getHexString(group.get(BLOCK3), 4);
      break;
    }

    default:
      json_["debug"].push_back("TODO: EON variant " +
                               std::to_string(getBits<4>(group.get(BLOCK2), 0)));
      break;
  }
}

/* Group 15A: Long PS or ODA */
// @note Based on captures and https://www.rds.org.uk/2010/Glossary-Of-Terms.htm
void Station::decodeType15A(const Group& group) {
  const uint16_t segment_address = getBits<3>(group.get(BLOCK2), 0);

  if (group.has(BLOCK3)) {
    long_ps_.update(segment_address * 4, getUint8(group.get(BLOCK3), 8),
                    getUint8(group.get(BLOCK3), 0));
  }
  if (group.has(BLOCK4)) {
    long_ps_.update(segment_address * 4 + 2, getUint8(group.get(BLOCK4), 8),
                    getUint8(group.get(BLOCK4), 0));
  }

  if ((group.has(BLOCK3) || group.has(BLOCK4)) && long_ps_.text.isComplete())
    json_["long_ps"] = rtrim(long_ps_.text.getLastCompleteString());
  else if (options_.show_partial)
    json_["partial_long_ps"] = long_ps_.text.str();
}

/* Group 15B: Fast basic tuning and switching information */
void Station::decodeType15B(const Group& group) {
  const auto block_num = group.has(BLOCK2) ? BLOCK2 : BLOCK4;

  json_["ta"]       = getBool(group.get(block_num), 4);
  json_["is_music"] = getBool(group.get(block_num), 3);
}

/* Open Data Application */
void Station::decodeODAGroup(const Group& group) {
  if (oda_app_for_group_.find(group.getType()) == oda_app_for_group_.end()) {
    json_["unknown_oda"]["raw_data"] =
        getHexString(group.get(BLOCK2) & 0b11111, 2) + " " +
        (group.has(BLOCK3) ? getHexString(group.get(BLOCK3), 4) : "----") + " " +
        (group.has(BLOCK4) ? getHexString(group.get(BLOCK4), 4) : "----");

    return;
  }

  const uint16_t oda_app_id = oda_app_for_group_[group.getType()];

  switch (oda_app_id) {
    // DAB cross-referencing
    case 0x0093: parseDAB(group); break;

    // RT+
    case 0x4BD7: parseRadioTextPlus(group, radiotext_, json_["radiotext_plus"]); break;

    // RT+ for Enhanced RadioText
    case 0x4BD8: parseRadioTextPlus(group, ert_, json_["ert_plus"]); break;

    // Enhanced RadioText (eRT)
    case 0x6552: parseEnhancedRT(group); break;

    // RDS-TMC
    case 0xCD46:
    case 0xCD47:
      if (group.has(BLOCK2) && group.has(BLOCK3) && group.has(BLOCK4))
        tmc_.receiveUserGroup(getBits<5>(group.get(BLOCK2), 0), group.get(BLOCK3),
                              group.get(BLOCK4), &json_);
      break;

    default:
      json_["unknown_oda"]["app_id"]   = getHexString(oda_app_id, 4);
      json_["unknown_oda"]["app_name"] = getAppNameString(oda_app_id);
      json_["unknown_oda"]["raw_data"] =
          getHexString(group.get(BLOCK2) & 0b11111, 2) + " " +
          (group.has(BLOCK3) ? getHexString(group.get(BLOCK3), 4) : "----") + " " +
          (group.has(BLOCK4) ? getHexString(group.get(BLOCK4), 4) : "----");
  }
}

void parseRadioTextPlus(const Group& group, RadioText& rt, nlohmann::ordered_json& json_el) {
  const bool item_toggle  = getBool(group.get(BLOCK2), 4);
  const bool item_running = getBool(group.get(BLOCK2), 3);

  if (item_toggle != rt.plus.toggle || item_running != rt.plus.item_running) {
    rt.text.clear();
    rt.plus.toggle       = item_toggle;
    rt.plus.item_running = item_running;
  }

  json_el["item_running"] = item_running;
  json_el["item_toggle"]  = item_toggle ? 1 : 0;

  const size_t num_tags = group.has(BLOCK3) ? (group.has(BLOCK4) ? 2 : 1) : 0;
  std::vector<RadioText::Plus::Tag> tags(num_tags);

  if (num_tags > 0) {
    tags[0].content_type =
        static_cast<uint16_t>(getBits<6>(group.get(BLOCK2), group.get(BLOCK3), 13));
    tags[0].start  = getBits<6>(group.get(BLOCK3), 7);
    tags[0].length = getBits<6>(group.get(BLOCK3), 1) + 1;

    if (num_tags == 2) {
      tags[1].content_type =
          static_cast<uint16_t>(getBits<6>(group.get(BLOCK3), group.get(BLOCK4), 11));
      tags[1].start  = getBits<6>(group.get(BLOCK4), 5);
      tags[1].length = getBits<5>(group.get(BLOCK4), 0) + 1;
    }
  }

  for (const auto& tag : tags) {
    const std::string text = rt.text.getLastCompleteString(tag.start, tag.length);

    if (text.length() > 0 && tag.content_type != 0) {
      nlohmann::ordered_json tag_json;
      tag_json["content-type"] = getRTPlusContentTypeString(tag.content_type);
      tag_json["data"]         = text;
      json_el["tags"].push_back(tag_json);
    }
  }
}

void Station::parseEnhancedRT(const Group& group) {
  const size_t position = getBits<5>(group.get(BLOCK2), 0) * 4;

  ert_.update(position, getUint8(group.get(BLOCK3), 8), getUint8(group.get(BLOCK3), 0));

  if (group.has(BLOCK4)) {
    ert_.update(position + 2, getUint8(group.get(BLOCK4), 8), getUint8(group.get(BLOCK4), 0));
  }

  if (ert_.text.isComplete()) {
    json_["enhanced_radiotext"] = rtrim(ert_.text.getLastCompleteString());
  }
}

// ETSI EN 301 700 V1.1.1 (2000-03)
void Station::parseDAB(const Group& group) {
  const bool es_flag = getBool(group.get(BLOCK2), 4);

  if (es_flag) {
    // Service table
    json_["debug"].push_back("TODO: DAB service table");

  } else {
    // Ensemble table

    const auto mode = getBits<2>(group.get(BLOCK2), 2);
    const std::array<std::string, 4> modes{"unspecified", "I", "II or III", "IV"};
    json_["dab"]["mode"] = modes[mode];

    const uint32_t freq = 16 * getBits<18>(group.get(BLOCK2), group.get(BLOCK3), 0);

    json_["dab"]["kilohertz"] = freq;

    const std::map<uint32_t, std::string> dab_channels({
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
        {1490'624,  "LW"}
        // clang-format on
    });

    if (dab_channels.find(freq) != dab_channels.end()) {
      json_["dab"]["channel"] = dab_channels.at(freq);
    }

    json_["dab"]["ensemble_id"] = getPrefixedHexString(group.get(BLOCK4), 4);
  }
}

void Pager::decode1ABlock4(uint16_t block4) {
  const int sub_type = getBits<1>(block4, 10);
  if (sub_type == 0) {
    pac = getBits<6>(block4, 4);
    opc = getBits<4>(block4, 0);
  } else if (sub_type == 1) {
    const int sub_usage = getBits<2>(block4, 8);
    if (sub_usage == 0)
      ecc = getBits<6>(block4, 0);
    else if (sub_usage == 3)
      ccf = getBits<4>(block4, 0);
  }
}

}  // namespace redsea
