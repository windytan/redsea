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
#include <numeric>
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

// Programme Item Number (IEC 62106:2015, section 6.1.5.2)
bool DecodePin(uint16_t pin, Json::Value* json) {
  uint16_t day    = Bits<5>(pin, 11);
  uint16_t hour   = Bits<5>(pin, 6);
  uint16_t minute = Bits<6>(pin, 0);
  if (day >= 1 && hour <= 24 && minute <= 59) {
    (*json)["prog_item_number"] = pin;
    (*json)["prog_item_started"]["day"] = day;
    (*json)["prog_item_started"]["time"] = HoursMinutesString(hour, minute);
    return true;
  } else {
    return false;
  }
}

GroupType::GroupType(uint16_t type_code) :
  number((type_code >> 1) & 0xF),
  version((type_code & 0x1) == 0 ? GroupType::Version::A : GroupType::Version::B) {}

std::string GroupType::str() const {
  return std::string(std::to_string(number) +
         (version == GroupType::Version::A ? "A" : "B"));
}

bool operator==(const GroupType& type1, const GroupType& type2) {
  return type1.number == type2.number && type1.version == type2.version;
}

bool operator<(const GroupType& type1, const GroupType& type2) {
  return (type1.number < type2.number) ||
         (type1.number == type2.number && type1.version < type2.version);
}

/*
 * A single RDS group transmitted as four 16-bit blocks.
 *
 */
Group::Group() {
}

uint16_t Group::block(eBlockNumber block_num) const {
  return blocks_[block_num].data;
}

uint16_t Group::block1() const {
  return blocks_[0].data;
}

uint16_t Group::block2() const {
  return blocks_[1].data;
}

uint16_t Group::block3() const {
  return blocks_[2].data;
}

uint16_t Group::block4() const {
  return blocks_[3].data;
}

bool Group::has(eBlockNumber block_num) const {
  return blocks_[block_num].is_received;
}

bool Group::empty() const {
  return !(has(BLOCK1) || has(BLOCK2) || has(BLOCK3) || has(BLOCK4));
}

uint16_t Group::pi() const {
  if (blocks_[BLOCK1].is_received)
    return blocks_[BLOCK1].data;
  else if (blocks_[BLOCK3].is_received &&
           blocks_[BLOCK3].offset == Offset::Cprime)
    return blocks_[BLOCK3].data;
  else
    return 0x0000;
}

uint8_t Group::bler() const {
  return bler_;
}

uint8_t Group::num_errors() const {
  return std::accumulate(blocks_.cbegin(), blocks_.cend(), 0,
      [](int a, Block b) {
        return a + (b.had_errors || !b.is_received);
      });
}

