#include "src/groups.h"

#include <cassert>
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

namespace {

std::string hoursMinutesString(int hr, int mn) {
  std::stringstream ss;
  ss << std::setfill('0') << std::setw(2) << hr << ":" << mn;
  return ss.str();
}

}  // namespace

GroupType::GroupType(uint16_t type_code) : num((type_code >> 1) & 0xF),
  ab((type_code & 0x1) == 0 ? VERSION_A : VERSION_B) {}
GroupType::GroupType(const GroupType& obj) : num(obj.num), ab(obj.ab) {}

std::string GroupType::toString() const {
  return std::string(std::to_string(num) + (ab == VERSION_A ? "A" : "B"));
}

bool operator==(const GroupType& obj1, const GroupType& obj2) {
  return (obj1.num == obj2.num && obj1.ab == obj2.ab);
}

bool operator<(const GroupType& obj1, const GroupType& obj2) {
  return ((obj1.num < obj2.num) || (obj1.ab < obj2.ab));
}

Group::Group() : hasType(false), hasPi(false),
  hasOffset({false,false,false,false,false}), block(5) {
}

void Group::printHex(std::ostream* stream) const {

  stream->fill('0');
  stream->setf(std::ios_base::uppercase);

  if (hasOffset[OFFSET_A])
    *stream << std::hex << std::setw(4) << block[OFFSET_A] << " ";
  else
    *stream << "---- ";

  if (hasOffset[OFFSET_B])
    *stream << std::hex << std::setw(4) << block[OFFSET_B] << " ";
  else
    *stream << "---- ";

  if (hasOffset[OFFSET_C] || hasOffset[OFFSET_CI])
    *stream << std::hex << std::setw(4) << block[OFFSET_C] << " ";
  else
    *stream << "---- ";

  if (hasOffset[OFFSET_D])
    *stream << std::hex << std::setw(4) << block[OFFSET_D];
  else
    *stream << "----";

  *stream << std::endl;
}

AltFreqList::AltFreqList() : alt_freqs_(), num_alt_freqs_(0),
                             lf_mf_follows_(false) {
}

