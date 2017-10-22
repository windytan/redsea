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

#include <cmath>
#include <iomanip>
#include <iostream>
#include <map>
#include <string>
#include <sstream>
#include <vector>

#include <json/json.h>

#include "config.h"
#include "src/common.h"
#include "src/rdsstring.h"
#include "src/tables.h"
#include "src/util.h"

namespace redsea {

std::string TimePointToString(
    const std::chrono::time_point<std::chrono::system_clock>& timepoint,
    const std::string& format) {
  std::time_t t = std::chrono::system_clock::to_time_t(timepoint);
  std::tm tm = *std::localtime(&t);

  std::string result;
  char buffer[64];
  if (strftime(buffer, sizeof(buffer), format.c_str(), &tm) > 0) {
    result = std::string(buffer);
  } else {
    result = "(format error)";
  }

  return result;
}

void PrintHexGroup(const Group& group, std::ostream* stream,
                   const std::string& time_format) {
  stream->fill('0');
  stream->setf(std::ios_base::uppercase);

  if (group.empty())
    return;

  for (eBlockNumber block_num : {BLOCK1, BLOCK2, BLOCK3, BLOCK4}) {
    if (group.has(block_num))
      *stream << std::hex << std::setw(4) << group.block(block_num);
    else
      *stream << "----";

    if (block_num != BLOCK4)
      *stream << " ";
  }

  if (group.has_time())
    *stream << " " << TimePointToString(group.rx_time(), time_format);

  *stream << '\n';
}

GroupType::GroupType(uint16_t type_code) :
  number((type_code >> 1) & 0xF),
  version((type_code & 0x1) == 0 ? VERSION_A : VERSION_B) {}

std::string GroupType::str() const {
  return std::string(std::to_string(number) +
         (version == VERSION_A ? "A" : "B"));
}

bool operator==(const GroupType& type1, const GroupType& type2) {
  return type1.number == type2.number && type1.version == type2.version;
}

bool operator<(const GroupType& type1, const GroupType& type2) {
  return (type1.number < type2.number) ||
         (type1.number == type2.number && type1.version < type2.version);
}

Group::Group() : pi_(0x0000), has_block_({false, false, false, false, false}),
                 block_(5), block_had_errors_(4), bler_(0), has_type_(false),
                 has_pi_(false), has_c_prime_(false), has_bler_(false),
                 has_time_(false), no_offsets_(false) {
}

uint16_t Group::block(eBlockNumber block_num) const {
  return block_.at(block_num);
}

bool Group::has(eBlockNumber block_num) const {
  return has_block_.at(block_num);
}

bool Group::empty() const {
  return !(has(BLOCK1) || has(BLOCK2) || has(BLOCK3) || has(BLOCK4));
}

uint16_t Group::pi() const {
  return pi_;
}

uint8_t Group::bler() const {
  return bler_;
}

uint8_t Group::num_errors() const {
  int n_err = 0;
  for (eBlockNumber b : {BLOCK1, BLOCK2, BLOCK3, BLOCK4})
    n_err += (block_had_errors_[b] || !has(b));
  return n_err;
}

bool Group::has_pi() const {
  return has_pi_;
}

GroupType Group::type() const {
  return type_;
}

bool Group::has_type() const {
  return has_type_;
}

bool Group::has_bler() const {
  return has_bler_;
}

bool Group::has_time() const {
  return has_time_;
}

std::chrono::time_point<std::chrono::system_clock> Group::rx_time() const {
  return time_received_;
}

void Group::disable_offsets() {
  no_offsets_ = true;
}

void Group::set(eBlockNumber block_num, uint16_t data, bool had_errors) {
  block_[block_num] = data;
  has_block_[block_num] = true;
  block_had_errors_[block_num] = had_errors;

  if (block_num == BLOCK1) {
    pi_ = data;
    has_pi_ = true;
  } else if (block_num == BLOCK2) {
    type_ = GroupType(Bits(data, 11, 5));
    if (type_.version == VERSION_A)
      has_type_ = true;
    else
      has_type_ = (has_c_prime_ || no_offsets_);
  } else if (block_num == BLOCK3 && has_c_prime_ && !has_pi_) {
    pi_ = data;
    has_pi_ = true;
  } else if (block_num == BLOCK4 && has_c_prime_ && !has_type_) {
    GroupType potential_type(Bits(data, 11, 5));
    if (potential_type.number == 15 && potential_type.version == VERSION_B) {
      type_ = potential_type;
      has_type_ = true;
    }
  }
}

void Group::set_c_prime(uint16_t data, bool had_errors) {
  has_c_prime_ = true;
  block_had_errors_[BLOCK3] = had_errors;

  set(BLOCK3, data);
  if (has(BLOCK2))
    has_type_ = (type_.version == VERSION_B);
}

void Group::set_time(std::chrono::time_point<std::chrono::system_clock> t) {
  time_received_ = t;
  has_time_ = true;
}

void Group::set_bler(uint8_t bler) {
  bler_ = bler;
  has_bler_ = true;
}

Station::Station() : Station(0x0000, Options()) {
}

Station::Station(uint16_t _pi, const Options& options) : pi_(_pi),
  options_(options), ps_(8), radiotext_(64), radiotext_ab_(0), pty_(0),
  is_tp_(false), is_ta_(false), is_music_(false), pin_(0), ecc_(0), cc_(0),
  tmc_id_(0), ews_channel_(0), lang_(0), linkage_la_(0), clock_time_(""),
  has_country_(false), oda_app_for_group_(), has_radiotext_plus_(false),
  radiotext_plus_cb_(false), radiotext_plus_scb_(0),
  radiotext_plus_template_num_(0),
  radiotext_plus_toggle_(false), radiotext_plus_item_running_(false),
  last_block_had_pi_(false), alt_freq_list_(), pager_pac_(0), pager_opc_(0),
  pager_tng_(0), pager_ecc_(0), pager_ccf_(0), pager_interval_(0),
  writer_builder_(), json_()
#ifdef ENABLE_TMC
                    , tmc_(options)
#endif
{
  writer_builder_["indentation"] = "";
  writer_builder_["precision"] = 7;
  writer_ =
      std::unique_ptr<Json::StreamWriter>(writer_builder_.newStreamWriter());
}

void Station::UpdateAndPrint(const Group& group, std::ostream* stream) {
  if (group.empty())
    return;

  // Allow 1 group with missed PI
  if (group.has_pi()) {
    last_block_had_pi_ = true;
  } else if (!last_block_had_pi_) {
    return;
  } else if (last_block_had_pi_) {
    last_block_had_pi_ = false;
  }

  json_.clear();
  json_["pi"] = "0x" + HexString(pi(), 4);
  if (options_.timestamp)
    json_["rx_time"] = TimePointToString(group.rx_time(), options_.time_format);

  if (group.has_bler())
    json_["bler"] = group.bler();

  DecodeBasics(group);

  if (group.has_type()) {
    if      (group.type().number == 0)
      DecodeType0(group);
    else if (group.type().number == 1)
      DecodeType1(group);
    else if (group.type().number == 2)
      DecodeType2(group);
    else if (group.type().number == 3 && group.type().version == VERSION_A)
      DecodeType3A(group);
    else if (group.type().number == 4 && group.type().version == VERSION_A)
      DecodeType4A(group);
    else if (group.type().number == 14)
      DecodeType14(group);
    else if (group.type().number == 15 && group.type().version == VERSION_B)
      DecodeType15B(group);
    else if (oda_app_for_group_.count(group.type()) > 0)
      DecodeODAGroup(group);
    else if (group.type().number == 6)
      DecodeType6(group);
    else
      json_["debug"].append("TODO " + group.type().str());
  }

  writer_->write(json_, stream);

  *stream << '\n';
}

uint16_t Station::pi() const {
  return pi_;
}

void Station::UpdatePS(int pos, int char1, int char2) {
  ps_.set(pos, RDSChar(char1), RDSChar(char2));

  if (ps_.complete())
    json_["ps"] = ps_.last_complete_string();
  else if (options_.show_partial)
    json_["partial_ps"] = ps_.str();
}

void Station::UpdateRadioText(int pos, int char1, int char2) {
  radiotext_.set(pos, RDSChar(char1), RDSChar(char2));
}

void Station::DecodeBasics(const Group& group) {
  if (group.has(BLOCK2)) {
    is_tp_ = Bits(group.block(BLOCK2), 10, 1);
    pty_   = Bits(group.block(BLOCK2),  5, 5);

    if (group.has_type())
      json_["group"] = group.type().str();
    json_["tp"] = is_tp_;
    json_["prog_type"] = PTYNameString(pty_, options_.rbds);
  } else if (group.type().number == 15 && group.type().version == VERSION_B &&
      group.has(BLOCK4)) {
    is_tp_ = Bits(group.block(BLOCK4), 10, 1);
    pty_   = Bits(group.block(BLOCK4),  5, 5);

    json_["group"] = group.type().str();
    json_["tp"] = is_tp_;
    json_["prog_type"] = PTYNameString(pty_, options_.rbds);
  }
}

// Group 0: Basic tuning and switching information
void Station::DecodeType0(const Group& group) {
  uint16_t segment_address = Bits(group.block(BLOCK2), 0, 2);
  bool is_di = Bits(group.block(BLOCK2), 2, 1);
  json_["di"][DICodeString(segment_address)] = is_di;

  is_ta_    = Bits(group.block(BLOCK2), 4, 1);
  is_music_ = Bits(group.block(BLOCK2), 3, 1);

  json_["ta"] = is_ta_;
  json_["is_music"] = is_music_;

  if (!group.has(BLOCK3))
    return;

  if (group.type().version == VERSION_A) {
    alt_freq_list_.insert(Bits(group.block(BLOCK3), 8, 8));
    alt_freq_list_.insert(Bits(group.block(BLOCK3), 0, 8));

    if (alt_freq_list_.complete()) {
      for (CarrierFrequency f : alt_freq_list_.get())
        json_["alt_kilohertz"].append(f.kHz());
      alt_freq_list_.clear();
    }
  }

  if (!group.has(BLOCK4))
    return;

  UpdatePS(segment_address * 2,
           Bits(group.block(BLOCK4), 8, 8),
           Bits(group.block(BLOCK4), 0, 8));
}

// Group 1: Programme Item Number and slow labelling codes
void Station::DecodeType1(const Group& group) {
  if (!(group.has(BLOCK3) && group.has(BLOCK4)))
    return;

  pin_ = group.block(BLOCK4);

  if (pin_ != 0x0000) {
    uint16_t day    = Bits(pin_, 11, 5);
    uint16_t hour   = Bits(pin_, 6, 5);
    uint16_t minute = Bits(pin_, 0, 6);
    if (day >= 1 && hour <= 24 && minute <= 59) {
      json_["prog_item_started"]["day"] = day;
      json_["prog_item_started"]["time"] = HoursMinutesString(hour, minute);
    } else {
      json_["debug"].append("invalid PIN");
    }
  }

  if (group.type().version == VERSION_A) {
    pager_tng_ = Bits(group.block(BLOCK2), 2, 3);
    if (pager_tng_ != 0) {
      pager_interval_ = Bits(group.block(BLOCK2), 0, 2);
    }
    linkage_la_ = Bits(group.block(BLOCK3), 15, 1);
    json_["has_linkage"] = linkage_la_;

    int slc_variant = Bits(group.block(BLOCK3), 12, 3);

    if (slc_variant == 0) {
      if (pager_tng_ != 0) {
        pager_opc_ = Bits(group.block(BLOCK3), 8, 4);
      }

      // No PIN, section M.3.2.4.3
      if (group.has(BLOCK4) && (group.block(BLOCK4) >> 11) == 0) {
        int subtype = Bits(group.block(BLOCK4), 10, 1);
        if (subtype == 0) {
          if (pager_tng_ != 0) {
            pager_pac_ = Bits(group.block(BLOCK4), 4, 6);
            pager_opc_ = Bits(group.block(BLOCK4), 0, 4);
          }
        } else if (subtype == 1) {
          if (pager_tng_ != 0) {
            int b = Bits(group.block(BLOCK4), 8, 2);
            if (b == 0) {
              pager_ecc_ = Bits(group.block(BLOCK4), 0, 6);
            } else if (b == 3) {
              pager_ccf_ = Bits(group.block(BLOCK4), 0, 4);
            }
          }
        }
      }

      ecc_ = Bits(group.block(BLOCK3),  0, 8);
      cc_  = Bits(pi_, 12, 4);

      if (ecc_ != 0x00) {
        has_country_ = true;

        json_["country"] = CountryString(pi_, ecc_);
      }

    } else if (slc_variant == 1) {
      tmc_id_ = Bits(group.block(BLOCK3), 0, 12);
      json_["tmc_id"] = tmc_id_;

    } else if (slc_variant == 2) {
      if (pager_tng_ != 0) {
        pager_pac_ = Bits(group.block(BLOCK3), 0, 6);
        pager_opc_ = Bits(group.block(BLOCK3), 8, 4);
      }

      // No PIN, section M.3.2.4.3
      if (group.has(BLOCK4) && (group.block(BLOCK4) >> 11) == 0) {
        int subtype = Bits(group.block(BLOCK4), 10, 1);
        if (subtype == 0) {
          if (pager_tng_ != 0) {
            pager_pac_ = Bits(group.block(BLOCK4), 4, 6);
            pager_opc_ = Bits(group.block(BLOCK4), 0, 4);
          }
        } else if (subtype == 1) {
          if (pager_tng_ != 0) {
            int b = Bits(group.block(BLOCK4), 8, 2);
            if (b == 0) {
              pager_ecc_ = Bits(group.block(BLOCK4), 0, 6);
            } else if (b == 3) {
              pager_ccf_ = Bits(group.block(BLOCK4), 0, 4);
            }
          }
        }
      }

    } else if (slc_variant == 3) {
      lang_ = Bits(group.block(BLOCK3), 0, 8);
      json_["language"] = LanguageString(lang_);

    } else if (slc_variant == 7) {
      ews_channel_ = Bits(group.block(BLOCK3), 0, 12);
      json_["ews"] = ews_channel_;

    } else {
      json_["debug"].append("TODO: SLC variant " +
          std::to_string(slc_variant));
    }
  }
}

// Group 2: RadioText
void Station::DecodeType2(const Group& group) {
  if (!(group.has(BLOCK3) && group.has(BLOCK4)))
    return;

  int radiotext_position = Bits(group.block(BLOCK2), 0, 4) *
    (group.type().version == VERSION_A ? 4 : 2);
  int prev_text_ab = radiotext_ab_;
  radiotext_ab_ = Bits(group.block(BLOCK2), 4, 1);

  if (prev_text_ab != radiotext_ab_)
    radiotext_.clear();

  if (group.type().version == VERSION_A) {
    radiotext_.resize(64);
    UpdateRadioText(radiotext_position,
                    Bits(group.block(BLOCK3), 8, 8),
                    Bits(group.block(BLOCK3), 0, 8));
  } else {
    radiotext_.resize(32);
  }

  if (group.has(BLOCK4)) {
    UpdateRadioText(radiotext_position +
                    (group.type().version == VERSION_A ? 2 : 0),
                    Bits(group.block(BLOCK4), 8, 8),
                    Bits(group.block(BLOCK4), 0, 8));
  }

  if (radiotext_.complete())
    json_["radiotext"] = rtrim(radiotext_.last_complete_string());
  else if (options_.show_partial && rtrim(radiotext_.str()).length() > 0)
    json_["partial_radiotext"] = rtrim(radiotext_.str());
}

// Group 3A: Application identification for Open Data
void Station::DecodeType3A(const Group& group) {
  if (!(group.has(BLOCK3) && group.has(BLOCK4)))
    return;

  if (group.type().version != VERSION_A)
    return;

  GroupType oda_group_type(Bits(group.block(BLOCK2), 0, 5));
  uint16_t oda_message = group.block(BLOCK3);
  uint16_t oda_app_id  = group.block(BLOCK4);

  oda_app_for_group_[oda_group_type] = oda_app_id;

  json_["open_data_app"]["oda_group"] = oda_group_type.str();
  json_["open_data_app"]["app_name"] = AppNameString(oda_app_id);

  if (oda_app_id == 0xCD46 || oda_app_id == 0xCD47) {
#ifdef ENABLE_TMC
    tmc_.SystemGroup(group.block(BLOCK3), &json_);
#else
    json_["debug"].append("redsea compiled without TMC support");
#endif
  } else if (oda_app_id == 0x4BD7) {
    has_radiotext_plus_ = true;
    radiotext_plus_cb_ = Bits(group.block(BLOCK3), 12, 1);
    radiotext_plus_scb_ = Bits(group.block(BLOCK3), 8, 4);
    radiotext_plus_template_num_ = Bits(group.block(BLOCK3), 0, 8);
  } else {
    json_["debug"].append("TODO: Unimplemented ODA app " +
        std::to_string(oda_app_id));
    json_["open_data_app"]["message"] = oda_message;
  }
}

// Group 4A: Clock-time and date
void Station::DecodeType4A(const Group& group) {
  if (!(group.has(BLOCK3) && group.has(BLOCK4)))
    return;

  int modified_julian_date = (Bits(group.block(BLOCK2), 0, 2) << 15) +
             Bits(group.block(BLOCK3), 1, 15);
  double local_offset = (Bits(group.block(BLOCK4), 5, 1) ? -1 : 1) *
         Bits(group.block(BLOCK4), 0, 5) / 2.0;
  modified_julian_date += local_offset / 24.0;

  int year = (modified_julian_date - 15078.2) / 365.25;
  int month = (modified_julian_date - 14956.1 -
              std::trunc(year * 365.25)) / 30.6001;
  int day = modified_julian_date - 14956 - std::trunc(year * 365.25) -
            std::trunc(month * 30.6001);
  if (month == 14 || month == 15) {
    year += 1;
    month -= 12;
  }
  year += 1900;
  month -= 1;

  int local_offset_min = (local_offset - std::trunc(local_offset)) * 60;

  int hour = static_cast<int>((Bits(group.block(BLOCK3), 0, 1) << 4) +
      Bits(group.block(BLOCK4), 12, 14) + local_offset) % 24;
  int minute = Bits(group.block(BLOCK4), 6, 6) + local_offset_min;

  if (month >= 1 && month <= 12 && day >= 1 && day <= 31 &&
      hour >= 0 && hour <= 23 && minute >= 0 && minute <= 59 &&
      fabs(std::trunc(local_offset)) <= 13.0) {
    char buffer[100];
    int local_offset_hour = fabs(std::trunc(local_offset));

    if (local_offset_hour == 0 && local_offset_min == 0) {
      snprintf(buffer, sizeof(buffer), "%04d-%02d-%02dT%02d:%02d:00Z",
               year, month, day, hour, minute);
    } else {
      snprintf(buffer, sizeof(buffer),
               "%04d-%02d-%02dT%02d:%02d:00%s%02d:%02d",
               year, month, day, hour, minute, local_offset > 0 ? "+" : "-",
               local_offset_hour, abs(local_offset_min));
    }
    clock_time_ = std::string(buffer);
    json_["clock_time"] = clock_time_;
  } else {
    json_["debug"].append("invalid date/time");
  }
}

// Group 6: In-house applications
void Station::DecodeType6(const Group& group) {
  json_["in_house_data"].append(Bits(group.block(BLOCK2), 0, 5));

  if (group.type().version == VERSION_A) {
    if (group.has(BLOCK3)) {
      json_["in_house_data"].append(Bits(group.block(BLOCK3), 0, 16));
      if (group.has(BLOCK4)) {
        json_["in_house_data"].append(Bits(group.block(BLOCK4), 0, 16));
      }
    }
  } else {
    if (group.has(BLOCK4)) {
      json_["in_house_data"].append(Bits(group.block(BLOCK4), 0, 16));
    }
  }
}

// Group 14: Enhanced Other Networks information
void Station::DecodeType14(const Group& group) {
  if (!(group.has(BLOCK4)))
    return;

  uint16_t on_pi = group.block(BLOCK4);
  json_["other_network"]["pi"] = "0x" + HexString(on_pi, 4);

  bool tp = Bits(group.block(BLOCK2), 4, 1);

  json_["other_network"]["tp"] = tp;

  if (group.type().version == VERSION_B) {
    bool ta = Bits(group.block(BLOCK2), 3, 1);
    json_["other_network"]["ta"] = ta;
    return;
  }

  if (!group.has(BLOCK3))
    return;

  uint16_t eon_variant = Bits(group.block(BLOCK2), 0, 4);

  if (eon_variant <= 3) {
    if (eon_ps_names_.count(on_pi) == 0)
      eon_ps_names_[on_pi] = RDSString(8);

    eon_ps_names_[on_pi].set(2 * eon_variant,
        RDSChar(Bits(group.block(BLOCK3), 8, 8)));
    eon_ps_names_[on_pi].set(2 * eon_variant+1,
        RDSChar(Bits(group.block(BLOCK3), 0, 8)));

    if (eon_ps_names_[on_pi].complete())
      json_["other_network"]["ps"] =
          eon_ps_names_[on_pi].last_complete_string();

  } else if (eon_variant == 4) {
    eon_alt_freqs_[on_pi].insert(Bits(group.block(BLOCK3), 8, 8));
    eon_alt_freqs_[on_pi].insert(Bits(group.block(BLOCK3), 0, 8));

    if (eon_alt_freqs_[on_pi].complete()) {
      for (CarrierFrequency freq : eon_alt_freqs_[on_pi].get())
        json_["other_network"]["alt_kilohertz"].append(freq.kHz());
      eon_alt_freqs_[on_pi].clear();
    }

  } else if (eon_variant >= 5 && eon_variant <= 9) {
    CarrierFrequency freq_other(Bits(group.block(BLOCK3), 0, 8));

    if (freq_other.valid())
      json_["other_network"]["kilohertz"] = freq_other.kHz();

  } else if (eon_variant == 12) {
    bool has_linkage = Bits(group.block(BLOCK3), 15, 1);
    uint16_t lsn = Bits(group.block(BLOCK3), 0, 12);
    json_["other_network"]["has_linkage"] = has_linkage;
    if (has_linkage && lsn != 0)
      json_["other_network"]["linkage_set"] = lsn;

  } else if (eon_variant == 13) {
    uint16_t pty = Bits(group.block(BLOCK3), 11, 5);
    bool ta      = Bits(group.block(BLOCK3), 0, 1);
    json_["other_network"]["prog_type"] = PTYNameString(pty, options_.rbds);
    json_["other_network"]["ta"] = ta;

  } else if (eon_variant == 14) {
    uint16_t pin = group.block(BLOCK3);

    if (pin != 0x0000) {
      json_["other_network"]["prog_item_started"]["day"] = Bits(pin, 11, 5);
      json_["other_network"]["prog_item_started"]["time"] =
          HoursMinutesString(Bits(pin, 6, 5), Bits(pin, 0, 6));
    }

  } else {
    json_["debug"].append("TODO: EON variant " +
        std::to_string(Bits(group.block(BLOCK2), 0, 4)));
  }
}

/* Group 15B: Fast basic tuning and switching information */
void Station::DecodeType15B(const Group& group) {
  eBlockNumber block_num = group.has(BLOCK2) ? BLOCK2 : BLOCK4;

  is_ta_    = Bits(group.block(block_num), 4, 1);
  is_music_ = Bits(group.block(block_num), 3, 1);

  json_["ta"] = is_ta_;
  json_["is_music"] = is_music_;
}

/* Open Data Application */
void Station::DecodeODAGroup(const Group& group) {
  if (oda_app_for_group_.count(group.type()) == 0)
    return;

  uint16_t app_id = oda_app_for_group_[group.type()];

  if (app_id == 0xCD46 || app_id == 0xCD47) {
#ifdef ENABLE_TMC

    if (group.has(BLOCK2) && group.has(BLOCK3) && group.has(BLOCK4))
      tmc_.UserGroup(Bits(group.block(BLOCK2), 0, 5), group.block(BLOCK3),
                     group.block(BLOCK4), &json_);
#endif
  } else if (app_id == 0x4BD7) {
    ParseRadioTextPlus(group);
  }
}

void Station::ParseRadioTextPlus(const Group& group) {
  bool item_toggle  = Bits(group.block(BLOCK2), 4, 1);
  bool item_running = Bits(group.block(BLOCK2), 3, 1);

  if (item_toggle != radiotext_plus_toggle_ ||
      item_running != radiotext_plus_item_running_) {
    radiotext_.clear();
    radiotext_plus_toggle_ = item_toggle;
    radiotext_plus_item_running_ = item_running;
  }

  json_["radiotext_plus"]["item_running"] = item_running;
  json_["radiotext_plus"]["item_toggle"] = item_toggle ? 1 : 0;

  int num_tags = group.has(BLOCK3) ? (group.has(BLOCK4) ? 2 : 1) : 0;
  std::vector<RTPlusTag> tags(num_tags);

  if (num_tags > 0) {
    tags[0].content_type = (Bits(group.block(BLOCK2), 0, 3) << 3) +
                            Bits(group.block(BLOCK3), 13, 3);
    tags[0].start  = Bits(group.block(BLOCK3), 7, 6);
    tags[0].length = Bits(group.block(BLOCK3), 1, 6) + 1;

    if (num_tags == 2) {
      tags[1].content_type = (Bits(group.block(BLOCK3), 0, 1) << 5) +
                              Bits(group.block(BLOCK4), 11, 5);
      tags[1].start  = Bits(group.block(BLOCK4), 5, 6);
      tags[1].length = Bits(group.block(BLOCK4), 0, 5) + 1;
    }
  }

  for (RTPlusTag tag : tags) {
    std::string text =
      rtrim(radiotext_.last_complete_string(tag.start, tag.length));

    if (radiotext_.has_chars(tag.start, tag.length) && text.length() > 0 &&
        tag.content_type != 0) {
      Json::Value tag_json;
      tag_json["content-type"] = RTPlusContentTypeString(tag.content_type);
      tag_json["data"] = text;
      json_["radiotext_plus"]["tags"].append(tag_json);
    }
  }
}

}  // namespace redsea