bool Group::has_pi() const {
  return blocks_[BLOCK1].is_received ||
         (blocks_[BLOCK3].is_received && blocks_[BLOCK3].offset == Offset::Cprime);
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

void Group::set_block(eBlockNumber block_num, Block block) {
  blocks_[block_num] = block;

  if (block_num == BLOCK2) {
    type_ = GroupType(Bits<5>(block.data, 11));
    if (type_.version == GroupType::Version::A)
      has_type_ = true;
    else
      has_type_ = (has_c_prime_ || no_offsets_);

  } else if (block_num == BLOCK4) {
    if (has_c_prime_ && !has_type_) {
      GroupType potential_type(Bits<5>(block.data, 11));
      if (potential_type.number == 15 &&
          potential_type.version == GroupType::Version::B) {
        type_ = potential_type;
        has_type_ = true;
      }
    }
  }

  if (block.offset == Offset::Cprime && has(BLOCK2))
    has_type_ = (type_.version == GroupType::Version::B);
}

void Group::set_time(std::chrono::time_point<std::chrono::system_clock> t) {
  time_received_ = t;
  has_time_ = true;
}

void Group::set_bler(uint8_t bler) {
  bler_ = bler;
  has_bler_ = true;
}

void Group::PrintHex(std::ostream* stream,
                     const std::string& time_format) const {
  stream->fill('0');
  stream->setf(std::ios_base::uppercase);

  if (empty())
    return;

  for (eBlockNumber block_num : {BLOCK1, BLOCK2, BLOCK3, BLOCK4}) {
    const Block& block = blocks_[block_num];
    if (block.is_received)
      *stream << std::hex << std::setw(4) << block.data;
    else
      *stream << "----";

    if (block_num != BLOCK4)
      *stream << " ";
  }

  if (has_time())
    *stream << " " << TimePointToString(rx_time(), time_format);

  *stream << '\n' << std::flush;
}

/*
 * A Station represents a single broadcast carrier identified by its RDS PI
 * code.
 *
 */
Station::Station() : Station(0x0000, Options(), 0) {
}

Station::Station(uint16_t _pi, const Options& options, int which_channel) :
  pi_(_pi), options_(options), which_channel_(which_channel)
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
  // Allow 1 group with missed PI
  if (group.has_pi())
    last_group_had_pi_ = true;
  else if (last_group_had_pi_)
    last_group_had_pi_ = false;
  else
    return;

  if (group.empty())
    return;

  json_.clear();
  json_["pi"] = "0x" + HexString(pi(), 4);
  if (options_.rbds) {
    std::string callsign = PiToCallSign(pi());
    if (!callsign.empty()) {
      if ((pi() & 0xF000) == 0x1000)
        json_["callsign_uncertain"] = callsign;
      else
        json_["callsign"] = callsign;
    }
  }

  if (options_.timestamp)
    json_["rx_time"] = TimePointToString(group.rx_time(), options_.time_format);

  if (group.has_bler())
    json_["bler"] = group.bler();

  if (options_.num_channels > 1)
    json_["channel"] = which_channel_;

  DecodeBasics(group);

  if (group.has_type()) {
    if      (group.type().number == 0)
      DecodeType0(group);
    else if (group.type().number == 1)
      DecodeType1(group);
    else if (group.type().number == 2)
      DecodeType2(group);
    else if (group.type().number == 3 && group.type().version == GroupType::Version::A)
      DecodeType3A(group);
    else if (group.type().number == 4 && group.type().version == GroupType::Version::A)
      DecodeType4A(group);
    else if (group.type().number == 14)
      DecodeType14(group);
    else if (group.type().number == 15 && group.type().version == GroupType::Version::B)
      DecodeType15B(group);
    else if (oda_app_for_group_.count(group.type()) > 0)
      DecodeODAGroup(group);
    else if (group.type().number == 6)
      DecodeType6(group);
    else
      json_["debug"].append("TODO " + group.type().str());
  }

  std::stringstream ss;
  writer_->write(json_, &ss);
  ss << '\n';

  *stream << ss.str() << std::flush;
}

uint16_t Station::pi() const {
  return pi_;
}

void Station::DecodeBasics(const Group& group) {
  if (group.has(BLOCK2)) {
    uint16_t pty = Bits<5>(group.block2(), 5);

    if (group.has_type())
      json_["group"] = group.type().str();
    json_["tp"] = Bits<1>(group.block2(), 10);
    json_["prog_type"] =
        options_.rbds ? PTYNameStringRBDS(pty) : PTYNameString(pty);
  } else if (group.type().number == 15 && group.type().version == GroupType::Version::B &&
      group.has(BLOCK4)) {
    uint16_t pty = Bits<5>(group.block4(), 5);

    json_["group"] = group.type().str();
    json_["tp"] = Bits<1>(group.block4(), 10);
    json_["prog_type"] =
        options_.rbds ? PTYNameStringRBDS(pty) : PTYNameString(pty);
  }
}

// Group 0: Basic tuning and switching information
void Station::DecodeType0(const Group& group) {
  uint16_t segment_address = Bits<2>(group.block2(), 0);
  bool is_di = Bits<1>(group.block2(), 2);
  json_["di"][DICodeString(segment_address)] = is_di;

  json_["ta"]       = Bits<1>(group.block2(), 4);
  json_["is_music"] = Bits<1>(group.block2(), 3);

  if (!group.has(BLOCK3))
    return;

  if (group.type().version == GroupType::Version::A) {
    alt_freq_list_.insert(Bits<8>(group.block3(), 8));
    alt_freq_list_.insert(Bits<8>(group.block3(), 0));

    if (alt_freq_list_.complete()) {
      for (CarrierFrequency f : alt_freq_list_.get())
        json_["alt_kilohertz"].append(f.kHz());
      alt_freq_list_.clear();

    } else if (options_.show_partial) {
      for (CarrierFrequency f : alt_freq_list_.get())
        json_["partial_alt_kilohertz"].append(f.kHz());
    }
  }

  if (!group.has(BLOCK4))
    return;

  ps_.Update(segment_address * 2,
             RDSChar(Bits<8>(group.block4(), 8)),
             RDSChar(Bits<8>(group.block4(), 0)));

  if (ps_.text.complete())
    json_["ps"] = ps_.text.last_complete_string();
  else if (options_.show_partial)
    json_["partial_ps"] = ps_.text.str();
}

