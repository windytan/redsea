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

// Programme Item Number (IEC 62106:2015, section 6.1.5.2)
bool decodePIN(uint16_t pin, Json::Value* json) {
  uint16_t day    = getBits<5>(pin, 11);
  uint16_t hour   = getBits<5>(pin, 6);
  uint16_t minute = getBits<6>(pin, 0);
  if (day >= 1 && hour <= 24 && minute <= 59) {
    (*json)["prog_item_number"] = pin;
    (*json)["prog_item_started"]["day"] = day;
    (*json)["prog_item_started"]["time"] = getHoursMinutesString(hour, minute);
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

uint16_t Group::getBlock(eBlockNumber block_num) const {
  return blocks_[block_num].data;
}

uint16_t Group::getBlock1() const {
  return blocks_[0].data;
}

uint16_t Group::getBlock2() const {
  return blocks_[1].data;
}

uint16_t Group::getBlock3() const {
  return blocks_[2].data;
}

uint16_t Group::getBlock4() const {
  return blocks_[3].data;
}

bool Group::has(eBlockNumber block_num) const {
  return blocks_[block_num].is_received;
}

bool Group::isEmpty() const {
  return !(has(BLOCK1) || has(BLOCK2) || has(BLOCK3) || has(BLOCK4));
}

uint16_t Group::getPI() const {
  if (blocks_[BLOCK1].is_received)
    return blocks_[BLOCK1].data;
  else if (blocks_[BLOCK3].is_received &&
           blocks_[BLOCK3].offset == Offset::Cprime)
    return blocks_[BLOCK3].data;
  else
    return 0x0000;
}

float Group::getBLER() const {
  return bler_;
}

int Group::getNumErrors() const {
  return std::accumulate(blocks_.cbegin(), blocks_.cend(), 0,
      [](int a, Block b) {
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
      GroupType potential_type(getBits<5>(block.data, 11));
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

void Group::setTime(std::chrono::time_point<std::chrono::system_clock> t) {
  time_received_ = t;
  has_time_ = true;
}

void Group::setAverageBLER(float bler) {
  bler_ = bler;
  has_bler_ = true;
}

/*
 * Print the raw group data into a stream, encoded as hex, like in RDS Spy.
 * Invalid blocks are replaced with "----".
 *
 */
void Group::printHex(std::ostream* stream) const {
  stream->fill('0');
  stream->setf(std::ios_base::uppercase);

  for (eBlockNumber block_num : {BLOCK1, BLOCK2, BLOCK3, BLOCK4}) {
    const Block& block = blocks_[block_num];
    if (block.is_received)
      *stream << std::hex << std::setw(4) << block.data;
    else
      *stream << "----";

    if (block_num != BLOCK4)
      *stream << " ";
  }
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

void Station::updateAndPrint(const Group& group, std::ostream* stream) {
  // Allow 1 group with missed PI
  if (group.hasPI())
    last_group_had_pi_ = true;
  else if (last_group_had_pi_)
    last_group_had_pi_ = false;
  else
    return;

  if (group.isEmpty())
    return;

  json_.clear();
  json_["*SORT00*pi"] = "0x" + getHexString(getPI(), 4);
  if (options_.rbds) {
    std::string callsign = getCallsignFromPI(getPI());
    if (!callsign.empty()) {
      if ((getPI() & 0xF000) == 0x1000)
        json_["*SORT02*callsign_uncertain"] = callsign;
      else
        json_["*SORT02*callsign"] = callsign;
    }
  }

  if (options_.timestamp)
    json_["*SORT01*rx_time"] = getTimePointString(group.getRxTime(), options_.time_format);

  if (group.hasBLER())
    json_["bler"] = int(group.getBLER() + .5f);

  if (options_.num_channels > 1)
    json_["channel"] = which_channel_;

  if (options_.show_raw) {
    std::stringstream ss;
    group.printHex(&ss);
    json_["raw_data"] = ss.str();
  }

  decodeBasics(group);

  if (group.hasType()) {
    const GroupType& type = group.getType();

    if      (type.number == 0)
      decodeType0(group);
    else if (type.number == 1)
      decodeType1(group);
    else if (type.number == 2)
      decodeType2(group);
    else if (type.number == 3 && type.version == GroupType::Version::A)
      decodeType3A(group);
    else if (type.number == 4 && type.version == GroupType::Version::A)
      decodeType4A(group);
    else if (type.number == 14)
      decodeType14(group);
    else if (type.number == 15 && type.version == GroupType::Version::B)
      decodeType15B(group);
    else if (oda_app_for_group_.count(type) > 0)
      decodeODAGroup(group);
    else if (type.number == 6)
      decodeType6(group);
    else
      json_["debug"].append("TODO " + type.str());
  }

  std::stringstream ss;
  writer_->write(json_, &ss);
  ss << '\n';

  *stream << ss.str() << std::flush;
}

uint16_t Station::getPI() const {
  return pi_;
}

void Station::decodeBasics(const Group& group) {
  if (group.has(BLOCK2)) {
    uint16_t pty = getBits<5>(group.getBlock2(), 5);

    if (group.hasType())
      json_["*SORT03*group"] = group.getType().str();

    bool tp = getBits<1>(group.getBlock2(), 10);
    json_["tp"] = tp;

    json_["prog_type"] =
        options_.rbds ? getPTYNameStringRBDS(pty) : getPTYNameString(pty);
  } else if (group.getType().number == 15 && group.getType().version == GroupType::Version::B &&
      group.has(BLOCK4)) {
    uint16_t pty = getBits<5>(group.getBlock4(), 5);

    json_["*SORT03*group"] = group.getType().str();

    bool tp = getBits<1>(group.getBlock4(), 10);
    json_["tp"] = tp;
    json_["prog_type"] =
        options_.rbds ? getPTYNameStringRBDS(pty) : getPTYNameString(pty);
  }
}

// Group 0: Basic tuning and switching information
void Station::decodeType0(const Group& group) {
  uint16_t segment_address = getBits<2>(group.getBlock2(), 0);
  bool is_di = getBits<1>(group.getBlock2(), 2);
  json_["di"][getDICodeString(segment_address)] = is_di;

  json_["ta"]       = static_cast<bool>(getBits<1>(group.getBlock2(), 4));
  json_["is_music"] = static_cast<bool>(getBits<1>(group.getBlock2(), 3));

  if (!group.has(BLOCK3))
    return;

  if (group.getType().version == GroupType::Version::A) {
    alt_freq_list_.insert(getBits<8>(group.getBlock3(), 8));
    alt_freq_list_.insert(getBits<8>(group.getBlock3(), 0));

    if (alt_freq_list_.isComplete()) {
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

  ps_.update(segment_address * 2,
             RDSChar(getBits<8>(group.getBlock4(), 8)),
             RDSChar(getBits<8>(group.getBlock4(), 0)));

  if (ps_.text.isComplete())
    json_["*SORT04*ps"] = ps_.text.getLastCompleteString();
  else if (options_.show_partial)
    json_["*SORT04*partial_ps"] = ps_.text.str();
}

// Group 1: Programme Item Number and slow labelling codes
void Station::decodeType1(const Group& group) {
  if (!(group.has(BLOCK3) && group.has(BLOCK4)))
    return;

  pin_ = group.getBlock4();

  if (pin_ != 0x0000)
    if (!decodePIN(pin_, &json_))
      json_["debug"].append("invalid PIN");

  if (group.getType().version == GroupType::Version::A) {
    pager_.paging_code = getBits<3>(group.getBlock2(), 2);
    if (pager_.paging_code != 0)
      pager_.interval = getBits<2>(group.getBlock2(), 0);
    linkage_la_ = getBits<1>(group.getBlock3(), 15);
    json_["has_linkage"] = linkage_la_;

    int slow_label_variant = getBits<3>(group.getBlock3(), 12);

    switch (slow_label_variant) {
      case 0:
        if (pager_.paging_code != 0) {
          pager_.opc = getBits<4>(group.getBlock3(), 8);

          // No PIN (IEC 62106:2015, section M.3.2.5.3)
          if (group.has(BLOCK4) && getBits<5>(group.getBlock4(), 11) == 0)
            pager_.decode1ABlock4(group.getBlock4());
        }

        ecc_ = getBits<8>(group.getBlock3(), 0);
        cc_  = getBits<4>(pi_, 12);

        if (ecc_ != 0x00) {
          has_country_ = true;
          json_["country"] = getCountryString(pi_, ecc_);
        }
        break;

      case 1:
        tmc_id_ = getBits<12>(group.getBlock3(), 0);
        json_["tmc_id"] = tmc_id_;
        break;

      case 2:
        if (pager_.paging_code != 0) {
          pager_.pac = getBits<6>(group.getBlock3(), 0);
          pager_.opc = getBits<4>(group.getBlock3(), 8);

          // No PIN (IEC 62105:2015, section M.3.2.5.3)
          if (group.has(BLOCK4) && getBits<5>(group.getBlock4(), 11) == 0)
            pager_.decode1ABlock4(group.getBlock4());
        }
        break;

      case 3:
        json_["language"] = getLanguageString(getBits<8>(group.getBlock3(), 0));
        break;

      case 7:
        json_["ews"] = getBits<12>(group.getBlock3(), 0);
        break;

      default:
        json_["debug"].append("TODO: SLC variant " +
            std::to_string(slow_label_variant));
        break;
    }
  }
}

// Group 2: RadioText
void Station::decodeType2(const Group& group) {
  if (!(group.has(BLOCK3) && group.has(BLOCK4)))
    return;

  size_t radiotext_position = getBits<4>(group.getBlock2(), 0) *
    (group.getType().version == GroupType::Version::A ? 4 : 2);

  if (radiotext_.isABChanged(getBits<1>(group.getBlock2(), 4)))
    radiotext_.text.clear();

  if (group.getType().version == GroupType::Version::A) {
    radiotext_.text.resize(64);
    radiotext_.update(radiotext_position,
                      RDSChar(getBits<8>(group.getBlock3(), 8)),
                      RDSChar(getBits<8>(group.getBlock3(), 0)));
  } else {
    radiotext_.text.resize(32);
  }

  if (group.has(BLOCK4)) {
    radiotext_.update(radiotext_position +
                      (group.getType().version == GroupType::Version::A ? 2 : 0),
                      RDSChar(getBits<8>(group.getBlock4(), 8)),
                      RDSChar(getBits<8>(group.getBlock4(), 0)));
  }

  if (radiotext_.text.isComplete())
    json_["*SORT04*radiotext"] = rtrim(radiotext_.text.getLastCompleteString());
  else if (options_.show_partial && rtrim(radiotext_.text.str()).length() > 0)
    json_["*SORT04*partial_radiotext"] = rtrim(radiotext_.text.str());
}

// Group 3A: Application identification for Open Data
void Station::decodeType3A(const Group& group) {
  if (!(group.has(BLOCK3) && group.has(BLOCK4)))
    return;

  if (group.getType().version != GroupType::Version::A)
    return;

  GroupType oda_group_type(getBits<5>(group.getBlock2(), 0));
  uint16_t oda_message = group.getBlock3();
  uint16_t oda_app_id  = group.getBlock4();

  oda_app_for_group_[oda_group_type] = oda_app_id;

  json_["open_data_app"]["oda_group"] = oda_group_type.str();
  json_["open_data_app"]["app_name"] = getAppNameString(oda_app_id);

  switch (oda_app_id) {
    case 0xCD46:
    case 0xCD47:
#ifdef ENABLE_TMC
      tmc_.receiveSystemGroup(oda_message, &json_);
#else
      json_["debug"].append("redsea compiled without TMC support");
#endif
      break;

    case 0x4BD7:
      has_radiotext_plus_ = true;
      radiotext_plus_.cb = getBits<1>(oda_message, 12);
      radiotext_plus_.scb = getBits<4>(oda_message, 8);
      radiotext_plus_.template_num = getBits<8>(oda_message, 0);
      break;

    default:
      json_["debug"].append("TODO: Unimplemented ODA app " +
          std::to_string(oda_app_id));
      json_["open_data_app"]["message"] = oda_message;
      break;
  }
}

// Group 4A: Clock-time and date
void Station::decodeType4A(const Group& group) {
  if (!(group.has(BLOCK3) && group.has(BLOCK4)))
    return;

  uint32_t modified_julian_date = getBits<17>(group.getBlock2(), group.getBlock3(), 1);
  double local_offset = (getBits<1>(group.getBlock4(), 5) ? -1 : 1) *
                         getBits<5>(group.getBlock4(), 0) / 2.0;
  modified_julian_date += local_offset / 24.0;

  int year  = int((modified_julian_date - 15078.2) / 365.25);
  int month = int((modified_julian_date - 14956.1 -
                std::trunc(year * 365.25)) / 30.6001);
  int day   = int(modified_julian_date - 14956 - std::trunc(year * 365.25) -
                std::trunc(month * 30.6001));
  if (month == 14 || month == 15) {
    year += 1;
    month -= 12;
  }
  year += 1900;
  month -= 1;

  int local_offset_min = int((local_offset - std::trunc(local_offset)) * 60.0);

  int hour = static_cast<int>(getBits<5>(group.getBlock3(), group.getBlock4(), 12) +
                              + local_offset) % 24;
  int minute = getBits<6>(group.getBlock4(), 6) + local_offset_min;

  bool is_date_valid = (month >= 1 && month <= 12 && day >= 1 && day <= 31 &&
                        hour >= 0 && hour <= 23 && minute >= 0 &&
                        minute <= 59 && fabs(std::trunc(local_offset)) <= 14.0);
  if (is_date_valid) {
    char buffer[100];
    int local_offset_hour = int(fabs(std::trunc(local_offset)));

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
void Station::decodeType6(const Group& group) {
  json_["in_house_data"].append(getBits<5>(group.getBlock2(), 0));

  if (group.getType().version == GroupType::Version::A) {
    if (group.has(BLOCK3)) {
      json_["in_house_data"].append(getBits<16>(group.getBlock3(), 0));
      if (group.has(BLOCK4)) {
        json_["in_house_data"].append(getBits<16>(group.getBlock4(), 0));
      }
    }
  } else {
    if (group.has(BLOCK4)) {
      json_["in_house_data"].append(getBits<16>(group.getBlock4(), 0));
    }
  }
}

// Group 14: Enhanced Other Networks information
void Station::decodeType14(const Group& group) {
  if (!(group.has(BLOCK4)))
    return;

  uint16_t on_pi = group.getBlock4();
  json_["other_network"]["*SORT00*pi"] = "0x" + getHexString(on_pi, 4);

  bool tp = getBits<1>(group.getBlock2(), 4);

  json_["other_network"]["tp"] = tp;

  if (group.getType().version == GroupType::Version::B) {
    bool ta = getBits<1>(group.getBlock2(), 3);
    json_["other_network"]["ta"] = ta;
    return;
  }

  if (!group.has(BLOCK3))
    return;

  uint16_t eon_variant = getBits<4>(group.getBlock2(), 0);
  switch (eon_variant) {
    case 0:
    case 1:
    case 2:
    case 3:
      if (eon_ps_names_.count(on_pi) == 0)
        eon_ps_names_[on_pi] = RDSString(8);

      eon_ps_names_[on_pi].set(2 * eon_variant,
          RDSChar(getBits<8>(group.getBlock3(), 8)));
      eon_ps_names_[on_pi].set(2 * eon_variant+1,
          RDSChar(getBits<8>(group.getBlock3(), 0)));

      if (eon_ps_names_[on_pi].isComplete())
        json_["other_network"]["ps"] =
            eon_ps_names_[on_pi].getLastCompleteString();
      break;

    case 4:
      eon_alt_freqs_[on_pi].insert(getBits<8>(group.getBlock3(), 8));
      eon_alt_freqs_[on_pi].insert(getBits<8>(group.getBlock3(), 0));

      if (eon_alt_freqs_[on_pi].isComplete()) {
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
      CarrierFrequency freq_other(getBits<8>(group.getBlock3(), 0));

      if (freq_other.isValid())
        json_["other_network"]["kilohertz"] = freq_other.kHz();

      break;
    }

    case 12:
    {
      bool has_linkage = getBits<1>(group.getBlock3(), 15);
      uint16_t lsn = getBits<12>(group.getBlock3(), 0);
      json_["other_network"]["has_linkage"] = has_linkage;
      if (has_linkage && lsn != 0)
        json_["other_network"]["linkage_set"] = lsn;
      break;
    }

    case 13:
    {
      uint16_t pty = getBits<5>(group.getBlock3(), 11);
      bool ta      = getBits<1>(group.getBlock3(), 0);
      json_["other_network"]["prog_type"] =
        options_.rbds ? getPTYNameStringRBDS(pty) : getPTYNameString(pty);
      json_["other_network"]["ta"] = ta;
      break;
    }

    case 14:
    {
      uint16_t pin = group.getBlock3();

      if (pin != 0x0000)
        decodePIN(pin, &(json_["other_network"]));
      break;
    }

    default:
      json_["debug"].append("TODO: EON variant " +
          std::to_string(getBits<4>(group.getBlock2(), 0)));
      break;
  }
}

/* Group 15B: Fast basic tuning and switching information */
void Station::decodeType15B(const Group& group) {
  eBlockNumber block_num = group.has(BLOCK2) ? BLOCK2 : BLOCK4;

  bool ta       = getBits<1>(group.getBlock(block_num), 4);
  bool is_music = getBits<1>(group.getBlock(block_num), 3);

  json_["ta"]       = ta;
  json_["is_music"] = is_music;
}

/* Open Data Application */
void Station::decodeODAGroup(const Group& group) {
  if (oda_app_for_group_.count(group.getType()) == 0)
    return;

  uint16_t app_id = oda_app_for_group_[group.getType()];

  if (app_id == 0xCD46 || app_id == 0xCD47) {
#ifdef ENABLE_TMC

    if (group.has(BLOCK2) && group.has(BLOCK3) && group.has(BLOCK4))
      tmc_.receiveUserGroup(getBits<5>(group.getBlock2(), 0), group.getBlock3(),
                            group.getBlock4(), &json_);
#endif
  } else if (app_id == 0x4BD7) {
    parseRadioTextPlus(group);
  }
}

void Station::parseRadioTextPlus(const Group& group) {
  bool item_toggle  = getBits<1>(group.getBlock2(), 4);
  bool item_running = getBits<1>(group.getBlock2(), 3);

  if (item_toggle != radiotext_plus_.toggle ||
      item_running != radiotext_plus_.item_running) {
    radiotext_.text.clear();
    radiotext_plus_.toggle = item_toggle;
    radiotext_plus_.item_running = item_running;
  }

  json_["radiotext_plus"]["item_running"] = item_running;
  json_["radiotext_plus"]["item_toggle"] = item_toggle ? 1 : 0;

  size_t num_tags = group.has(BLOCK3) ? (group.has(BLOCK4) ? 2 : 1) : 0;
  std::vector<RTPlusTag> tags(num_tags);

  if (num_tags > 0) {
    tags[0].content_type = uint16_t(getBits<6>(group.getBlock2(), group.getBlock3(), 13));
    tags[0].start        = getBits<6>(group.getBlock3(), 7);
    tags[0].length       = getBits<6>(group.getBlock3(), 1) + 1;

    if (num_tags == 2) {
      tags[1].content_type = uint16_t(getBits<6>(group.getBlock3(), group.getBlock4(), 11));
      tags[1].start        = getBits<6>(group.getBlock4(), 5);
      tags[1].length       = getBits<5>(group.getBlock4(), 0) + 1;
    }
  }

  for (RTPlusTag tag : tags) {
    std::string text =
      rtrim(radiotext_.text.getLastCompleteString(tag.start, tag.length));

    if (radiotext_.text.hasChars(tag.start, tag.length) && text.length() > 0 &&
        tag.content_type != 0) {
      Json::Value tag_json;
      tag_json["content-type"] = getRTPlusContentTypeString(tag.content_type);
      tag_json["data"] = text;
      json_["radiotext_plus"]["tags"].append(tag_json);
    }
  }
}

void Pager::decode1ABlock4(uint16_t block4) {
  int sub_type = getBits<1>(block4, 10);
  if (sub_type == 0) {
    pac = getBits<6>(block4, 4);
    opc = getBits<4>(block4, 0);
  } else if (sub_type == 1) {
    int sub_usage = getBits<2>(block4, 8);
    if (sub_usage == 0)
      ecc = getBits<6>(block4, 0);
    else if (sub_usage == 3)
      ccf = getBits<4>(block4, 0);
  }
}

}  // namespace redsea
