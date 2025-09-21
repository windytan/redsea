#include "src/decode/decode.hh"

#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <exception>
#include <set>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "src/freq.hh"
#include "src/group.hh"
#include "src/io/tree.hh"
#include "src/rft/crc.hh"
#include "src/rft/rft.hh"
#include "src/simplemap.hh"
#include "src/tables.hh"
#include "src/text/radiotext.hh"
#include "src/text/stringutil.hh"
#include "src/util.hh"

namespace redsea {

// Programme Item Number (IEC 62106:2015, section 6.1.5.2)
bool decodePIN(std::uint16_t pin, ObjectTree& tree) {
  const std::uint16_t day    = getBits(pin, 11, 5);
  const std::uint16_t hour   = getBits(pin, 6, 5);
  const std::uint16_t minute = getBits(pin, 0, 6);
  if (day >= 1 && hour <= 24 && minute <= 59) {
    tree["prog_item_number"]          = pin;
    tree["prog_item_started"]["day"]  = day;
    tree["prog_item_started"]["time"] = getHoursMinutesString(hour, minute);
    return true;
  } else {
    return false;
  }
}

// Decode basic information common to (almost) all groups
void decodeBasics(const Group& group, ObjectTree& tree, bool rbds) {
  if (group.getType().version == GroupType::Version::C) {
    tree["group"] = "C";
  } else if (group.has(BLOCK2)) {
    const std::uint16_t pty = getBits(group.get(BLOCK2), 5, 5);

    if (group.hasType())
      tree["group"] = group.getType().str();

    tree["tp"] = getBool(group.get(BLOCK2), 10);

    tree["prog_type"] = rbds ? getPTYNameStringRBDS(pty) : getPTYNameString(pty);
  } else if (group.getType().number == 15 && group.getType().version == GroupType::Version::B &&
             group.has(BLOCK4)) {
    const std::uint16_t pty = getBits(group.get(BLOCK4), 5, 5);

    tree["group"] = group.getType().str();

    tree["tp"]        = getBool(group.get(BLOCK4), 10);
    tree["prog_type"] = rbds ? getPTYNameStringRBDS(pty) : getPTYNameString(pty);
  }
}

// Group 0: Basic tuning and switching information
void decodeType0(const Group& group, ObjectTree& tree, AltFreqList& alt_freq_list,
                 ProgramServiceName& ps, bool show_partial) {
  // Block 2: Flags
  const std::uint16_t segment_address          = getBits(group.get(BLOCK2), 0, 2);
  const bool is_di                             = getBool(group.get(BLOCK2), 2);
  tree["di"][getDICodeString(segment_address)] = is_di;
  tree["ta"]                                   = getBool(group.get(BLOCK2), 4);
  tree["is_music"]                             = getBool(group.get(BLOCK2), 3);

  if (!group.has(BLOCK3)) {
    // Reset a Method B list to prevent mixing up different lists
    if (alt_freq_list.isMethodB())
      alt_freq_list.clear();
    return;
  }

  // Block 3: Alternative frequencies
  if (group.getType().version == GroupType::Version::A) {
    alt_freq_list.insert(getUint8(group.get(BLOCK3), 8));
    alt_freq_list.insert(getUint8(group.get(BLOCK3), 0));

    if (alt_freq_list.isComplete()) {
      const auto raw_frequencies = alt_freq_list.getRawList();

      // AF Method B sends longer lists with possible regional variants
      if (alt_freq_list.isMethodB()) {
        const int tuned_frequency = raw_frequencies[0];

        // We use std::sets for detecting duplicates
        std::set<int> unique_alternative_frequencies;
        std::set<int> unique_regional_variants;
        std::vector<int> alternative_frequencies;
        std::vector<int> regional_variants;

        // Frequency pairs
        for (std::size_t i = 1; i < raw_frequencies.size(); i += 2) {
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
          tree["alt_frequencies_b"]["tuned_frequency"] = tuned_frequency;

          for (const int frequency : alternative_frequencies)
            tree["alt_frequencies_b"]["same_programme"].push_back(frequency);

          for (const int frequency : regional_variants)
            tree["alt_frequencies_b"]["regional_variants"].push_back(frequency);
        }
      } else {
        // AF Method A is a simple list
        for (const int frequency : raw_frequencies) tree["alt_frequencies_a"].push_back(frequency);
      }

      alt_freq_list.clear();

    } else if (show_partial) {
      // If partial list is requested we'll print the raw list and not attempt to
      // deduce whether it's Method A or B
      for (const int f : alt_freq_list.getRawList()) tree["partial_alt_frequencies"].push_back(f);
    }
  }

  if (!group.has(BLOCK4))
    return;

  // Block 4: Program Service Name
  ps.update(segment_address * 2, getUint8(group.get(BLOCK4), 8), getUint8(group.get(BLOCK4), 0));

  if (ps.text.isComplete()) {
    // False maybe-uninitialized warning with gcc 13 - compiler bug?
    // https://gcc.gnu.org/bugzilla/show_bug.cgi?id=114592
#if defined(__clang__)
#pragma clang diagnostic ignored "-Wunknown-warning-option"
#endif
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
    tree["ps"] = ps.text.getLastCompleteString();
#pragma GCC diagnostic pop

  } else if (show_partial) {
    try {
      tree["partial_ps"] = ps.text.str();
    } catch (const std::exception& e) {
      tree["debug"].push_back(e.what());
      return;
    }
  }
}

// Group 1: Programme Item Number and slow labelling codes
void decodeType1(const Group& group, ObjectTree& tree, SlowLabelingCodes& slc, std::uint16_t pi) {
  if (!(group.has(BLOCK3) && group.has(BLOCK4)))
    return;

  slc.pin = group.get(BLOCK4);

  if (slc.pin != 0x0000 && !decodePIN(slc.pin, tree)) {
    tree["debug"].push_back("invalid PIN");
  }

  if (group.getType().version == GroupType::Version::A) {
    slc.linkage_la      = getBool(group.get(BLOCK3), 15);
    tree["has_linkage"] = slc.linkage_la;

    const int slow_label_variant = getBits(group.get(BLOCK3), 12, 3);

    switch (slow_label_variant) {
      case 0:
        slc.ecc = getUint8(group.get(BLOCK3), 0);
        slc.cc  = getBits(pi, 12, 4);

        if (slc.ecc != 0x00) {
          slc.has_country = true;
          tree["country"] = getCountryString(slc.cc, slc.ecc);
        }
        break;

      case 1:
        slc.tmc_id     = getBits(group.get(BLOCK3), 0, 12);
        tree["tmc_id"] = slc.tmc_id;
        break;

      case 2:
        // Pager is not implemented
        break;

      case 3:
        tree["language"] = getLanguageString(getUint8(group.get(BLOCK3), 0));
        break;

        // SLC variants 4, 5 are not assigned

      case 6:
        tree["slc_broadcaster_bits"] =
            std::string{"0x"} + getHexString(getBits(group.get(BLOCK3), 0, 11), 3);
        break;

      case 7: tree["ews"] = getBits(group.get(BLOCK3), 0, 12); break;

      default:
        tree["debug"].push_back("TODO: SLC variant " + std::to_string(slow_label_variant));
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
void decodeType2(const Group& group, ObjectTree& tree, RadioText& radiotext, bool show_partial) {
  if (!(group.has(BLOCK3) && group.has(BLOCK4)))
    return;

  const size_t radiotext_position = getBits(group.get(BLOCK2), 0, 4) *
                                    (group.getType().version == GroupType::Version::A ? 4UL : 2UL);

  const bool ab            = getBool(group.get(BLOCK2), 4);
  const bool is_ab_changed = radiotext.isABChanged(ab);

  if (show_partial) {
    tree["rt_ab"] = ab ? "B" : "A";
  }

  // If these heuristics match it's possible that we just received a full random-length message
  // with no string terminator (method 3 above).
  std::string potentially_complete_message;
  bool has_potentially_complete_message =
      radiotext_position == 0 && radiotext.text.getReceivedLength() > 1 &&
      not radiotext.text.isComplete() && not radiotext.text.hasPreviouslyReceivedTerminators();

  if (has_potentially_complete_message) {
    try {
      potentially_complete_message = rtrim(radiotext.text.str());
    } catch (const std::exception& e) {
      tree["debug"].push_back(e.what());
      return;
    }

    // No, perhaps we just lost the terminator in noise [could we use the actual BLER figure?],
    // or maybe the message got interrupted by an A/B change. Let's wait for a repeat.
    if (potentially_complete_message != radiotext.previous_potentially_complete_message) {
      has_potentially_complete_message = false;
    }
    radiotext.previous_potentially_complete_message = potentially_complete_message;
  }

  // The transmitter requests us to clear the buffer (message contents will change).
  // Note: This is sometimes overused in the wild.
  if (is_ab_changed) {
    radiotext.text.clear();
  }

  if (group.getType().version == GroupType::Version::A) {
    radiotext.text.resize(64);
    radiotext.update(radiotext_position, getUint8(group.get(BLOCK3), 8),
                     getUint8(group.get(BLOCK3), 0));
  } else {
    radiotext.text.resize(32);
  }

  if (group.has(BLOCK4)) {
    radiotext.update(
        radiotext_position + (group.getType().version == GroupType::Version::A ? 2 : 0),
        getUint8(group.get(BLOCK4), 8), getUint8(group.get(BLOCK4), 0));
  }

  // Transmitter used Method 1 or 2 convey the length of the string.
  if (radiotext.text.isComplete()) {
    tree["radiotext"] = rtrim(radiotext.text.getLastCompleteString());

    // Method 3 was used instead (and was confirmed by a repeat).
  } else if (has_potentially_complete_message) {
    tree["radiotext"] = rtrim(std::move(potentially_complete_message));

    // The string is not complete yet, but user wants to see it anyway.
  } else if (show_partial) {
    try {
      if (rtrim(radiotext.text.str()).length() > 0) {
        tree["partial_radiotext"] = radiotext.text.str();
      }
    } catch (const std::exception& e) {
      tree["debug"].push_back(e.what());
      return;
    }
  }
}

// Group 4A: Clock-time and date
void decodeType4A(const Group& group, ObjectTree& tree) {
  if (!(group.has(BLOCK3) && group.has(BLOCK4)))
    return;

  const double modified_julian_date{
      static_cast<double>(getBits(group.get(BLOCK2), group.get(BLOCK3), 1, 17))};

  // Would result in negative years/months
  if (modified_julian_date < 15079.0) {
    tree["debug"].push_back("invalid date/time");
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

  const auto hour_utc   = getBits(group.get(BLOCK3), group.get(BLOCK4), 12, 5);
  const auto minute_utc = getBits(group.get(BLOCK4), 6, 6);

  const double local_offset =
      (getBool(group.get(BLOCK4), 5) ? -1.0 : 1.0) * getBits(group.get(BLOCK4), 0, 5) / 2.0;

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
      hour_utc <= 23 && minute_utc <= 59 && std::abs(std::trunc(local_offset)) <= 14.0;
  if (is_date_valid) {
    std::array<char, 100> buffer{};
    const int local_offset_hour = static_cast<int>(std::abs(std::trunc(local_offset)));
    const int local_offset_min = static_cast<int>((local_offset - std::trunc(local_offset)) * 60.0);

    if (local_offset_hour == 0 && local_offset_min == 0) {
      std::ignore = std::snprintf(buffer.data(), buffer.size(), "%04d-%02d-%02dT%02d:%02d:00Z",
                                  local_tm->tm_year + 1900, local_tm->tm_mon + 1, local_tm->tm_mday,
                                  local_tm->tm_hour, local_tm->tm_min);
    } else {
      std::ignore =
          std::snprintf(buffer.data(), buffer.size(), "%04d-%02d-%02dT%02d:%02d:00%s%02d:%02d",
                        local_tm->tm_year + 1900, local_tm->tm_mon + 1, local_tm->tm_mday,
                        local_tm->tm_hour, local_tm->tm_min, local_offset > 0 ? "+" : "-",
                        local_offset_hour, std::abs(local_offset_min));
    }
    tree["clock_time"] = std::string(buffer.data());
  } else {
    tree["debug"].push_back("invalid date/time");
  }
}

// Group 5: Transparent data channels
void decodeType5(const Group& group, ObjectTree& tree, RDSString& full_tdc) {
  const auto address                  = getBits(group.get(BLOCK2), 0, 5);
  tree["transparent_data"]["address"] = address;

  if (group.getType().version == GroupType::Version::A) {
    const std::array<std::uint8_t, 4> data{
        getUint8(group.get(BLOCK3), 8), getUint8(group.get(BLOCK3), 0),
        getUint8(group.get(BLOCK4), 8), getUint8(group.get(BLOCK4), 0)};

    tree["transparent_data"]["raw"] = getHexString(data[0], 2) + " " + getHexString(data[1], 2) +
                                      " " + getHexString(data[2], 2) + " " +
                                      getHexString(data[3], 2);

    RDSString decoded_text(4);
    decoded_text.set(0, data[0], data[1]);
    decoded_text.set(2, data[2], data[3]);

    full_tdc.set(address * 4U, data[0], data[1]);
    full_tdc.set(address * 4U + 2U, data[2], data[3]);
    if (full_tdc.isComplete()) {
      tree["transparent_data"]["full_text"] = full_tdc.str();

      std::vector<std::string> full_raw;
      full_raw.reserve(full_tdc.getData().size());
      for (const auto b : full_tdc.getData()) {
        full_raw.push_back(getHexString(b, 2));
      }
      tree["transparent_data"]["full_raw"] = join(full_raw, " ");
    }

    tree["transparent_data"]["as_text"] = decoded_text.str();
  } else {
    const std::array<std::uint8_t, 2> data{getUint8(group.get(BLOCK4), 8),
                                           getUint8(group.get(BLOCK4), 0)};

    tree["transparent_data"]["raw"] = getHexString(data[0], 2) + " " + getHexString(data[1], 2);

    RDSString decoded_text(2);
    decoded_text.set(0, data[0], data[1]);
    tree["transparent_data"]["as_text"] = decoded_text.str();
  }
}

// Group 6: In-house applications
void decodeType6(const Group& group, ObjectTree& tree) {
  tree["in_house_data"].push_back(getBits(group.get(BLOCK2), 0, 5));

  if (group.getType().version == GroupType::Version::A) {
    if (group.has(BLOCK3)) {
      tree["in_house_data"].push_back(getBits(group.get(BLOCK3), 0, 16));
      if (group.has(BLOCK4)) {
        tree["in_house_data"].push_back(getBits(group.get(BLOCK4), 0, 16));
      }
    }
  } else {
    if (group.has(BLOCK4)) {
      tree["in_house_data"].push_back(getBits(group.get(BLOCK4), 0, 16));
    }
  }
}

// Group 7A: Radio Paging
void decodeType7A(const Group& group, ObjectTree& tree) {
  static_cast<void>(group);
  tree["debug"].push_back("TODO: 7A");
}

// Group 9A: Emergency warning systems
void decodeType9A(const Group& group, ObjectTree& tree) {
  static_cast<void>(group);
  tree["debug"].push_back("TODO: 9A");
}

// Group 10A: Programme Type Name
void decodeType10A(const Group& group, ObjectTree& tree, PTYName& ptyname) {
  if (!group.has(BLOCK3) || !group.has(BLOCK4))
    return;

  const std::uint16_t segment_address = getBits(group.get(BLOCK2), 0, 1);

  if (ptyname.isABChanged(getBits(group.get(BLOCK2), 4, 1)))
    ptyname.text.clear();

  ptyname.update(segment_address * 4U, getUint8(group.get(BLOCK3), 8),
                 getUint8(group.get(BLOCK3), 0), getUint8(group.get(BLOCK4), 8),
                 getUint8(group.get(BLOCK4), 0));

  if (ptyname.text.isComplete()) {
    tree["pty_name"] = ptyname.text.getLastCompleteString();
  }
}

// Group 14: Enhanced Other Networks information
void decodeType14(const Group& group, ObjectTree& tree,
                  SimpleMap<std::uint16_t, RDSString>& eon_ps_names,
                  SimpleMap<std::uint16_t, AltFreqList>& eon_alt_freqs, bool rbds) {
  if (!(group.has(BLOCK4)))
    return;

  const std::uint16_t on_pi   = group.get(BLOCK4);
  tree["other_network"]["pi"] = getPrefixedHexString(on_pi, 4);
  tree["other_network"]["tp"] = getBool(group.get(BLOCK2), 4);

  if (group.getType().version == GroupType::Version::B) {
    tree["other_network"]["ta"] = getBool(group.get(BLOCK2), 3);
    return;
  }

  if (!group.has(BLOCK3))
    return;

  const std::uint16_t eon_variant = getBits(group.get(BLOCK2), 0, 4);
  switch (eon_variant) {
    case 0:
    case 1:
    case 2:
    case 3: {
      if (!eon_ps_names.contains(on_pi))
        eon_ps_names.insert(on_pi, RDSString(8));

      auto& ps_name = eon_ps_names.at(on_pi);

      ps_name.set(2U * eon_variant, getUint8(group.get(BLOCK3), 8));
      ps_name.set(2U * eon_variant + 1, getUint8(group.get(BLOCK3), 0));

      if (ps_name.isComplete())
        tree["other_network"]["ps"] = ps_name.getLastCompleteString();
      break;
    }

    case 4: {
      if (!eon_alt_freqs.contains(on_pi))
        eon_alt_freqs.insert(on_pi, AltFreqList());

      auto& list = eon_alt_freqs.at(on_pi);

      list.insert(getUint8(group.get(BLOCK3), 8));
      list.insert(getUint8(group.get(BLOCK3), 0));

      if (list.isComplete()) {
        for (const int freq : list.getRawList())
          tree["other_network"]["alt_frequencies"].push_back(freq);
        list.clear();
      }
      break;
    }

    case 5:
    case 6:
    case 7:
    case 8:
    case 9: {
      const CarrierFrequency freq_other(getUint8(group.get(BLOCK3), 0));

      if (freq_other.isValid())
        tree["other_network"]["kilohertz"] = freq_other.kHz();

      break;
    }

      // 10, 11 unallocated

    case 12: {
      const bool has_linkage               = getBool(group.get(BLOCK3), 15);
      const std::uint16_t lsn              = getBits(group.get(BLOCK3), 0, 12);
      tree["other_network"]["has_linkage"] = has_linkage;
      if (has_linkage && lsn != 0)
        tree["other_network"]["linkage_set"] = lsn;
      break;
    }

    case 13: {
      const std::uint16_t pty            = getBits(group.get(BLOCK3), 11, 5);
      const bool ta                      = getBool(group.get(BLOCK3), 0);
      tree["other_network"]["prog_type"] = rbds ? getPTYNameStringRBDS(pty) : getPTYNameString(pty);
      tree["other_network"]["ta"]        = ta;
      break;
    }

    case 14: {
      const std::uint16_t pin = group.get(BLOCK3);

      if (pin != 0x0000)
        decodePIN(pin, tree["other_network"]);
      break;
    }

    case 15: {
      tree["other_network"]["broadcaster_data"] = getHexString(group.get(BLOCK3), 4);
      break;
    }

    default:
      tree["debug"].push_back("TODO: EON variant " +
                              std::to_string(getBits(group.get(BLOCK2), 0, 4)));
      break;
  }
}

/* Group 15A: Long PS or ODA */
// @note Based on captures and https://www.rds.org.uk/2010/Glossary-Of-Terms.htm
void decodeType15A(const Group& group, ObjectTree& tree, LongPS& long_ps, bool show_partial) {
  const std::uint32_t segment_address = getBits(group.get(BLOCK2), 0, 3);

  if (group.has(BLOCK3)) {
    long_ps.update(segment_address * 4U, getUint8(group.get(BLOCK3), 8),
                   getUint8(group.get(BLOCK3), 0));
  }
  if (group.has(BLOCK4)) {
    long_ps.update(segment_address * 4U + 2U, getUint8(group.get(BLOCK4), 8),
                   getUint8(group.get(BLOCK4), 0));
  }

  if ((group.has(BLOCK3) || group.has(BLOCK4)) && long_ps.text.isComplete()) {
    tree["long_ps"] = rtrim(long_ps.text.getLastCompleteString());
  } else if (show_partial) {
    try {
      // May throw if UCS-2
      tree["partial_long_ps"] = long_ps.text.str();
    } catch (const std::exception& e) {
      tree["debug"].push_back(e.what());
      return;
    }
  }
}

/* Group 15B: Fast basic tuning and switching information */
void decodeType15B(const Group& group, ObjectTree& tree) {
  const auto block_num                = group.has(BLOCK2) ? BLOCK2 : BLOCK4;
  const std::uint16_t segment_address = getBits(group.get(BLOCK2), 0, 2);
  const bool is_di                    = getBool(group.get(BLOCK2), 2);

  tree["di"][getDICodeString(segment_address)] = is_di;
  tree["ta"]                                   = getBool(group.get(block_num), 4);
  tree["is_music"]                             = getBool(group.get(block_num), 3);
}

// Type C groups (only transmitted on data streams 1-3)
void decodeC(const Group& group, ObjectTree& tree,
             SimpleMap<std::uint16_t, std::uint16_t>& oda_app_for_pipe,
             std::array<RFTFile, 16>& rft_file) {
  if (!group.has(BLOCK1) || !group.has(BLOCK2) || !group.has(BLOCK3) || !group.has(BLOCK4))
    return;

  const std::uint32_t fid = getBits(group.get(BLOCK1), 14, 2);
  const std::uint32_t fn  = getBits(group.get(BLOCK1), 8, 6);

  if (fid == 0 && fn == 0) {
    // Page 47: Legacy type A & B transmission
    tree["debug"].push_back("TODO: Tunnelling A & B over type C");
  } else if (fid == 0 && (fn & 0b11'0000U) == 0b10'0000U) {
    // Page 82: RFT data group for ODA pipe (fn & 0b1111)
    const auto pipe            = static_cast<int>(fn & 0b1111U);
    const auto toggle_bit      = getBits(group.get(BLOCK1), 7, 1);
    const auto segment_address = getBits(group.get(BLOCK1), group.get(BLOCK2), 8, 15);
    if (oda_app_for_pipe.contains(pipe)) {
      tree["open_data_app"]["app_name"] = getAppNameString(oda_app_for_pipe.at(pipe));
    }
    tree["rft"]["data"]["pipe"]         = pipe;
    tree["rft"]["data"]["toggle"]       = toggle_bit;
    tree["rft"]["data"]["byte_address"] = segment_address * 5;

    rft_file[pipe].receive(toggle_bit, segment_address, group.get(BLOCK2), group.get(BLOCK3),
                           group.get(BLOCK4));

    tree["rft"]["data"]["segment_data"].push_back(getBits(group.get(BLOCK2), 0, 8));
    tree["rft"]["data"]["segment_data"].push_back(getBits(group.get(BLOCK3), 8, 8));
    tree["rft"]["data"]["segment_data"].push_back(getBits(group.get(BLOCK3), 0, 8));
    tree["rft"]["data"]["segment_data"].push_back(getBits(group.get(BLOCK4), 8, 8));
    tree["rft"]["data"]["segment_data"].push_back(getBits(group.get(BLOCK4), 0, 8));

    if (rft_file[pipe].hasNewCompleteFile()) {
      tree["rft"]["data"]["file_contents"] = rft_file[pipe].getBase64Data();
      if (rft_file[pipe].isCRCExpected()) {
        // TODO Let's test and enable this when we have real-world data
        // tree["rft"]["data"]["crc_ok"] = rft_file_[pipe].checkCRC();
      }
      rft_file[pipe].clear();
    }

  } else if (fid == 1) {
    // Page 47
    // Group type C ODA channel fn
    // Channels 0-15 are reserved for providing additional data
    tree["open_data_app"]["channel"] = fn;
    tree["open_data_app"]["app_data"].push_back(getBits(group.get(BLOCK1), 0, 8));
    tree["open_data_app"]["app_data"].push_back(getBits(group.get(BLOCK2), 8, 8));
    tree["open_data_app"]["app_data"].push_back(getBits(group.get(BLOCK2), 0, 8));
    tree["open_data_app"]["app_data"].push_back(getBits(group.get(BLOCK3), 8, 8));
    tree["open_data_app"]["app_data"].push_back(getBits(group.get(BLOCK3), 0, 8));
    tree["open_data_app"]["app_data"].push_back(getBits(group.get(BLOCK4), 8, 8));
    tree["open_data_app"]["app_data"].push_back(getBits(group.get(BLOCK4), 0, 8));
  } else if (fid == 0b10 && fn == 0b000000) {
    // Page 48
    // AID and channel number assignment for group type C ODAs

    const int ass_method = getBits(group.get(BLOCK1), 6, 2) + 1;
    const int channel_id = getBits(group.get(BLOCK1), 0, 6);
    if (ass_method == 1) {
      tree["open_data_app"]["channel"]  = channel_id;
      tree["open_data_app"]["oda_aid"]  = group.get(BLOCK2);
      tree["open_data_app"]["app_name"] = getAppNameString(group.get(BLOCK2));
      oda_app_for_pipe.insert(channel_id, group.get(BLOCK2));

      const bool is_rft = channel_id < 16;

      if (is_rft) {
        // RFT: Page 79
        const int variant = getBits(group.get(BLOCK3), 12, 4);
        if (variant == 0) {
          const bool crc_flag            = getBool(group.get(BLOCK3), 11);
          const auto file_version        = getBits(group.get(BLOCK3), 8, 3);
          const auto file_identification = getBits(group.get(BLOCK3), 2, 6);
          const auto file_size_bytes     = getBits(group.get(BLOCK3), group.get(BLOCK4), 0, 18);

          rft_file[channel_id].setSize(file_size_bytes);
          rft_file[channel_id].setCRCFlag(crc_flag);

          tree["rft"]["file_info"]["version"] = file_version;
          tree["rft"]["file_info"]["id"]      = file_identification;
          tree["rft"]["file_info"]["size"]    = file_size_bytes;
          tree["rft"]["file_info"]["has_crc"] = crc_flag;
        } else if (variant == 1) {
          // CRC (Page 80)
          const auto crc_mode      = getBits(group.get(BLOCK3), 9, 3);
          const auto chunk_address = getBits(group.get(BLOCK3), 0, 9);
          const auto crc           = group.get(BLOCK4);

          const ChunkCRC chunk_crc{crc_mode, chunk_address, crc};
          rft_file[channel_id].receiveCRC(chunk_crc);

          switch (crc_mode) {
            case 0: tree["rft"]["crc_info"]["file_crc16"] = crc; break;
            case 1:
            case 2:
            case 3:
            case 4:
            case 5:
            case 7:
              tree["rft"]["crc_info"]["chunk_crc16"]   = chunk_crc.crc;
              tree["rft"]["crc_info"]["chunk_address"] = chunk_crc.address_raw;
              tree["rft"]["crc_info"]["crc_mode"]      = chunk_crc.mode;
              break;
            default: tree["debug"].push_back("TODO: CRC mode " + std::to_string(crc_mode)); break;
          }

        } else if (variant >= 8) {
          // 8..15: Non-file-related ODA data
          tree["open_data_app"]["non_file_oda_data"] =
              getHexString(getBits(group.get(BLOCK3), group.get(BLOCK4), 0, 28), 7);
        } else if (variant >= 2) {
          // File-related ODA data
          tree["open_data_app"]["file_oda_data"] =
              getHexString(getBits(group.get(BLOCK3), group.get(BLOCK4), 0, 28), 7);
        }
      } else {
        tree["open_data_app"]["app_data"].push_back(getBits(group.get(BLOCK3), 8, 8));
        tree["open_data_app"]["app_data"].push_back(getBits(group.get(BLOCK3), 0, 8));
        tree["open_data_app"]["app_data"].push_back(getBits(group.get(BLOCK4), 8, 8));
        tree["open_data_app"]["app_data"].push_back(getBits(group.get(BLOCK4), 0, 8));
      }

    } else {
      tree["debug"].push_back("TODO: assignment method " + std::to_string(ass_method));
    }
  } else {
    tree["debug"].push_back("TODO: FID " + std::to_string(fid) + " FN " + std::to_string(fn));
    tree["open_data_app"]["data"].push_back(getBits(group.get(BLOCK2), 8, 8));
    tree["open_data_app"]["data"].push_back(getBits(group.get(BLOCK2), 0, 8));
    tree["open_data_app"]["data"].push_back(getBits(group.get(BLOCK3), 8, 8));
    tree["open_data_app"]["data"].push_back(getBits(group.get(BLOCK3), 0, 8));
    tree["open_data_app"]["data"].push_back(getBits(group.get(BLOCK4), 8, 8));
    tree["open_data_app"]["data"].push_back(getBits(group.get(BLOCK4), 0, 8));
  }
}

}  // namespace redsea