// Group 1: Programme Item Number and slow labelling codes
void Station::DecodeType1(const Group& group) {
  if (!(group.has(BLOCK3) && group.has(BLOCK4)))
    return;

  pin_ = group.block4();

  if (pin_ != 0x0000)
    if (!DecodePin(pin_, &json_))
      json_["debug"].append("invalid PIN");

  if (group.type().version == GroupType::Version::A) {
    pager_.paging_code = Bits<3>(group.block2(), 2);
    if (pager_.paging_code != 0)
      pager_.interval = Bits<2>(group.block2(), 0);
    linkage_la_ = Bits<1>(group.block3(), 15);
    json_["has_linkage"] = linkage_la_;

    int slow_label_variant = Bits<3>(group.block3(), 12);

    switch (slow_label_variant) {
      case 0:
        if (pager_.paging_code != 0) {
          pager_.opc = Bits<4>(group.block3(), 8);

          // No PIN (IEC 62106:2015, section M.3.2.5.3)
          if (group.has(BLOCK4) && Bits<5>(group.block4(), 11) == 0)
            pager_.Decode1ABlock4(group.block4());
        }

        ecc_ = Bits<8>(group.block3(), 0);
        cc_  = Bits<4>(pi_, 12);

        if (ecc_ != 0x00) {
          has_country_ = true;
          json_["country"] = CountryString(pi_, ecc_);
        }
        break;

      case 1:
        tmc_id_ = Bits<12>(group.block3(), 0);
        json_["tmc_id"] = tmc_id_;
        break;

      case 2:
        if (pager_.paging_code != 0) {
          pager_.pac = Bits<6>(group.block3(), 0);
          pager_.opc = Bits<4>(group.block3(), 8);

          // No PIN (IEC 62105:2015, section M.3.2.5.3)
          if (group.has(BLOCK4) && Bits<5>(group.block4(), 11) == 0)
            pager_.Decode1ABlock4(group.block4());
        }
        break;

      case 3:
        json_["language"] = LanguageString(Bits<8>(group.block3(), 0));
        break;

      case 7:
        json_["ews"] = Bits<12>(group.block3(), 0);
        break;

      default:
        json_["debug"].append("TODO: SLC variant " +
            std::to_string(slow_label_variant));
        break;
    }
  }
}

// Group 2: RadioText
void Station::DecodeType2(const Group& group) {
  if (!(group.has(BLOCK3) && group.has(BLOCK4)))
    return;

  int radiotext_position = Bits<4>(group.block2(), 0) *
    (group.type().version == GroupType::Version::A ? 4 : 2);

  if (radiotext_.is_ab_changed(Bits<1>(group.block2(), 4)))
    radiotext_.text.clear();

  if (group.type().version == GroupType::Version::A) {
    radiotext_.text.resize(64);
    radiotext_.Update(radiotext_position,
                      RDSChar(Bits<8>(group.block3(), 8)),
                      RDSChar(Bits<8>(group.block3(), 0)));
  } else {
    radiotext_.text.resize(32);
  }

  if (group.has(BLOCK4)) {
    radiotext_.Update(radiotext_position +
                      (group.type().version == GroupType::Version::A ? 2 : 0),
                      RDSChar(Bits<8>(group.block4(), 8)),
                      RDSChar(Bits<8>(group.block4(), 0)));
  }

  if (radiotext_.text.complete())
    json_["radiotext"] = rtrim(radiotext_.text.last_complete_string());
  else if (options_.show_partial && rtrim(radiotext_.text.str()).length() > 0)
    json_["partial_radiotext"] = rtrim(radiotext_.text.str());
}