void AltFreqList::add(uint8_t af_code) {
  CarrierFrequency freq(af_code, lf_mf_follows_);
  lf_mf_follows_ = false;

  if (freq.isValid()) {
    alt_freqs_.insert(freq);
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

bool AltFreqList::hasAll() const {
  return (static_cast<int>(alt_freqs_.size()) == num_alt_freqs_ &&
          num_alt_freqs_ > 0);
}

std::set<CarrierFrequency> AltFreqList::get() const {
  return alt_freqs_;
}

void AltFreqList::clear() {
  alt_freqs_.clear();
}

Station::Station() : Station(0x0000, Options()) {
}

Station::Station(uint16_t _pi, Options options) : pi_(_pi), options_(options),
  ps_(8), rt_(64), rt_ab_(0), pty_(0), is_tp_(false), is_ta_(false),
  is_music_(false), pin_(0), ecc_(0), cc_(0),
  tmc_id_(0), ews_channel_(0), lang_(0), linkage_la_(0), clock_time_(""),
  has_country_(false), oda_app_for_group_(), has_rt_plus_(false),
  rt_plus_toggle_(false), rt_plus_item_running_(false),
  last_block_had_pi_(false), alt_freq_list_(), pager_pac_(0), pager_opc_(0),
  pager_tng_(0), pager_ecc_(0), pager_ccf_(0), pager_interval_(0),
  writer_builder_(), json_()
#ifdef ENABLE_TMC
                    , tmc_()
#endif
{
  writer_builder_["indentation"] = "";
  writer_builder_["precision"] = 1;
  writer_ =
      std::unique_ptr<Json::StreamWriter>(writer_builder_.newStreamWriter());
}

void Station::updateAndPrint(const Group& group, std::ostream* stream) {

  // Allow 1 group with missed PI
  if (group.hasPi) {
    last_block_had_pi_ = true;
  } else if (!last_block_had_pi_) {
    return;
  } else if (last_block_had_pi_) {
    last_block_had_pi_ = false;
  }

  json_.clear();
  json_["pi"] = "0x" + hexString(group.pi, 4);

  decodeBasics(group);

  if (group.hasType) {
    if      (group.type.num == 0)
      decodeType0(group);
    else if (group.type.num == 1)
      decodeType1(group);
    else if (group.type.num == 2)
      decodeType2(group);
    else if (group.type.num == 3 && group.type.ab == VERSION_A)
      decodeType3A(group);
    else if (group.type.num == 4 && group.type.ab == VERSION_A)
      decodeType4A(group);
    else if (group.type.num == 14 && group.type.ab == VERSION_A)
      decodeType14A(group);
    else if (group.type.num == 15 && group.type.ab == VERSION_B)
      decodeType15B(group);
    else if (oda_app_for_group_.count(group.type) > 0)
      decodeODAgroup(group);
    else if (group.type.num == 6)
      decodeType6(group);
    else
      json_["debug"].append("TODO " + group.type.toString());
  }

  writer_->write(json_, stream);

  *stream << std::endl;
}

bool Station::hasPS() const {
  return ps_.isComplete();
}

std::string Station::getPS() const {
  return ps_.getLastCompleteString();
}

std::string Station::getRT() const {
  return rt_.getLastCompleteString();
}

uint16_t Station::getPI() const {
  return pi_;
}

std::string Station::getCountryCode() const {
  return getCountryString(pi_, ecc_);
}

void Station::updatePS(int pos, std::vector<int> chars) {
  for (size_t i=pos; i < pos+chars.size(); i++)
    ps_.setAt(i, chars[i-pos]);

  if (ps_.isComplete())
    json_["ps"] = ps_.getLastCompleteStringTrimmed();
  else if (options_.show_partial)
    json_["partial_ps"] = ps_.getString();
}

void Station::updateRadioText(int pos, std::vector<int> chars) {
  for (size_t i=pos; i < pos+chars.size(); i++)
    rt_.setAt(i, chars[i-pos]);
}

void Station::decodeBasics (const Group& group) {

  if (group.hasOffset[OFFSET_B]) {
    is_tp_ = bits(group.block[OFFSET_B], 10, 1);
    pty_   = bits(group.block[OFFSET_B],  5, 5);

    json_["group"] = group.type.toString();
    json_["tp"] = is_tp_;
    json_["prog_type"] = getPTYname(pty_, options_.rbds);
  } else if (group.type.num == 15 && group.type.ab == VERSION_B &&
      group.hasOffset[OFFSET_D]) {
    is_tp_ = bits(group.block[OFFSET_D], 10, 1);
    pty_   = bits(group.block[OFFSET_D],  5, 5);

    json_["group"] = group.type.toString();
    json_["tp"] = is_tp_;
    json_["prog_type"] = getPTYname(pty_, options_.rbds);
  }
}

// Group 0: Basic tuning and switching information
void Station::decodeType0 (const Group& group) {
  // not implemented: Decoder Identification
  is_ta_    = bits(group.block[OFFSET_B], 4, 1);
  is_music_ = bits(group.block[OFFSET_B], 3, 1);

  json_["ta"] = is_ta_;
  json_["is_music"] = is_music_;

  if (!group.hasOffset[OFFSET_C])
    return;

  if (group.type.ab == VERSION_A) {
    for (int i=0; i<2; i++)
      alt_freq_list_.add(bits(group.block[OFFSET_C], 8-i*8, 8));

    if (alt_freq_list_.hasAll()) {
      for (auto f : alt_freq_list_.get())
        json_["alt_freqs"].append(f.getString());
      alt_freq_list_.clear();
    }
  }

  if (!group.hasOffset[OFFSET_D])
    return;

  updatePS(bits(group.block[OFFSET_B], 0, 2) * 2,
      { bits(group.block[OFFSET_D], 8, 8), bits(group.block[OFFSET_D], 0, 8) });
}

// Group 1: Programme Item Number and slow labelling codes
void Station::decodeType1 (const Group& group) {
  if (!(group.hasOffset[OFFSET_C] && group.hasOffset[OFFSET_D]))
    return;

  pin_ = group.block[OFFSET_D];

  if (pin_ != 0x0000) {
    uint16_t dy = bits(pin_, 11, 5);
    uint16_t hr = bits(pin_, 6, 5);
    uint16_t mn = bits(pin_, 0, 6);
    if (dy >= 1 && hr <= 24 && mn <= 59) {
      json_["prog_item_started"]["day"] = dy;
      json_["prog_item_started"]["time"] = hoursMinutesString(hr, mn);
    } else {
      json_["debug"].append("invalid PIN");
    }
  }

  if (group.type.ab == VERSION_A) {
    pager_tng_ = bits(group.block[OFFSET_B], 2, 3);
    if (pager_tng_ != 0) {
      pager_interval_ = bits(group.block[OFFSET_B], 0, 2);
    }
    linkage_la_ = bits(group.block[OFFSET_C], 15, 1);
    json_["has_linkage"] = linkage_la_;

    int slc_variant = bits(group.block[OFFSET_C], 12, 3);

    if (slc_variant == 0) {
      if (pager_tng_ != 0) {
        pager_opc_ = bits(group.block[OFFSET_C], 8, 4);
      }

      // No PIN, section M.3.2.4.3
      if (group.hasOffset[OFFSET_D] && (group.block[OFFSET_D] >> 11) == 0) {
        int subtype = bits(group.block[OFFSET_D], 10, 1);
        if (subtype == 0) {
          if (pager_tng_ != 0) {
            pager_pac_ = bits(group.block[OFFSET_D], 4, 6);
            pager_opc_ = bits(group.block[OFFSET_D], 0, 4);
          }
        } else if (subtype == 1) {
          if (pager_tng_ != 0) {
            int b = bits(group.block[OFFSET_D], 8, 2);
            if (b == 0) {
              pager_ecc_ = bits(group.block[OFFSET_D], 0, 6);
            } else if (b == 3) {
              pager_ccf_ = bits(group.block[OFFSET_D], 0, 4);
            }
          }
        }
      }

      ecc_ = bits(group.block[OFFSET_C],  0, 8);
      cc_  = bits(group.block[OFFSET_A], 12, 4);

      if (ecc_ != 0x00) {
        has_country_ = true;

        json_["country"] = getCountryString(pi_, ecc_);
      }

    } else if (slc_variant == 1) {
      tmc_id_ = bits(group.block[OFFSET_C], 0, 12);
      json_["tmc_id"] = tmc_id_;

    } else if (slc_variant == 2) {
      if (pager_tng_ != 0) {
        pager_pac_ = bits(group.block[OFFSET_C], 0, 6);
        pager_opc_ = bits(group.block[OFFSET_C], 8, 4);
      }

      // No PIN, section M.3.2.4.3
      if (group.hasOffset[OFFSET_D] && (group.block[OFFSET_D] >> 11) == 0) {
        int subtype = bits(group.block[OFFSET_D], 10, 1);
        if (subtype == 0) {
          if (pager_tng_ != 0) {
            pager_pac_ = bits(group.block[OFFSET_D], 4, 6);
            pager_opc_ = bits(group.block[OFFSET_D], 0, 4);
          }
        } else if (subtype == 1) {
          if (pager_tng_ != 0) {
            int b = bits(group.block[OFFSET_D], 8, 2);
            if (b == 0) {
              pager_ecc_ = bits(group.block[OFFSET_D], 0, 6);
            } else if (b == 3) {
              pager_ccf_ = bits(group.block[OFFSET_D], 0, 4);
            }
          }
        }
      }

    } else if (slc_variant == 3) {
      lang_ = bits(group.block[OFFSET_C], 0, 8);
      json_["language"] = getLanguageString(lang_);

    } else if (slc_variant == 7) {
      ews_channel_ = bits(group.block[OFFSET_C], 0, 12);
      json_["ews"] = ews_channel_;

    } else {
      json_["debug"].append("TODO: SLC variant " +
          std::to_string(slc_variant));
    }
  }
}

// Group 2: RadioText
void Station::decodeType2 (const Group& group) {

  if (!((group.hasOffset[OFFSET_C] || group.hasOffset[OFFSET_CI]) &&
         group.hasOffset[OFFSET_D]))
    return;

  int rt_position = bits(group.block[OFFSET_B], 0, 4) *
    (group.type.ab == VERSION_A ? 4 : 2);
  int prev_textAB = rt_ab_;
  rt_ab_          = bits(group.block[OFFSET_B], 4, 1);

  if (prev_textAB != rt_ab_)
    rt_.clear();

  std::string chars;

  if (group.type.ab == VERSION_A) {
    rt_.resize(64);
    updateRadioText(rt_position,
        {bits(group.block[OFFSET_C], 8, 8), bits(group.block[OFFSET_C], 0, 8)});
  } else {
    rt_.resize(32);
  }

  if (group.hasOffset[OFFSET_D]) {
    updateRadioText(rt_position + (group.type.ab == VERSION_A ? 2 : 0),
        {bits(group.block[OFFSET_D], 8, 8), bits(group.block[OFFSET_D], 0, 8)});
  }

  if (rt_.isComplete())
    json_["radiotext"] = rt_.getLastCompleteStringTrimmed();
  else if (options_.show_partial && rt_.getTrimmedString().length() > 0)
    json_["partial_radiotext"] = rt_.getTrimmedString();
}

// Group 3A: Application identification for Open Data
void Station::decodeType3A (const Group& group) {

  if (!(group.hasOffset[OFFSET_C] && group.hasOffset[OFFSET_D]))
    return;

  if (group.type.ab != VERSION_A)
    return;

  GroupType oda_group(bits(group.block[OFFSET_B], 0, 5));
  uint16_t oda_msg = group.block[OFFSET_C];
  uint16_t oda_aid = group.block[OFFSET_D];

  oda_app_for_group_[oda_group] = oda_aid;

  json_["open_data_app"]["oda_group"] = oda_group.toString();
  json_["open_data_app"]["app_name"] = getAppName(oda_aid);

  if (oda_aid == 0xCD46 || oda_aid == 0xCD47) {
#ifdef ENABLE_TMC
    tmc_.systemGroup(group.block[OFFSET_C], &json_);
#else
    json_["debug"].append("redsea compiled without TMC support");
#endif
  } else if (oda_aid == 0x4BD7) {
    has_rt_plus_ = true;
    rt_plus_cb_ = bits(group.block[OFFSET_C], 12, 1);
    rt_plus_scb_ = bits(group.block[OFFSET_C], 8, 4);
    rt_plus_template_num_ = bits(group.block[OFFSET_C], 0, 8);
  } else {
    json_["debug"].append("TODO: Unimplemented ODA app " +
        std::to_string(oda_aid));
    json_["open_data_app"]["message"] = oda_msg;
  }
}

// Group 4A: Clock-time and date
void Station::decodeType4A (const Group& group) {

  if (!(group.hasOffset[OFFSET_C] && group.hasOffset[OFFSET_D]))
    return;

  int mjd = (bits(group.block[OFFSET_B], 0, 2) << 15) +
             bits(group.block[OFFSET_C], 1, 15);
  double lto = 0.0;

  if (group.hasOffset[OFFSET_D]) {
    lto = (bits(group.block[OFFSET_D], 5, 1) ? -1 : 1) *
           bits(group.block[OFFSET_D], 0, 5) / 2.0;
    mjd = mjd + lto / 24.0;
  }

  int yr = (mjd - 15078.2) / 365.25;
  int mo = (mjd - 14956.1 - std::trunc(yr * 365.25)) / 30.6001;
  int dy = mjd - 14956 - std::trunc(yr * 365.25) - std::trunc(mo * 30.6001);
  if (mo == 14 || mo == 15) {
    yr += 1;
    mo -= 12;
  }
  yr += 1900;
  mo -= 1;

  if (group.hasOffset[OFFSET_D]) {
    int ltom = (lto - std::trunc(lto)) * 60;

    int hr = int((bits(group.block[OFFSET_C], 0, 1) << 4) +
        bits(group.block[OFFSET_D], 12, 14) + lto) % 24;
    int mn = bits(group.block[OFFSET_D], 6, 6) + ltom;

    if (mo >= 1 && mo <= 12 && dy >= 1 && dy <= 31 && hr >= 0 && hr <= 23 &&
        mn >= 0 && mn <= 59 && fabs(std::trunc(lto)) <= 13.0) {
      char buff[100];
      snprintf(buff, sizeof(buff),
          "%04d-%02d-%02dT%02d:%02d:00%s%02.0f:%02d",
          yr, mo, dy, hr, mn, lto > 0 ? "+" : "-", fabs(std::trunc(lto)),
          abs(ltom));
      clock_time_ = buff;
      json_["clock_time"] = clock_time_;
    } else {
      json_["debug"].append("invalid date/time");
    }
  }
}

// Group 6: In-house applications
void Station::decodeType6 (const Group& group) {
  json_["in_house_data"].append(bits(group.block[OFFSET_B], 0, 5));

  if (group.type.ab == VERSION_A) {
    if (group.hasOffset[OFFSET_C] || group.hasOffset[OFFSET_CI]) {
      json_["in_house_data"].append(bits(group.block[OFFSET_C], 0, 16));
    } else {
      json_["in_house_data"].append("(not received)");
    }
    if (group.hasOffset[OFFSET_D]) {
      json_["in_house_data"].append(bits(group.block[OFFSET_D], 0, 16));
    } else {
      json_["in_house_data"].append("(not received)");
    }
  } else {
    if (group.hasOffset[OFFSET_C] || group.hasOffset[OFFSET_CI]) {
      json_["in_house_data"].append(bits(group.block[OFFSET_D], 0, 16));
    } else {
      json_["in_house_data"].append("(not received)");
    }
  }
}

// Group 14A: Enhanced Other Networks information
void Station::decodeType14A (const Group& group) {

  if (!(group.hasOffset[OFFSET_C] || group.hasOffset[OFFSET_D]))
    return;

  uint16_t pi = group.block[OFFSET_D];
  bool tp = bits(group.block[OFFSET_B], 4, 1);

  json_["other_network"]["pi"] = "0x" + hexString(pi, 4);
  json_["other_network"]["tp"] = tp;

  uint16_t eon_variant = bits(group.block[OFFSET_B], 0, 4);

  if (eon_variant <= 3) {

    if (eon_ps_names_.count(pi) == 0)
      eon_ps_names_[pi] = RDSString(8);

    eon_ps_names_[pi].setAt(2*eon_variant,   bits(group.block[OFFSET_C], 8, 8));
    eon_ps_names_[pi].setAt(2*eon_variant+1, bits(group.block[OFFSET_C], 0, 8));

    if (eon_ps_names_[pi].isComplete())
      json_["other_network"]["ps"] = eon_ps_names_[pi].getLastCompleteString();

  } else if (eon_variant == 4) {
    eon_alt_freqs_[pi].add(bits(group.block[OFFSET_C], 8, 8));
    eon_alt_freqs_[pi].add(bits(group.block[OFFSET_C], 0, 8));

    if (eon_alt_freqs_[pi].hasAll()) {
      for (auto f : eon_alt_freqs_[pi].get())
        json_["other_network"]["alt_freqs"].append(f.getString());
      eon_alt_freqs_[pi].clear();
    }

  } else if (eon_variant >= 5 && eon_variant <= 9) {

    CarrierFrequency f_other(bits(group.block[OFFSET_C], 0, 8));

    if (f_other.isValid())
      json_["other_network"]["frequency"] = f_other.getString();

  } else if (eon_variant == 12) {

    bool has_linkage = bits(group.block[OFFSET_C], 15, 1);
    uint16_t lsn = bits(group.block[OFFSET_C], 0, 12);
    json_["other_network"]["has_linkage"] = has_linkage;
    if (has_linkage && lsn != 0)
      json_["other_network"]["linkage_set"] = lsn;

  } else if (eon_variant == 13) {
    uint16_t pty = bits(group.block[OFFSET_C], 11, 5);
    bool ta      = bits(group.block[OFFSET_C], 0, 1);
    json_["other_network"]["prog_type"] = getPTYname(pty, options_.rbds);
    json_["other_network"]["ta"] = ta;

  } else if (eon_variant == 14) {

    uint16_t pin = group.block[OFFSET_C];

    if (pin != 0x0000) {
      json_["other_network"]["prog_item_started"]["day"] = bits(pin, 11, 5);
      json_["other_network"]["prog_item_started"]["time"] =
          hoursMinutesString(bits(pin, 6,5), bits(pin, 0, 6));
    }

  } else {
    json_["debug"].append("TODO: EON variant " +
        std::to_string(bits(group.block[OFFSET_B], 0, 4)));
  }
}

/* Group 15B: Fast basic tuning and switching information */
void Station::decodeType15B(const Group& group) {
  is_ta_    = bits(group.block[OFFSET_B], 4, 1);
  is_music_ = bits(group.block[OFFSET_B], 3, 1);

  json_["ta"] = is_ta_;
  json_["is_music"] = is_music_;
}

/* Open Data Application */
void Station::decodeODAgroup(const Group& group) {

  if (oda_app_for_group_.count(group.type) == 0)
    return;

  uint16_t aid = oda_app_for_group_[group.type];

  if (aid == 0xCD46 || aid == 0xCD47) {
#ifdef ENABLE_TMC
    tmc_.userGroup(bits(group.block[OFFSET_B], 0, 5), group.block[OFFSET_C],
        group.block[OFFSET_D], &json_);
#endif
  } else if (aid == 0x4BD7) {
    parseRadioTextPlus(group);
  }
}

void Station::parseRadioTextPlus(const Group& group) {
  bool item_toggle  = bits(group.block[OFFSET_B], 4, 1);
  bool item_running = bits(group.block[OFFSET_B], 3, 1);

  if (item_toggle != rt_plus_toggle_ || item_running != rt_plus_item_running_) {
    rt_.clear();
    rt_plus_toggle_ = item_toggle;
    rt_plus_item_running_ = item_running;
  }

  json_["radiotext_plus"]["item_running"] = item_running;

  std::vector<RTPlusTag> tags(2);

  tags[0].content_type = (bits(group.block[OFFSET_B], 0, 3) << 3) +
                          bits(group.block[OFFSET_C], 13, 3);
  tags[0].start  = bits(group.block[OFFSET_C], 7, 6);
  tags[0].length = bits(group.block[OFFSET_C], 1, 6) + 1;

  tags[1].content_type = (bits(group.block[OFFSET_C], 0, 1) << 5) +
                          bits(group.block[OFFSET_D], 11, 5);
  tags[1].start  = bits(group.block[OFFSET_D], 5, 6);
  tags[1].length = bits(group.block[OFFSET_D], 0, 5) + 1;

  for (RTPlusTag tag : tags) {
    std::string text =
      rt_.getLastCompleteStringTrimmed(tag.start, tag.length);

    if (rt_.hasChars(tag.start, tag.length) && text.length() > 0 &&
        tag.content_type != 0)
      json_["radiotext_plus"][getRTPlusContentTypeName(tag.content_type)] =
          text;
  }
}

}  // namespace redsea