// Group 3A: Application identification for Open Data
void Station::DecodeType3A(const Group& group) {
  if (!(group.has(BLOCK3) && group.has(BLOCK4)))
    return;

  if (group.type().version != GroupType::Version::A)
    return;

  GroupType oda_group_type(Bits<5>(group.block2(), 0));
  uint16_t oda_message = group.block3();
  uint16_t oda_app_id  = group.block4();

  oda_app_for_group_[oda_group_type] = oda_app_id;

  json_["open_data_app"]["oda_group"] = oda_group_type.str();
  json_["open_data_app"]["app_name"] = AppNameString(oda_app_id);

  switch (oda_app_id) {
    case 0xCD46:
    case 0xCD47:
#ifdef ENABLE_TMC
      tmc_.ReceiveSystemGroup(oda_message, &json_);
#else
      json_["debug"].append("redsea compiled without TMC support");
#endif
      break;

    case 0x4BD7:
      has_radiotext_plus_ = true;
      radiotext_plus_.cb = Bits<1>(oda_message, 12);
      radiotext_plus_.scb = Bits<4>(oda_message, 8);
      radiotext_plus_.template_num = Bits<8>(oda_message, 0);
      break;

    default:
      json_["debug"].append("TODO: Unimplemented ODA app " +
          std::to_string(oda_app_id));
      json_["open_data_app"]["message"] = oda_message;
      break;
  }
}

// Group 4A: Clock-time and date
void Station::DecodeType4A(const Group& group) {
  if (!(group.has(BLOCK3) && group.has(BLOCK4)))
    return;

  int modified_julian_date = Bits<17>(group.block2(), group.block3(), 1);
  double local_offset = (Bits<1>(group.block4(), 5) ? -1 : 1) *
         Bits<5>(group.block4(), 0) / 2.0;
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

  int hour = static_cast<int>(Bits<5>(group.block3(), group.block4(), 12) +
                              + local_offset) % 24;
  int minute = Bits<6>(group.block4(), 6) + local_offset_min;

  bool is_date_valid = (month >= 1 && month <= 12 && day >= 1 && day <= 31 &&
                        hour >= 0 && hour <= 23 && minute >= 0 &&
                        minute <= 59 && fabs(std::trunc(local_offset)) <= 14.0);
  if (is_date_valid) {
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
  json_["in_house_data"].append(Bits<5>(group.block2(), 0));

  if (group.type().version == GroupType::Version::A) {
    if (group.has(BLOCK3)) {
      json_["in_house_data"].append(Bits<16>(group.block3(), 0));
      if (group.has(BLOCK4)) {
        json_["in_house_data"].append(Bits<16>(group.block4(), 0));
      }
    }
  } else {
    if (group.has(BLOCK4)) {
      json_["in_house_data"].append(Bits<16>(group.block4(), 0));
    }
  }
}

// Group 14: Enhanced Other Networks information
void Station::DecodeType14(const Group& group) {
  if (!(group.has(BLOCK4)))
    return;

  uint16_t on_pi = group.block4();
  json_["other_network"]["pi"] = "0x" + HexString(on_pi, 4);

  bool tp = Bits<1>(group.block2(), 4);

  json_["other_network"]["tp"] = tp;

  if (group.type().version == GroupType::Version::B) {
    bool ta = Bits<1>(group.block2(), 3);
    json_["other_network"]["ta"] = ta;
    return;
  }

  if (!group.has(BLOCK3))
    return;

  uint16_t eon_variant = Bits<4>(group.block2(), 0);
  switch (eon_variant) {
    case 0:
    case 1:
    case 2:
    case 3:
      if (eon_ps_names_.count(on_pi) == 0)
        eon_ps_names_[on_pi] = RDSString(8);

      eon_ps_names_[on_pi].set(2 * eon_variant,
          RDSChar(Bits<8>(group.block3(), 8)));
      eon_ps_names_[on_pi].set(2 * eon_variant+1,
          RDSChar(Bits<8>(group.block3(), 0)));

      if (eon_ps_names_[on_pi].complete())
        json_["other_network"]["ps"] =
            eon_ps_names_[on_pi].last_complete_string();
      break;

    case 4:
      eon_alt_freqs_[on_pi].insert(Bits<8>(group.block3(), 8));
      eon_alt_freqs_[on_pi].insert(Bits<8>(group.block3(), 0));

      if (eon_alt_freqs_[on_pi].complete()) {
        for (CarrierFrequency freq : eon_alt_freqs_[on_pi].get())
          json_["other_network"]["alt_kilohertz"].append(freq.kHz());
        eon_alt_freqs_[on_pi].clear();
      }
      break;

    case 5:
    case 6:
    case 7:
    case 8:
    case 9:
    {
      CarrierFrequency freq_other(Bits<8>(group.block3(), 0));

      if (freq_other.valid())
        json_["other_network"]["kilohertz"] = freq_other.kHz();

      break;
    }

    case 12:
    {
      bool has_linkage = Bits<1>(group.block3(), 15);
      uint16_t lsn = Bits<12>(group.block3(), 0);
      json_["other_network"]["has_linkage"] = has_linkage;
      if (has_linkage && lsn != 0)
        json_["other_network"]["linkage_set"] = lsn;
      break;
    }

    case 13:
    {
      uint16_t pty = Bits<5>(group.block3(), 11);
      bool ta      = Bits<1>(group.block3(), 0);
      json_["other_network"]["prog_type"] =
        options_.rbds ? PTYNameStringRBDS(pty) : PTYNameString(pty);
      json_["other_network"]["ta"] = ta;
      break;
    }

    case 14:
    {
      uint16_t pin = group.block3();

      if (pin != 0x0000)
        DecodePin(pin, &(json_["other_network"]));
      break;
    }

    default:
      json_["debug"].append("TODO: EON variant " +
          std::to_string(Bits<4>(group.block2(), 0)));
      break;
  }
}

/* Group 15B: Fast basic tuning and switching information */
void Station::DecodeType15B(const Group& group) {
  eBlockNumber block_num = group.has(BLOCK2) ? BLOCK2 : BLOCK4;

  json_["ta"]       = Bits<1>(group.block(block_num), 4);
  json_["is_music"] = Bits<1>(group.block(block_num), 3);
}

/* Open Data Application */
void Station::DecodeODAGroup(const Group& group) {
  if (oda_app_for_group_.count(group.type()) == 0)
    return;

  uint16_t app_id = oda_app_for_group_[group.type()];

  if (app_id == 0xCD46 || app_id == 0xCD47) {
#ifdef ENABLE_TMC

    if (group.has(BLOCK2) && group.has(BLOCK3) && group.has(BLOCK4))
      tmc_.ReceiveUserGroup(Bits<5>(group.block2(), 0), group.block3(),
                            group.block4(), &json_);
#endif
  } else if (app_id == 0x4BD7) {
    ParseRadioTextPlus(group);
  }
}

void Station::ParseRadioTextPlus(const Group& group) {
  bool item_toggle  = Bits<1>(group.block2(), 4);
  bool item_running = Bits<1>(group.block2(), 3);

  if (item_toggle != radiotext_plus_.toggle ||
      item_running != radiotext_plus_.item_running) {
    radiotext_.text.clear();
    radiotext_plus_.toggle = item_toggle;
    radiotext_plus_.item_running = item_running;
  }

  json_["radiotext_plus"]["item_running"] = item_running;
  json_["radiotext_plus"]["item_toggle"] = item_toggle ? 1 : 0;

  int num_tags = group.has(BLOCK3) ? (group.has(BLOCK4) ? 2 : 1) : 0;
  std::vector<RTPlusTag> tags(num_tags);

  if (num_tags > 0) {
    tags[0].content_type = Bits<6>(group.block2(), group.block3(), 13);
    tags[0].start        = Bits<6>(group.block3(), 7);
    tags[0].length       = Bits<6>(group.block3(), 1) + 1;

    if (num_tags == 2) {
      tags[1].content_type = Bits<6>(group.block3(), group.block4(), 11);
      tags[1].start        = Bits<6>(group.block4(), 5);
      tags[1].length       = Bits<5>(group.block4(), 0) + 1;
    }
  }

  for (RTPlusTag tag : tags) {
    std::string text =
      rtrim(radiotext_.text.last_complete_string(tag.start, tag.length));

    if (radiotext_.text.has_chars(tag.start, tag.length) && text.length() > 0 &&
        tag.content_type != 0) {
      Json::Value tag_json;
      tag_json["content-type"] = RTPlusContentTypeString(tag.content_type);
      tag_json["data"] = text;
      json_["radiotext_plus"]["tags"].append(tag_json);
    }
  }
}

void Pager::Decode1ABlock4(uint16_t block4) {
  int sub_type = Bits<1>(block4, 10);
  if (sub_type == 0) {
    pac = Bits<6>(block4, 4);
    opc = Bits<4>(block4, 0);
  } else if (sub_type == 1) {
    int sub_usage = Bits<2>(block4, 8);
    if (sub_usage == 0)
      ecc = Bits<6>(block4, 0);
    else if (sub_usage == 3)
      ccf = Bits<4>(block4, 0);
  }
}

}  // namespace redsea
