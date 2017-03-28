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

std::string HoursMinutesString(int hr, int mn) {
  std::stringstream ss;
  ss << std::setfill('0') << std::setw(2) << hr << ":" << mn;
  return ss.str();
}

}  // namespace

void PrintHexGroup(const Group& group, std::ostream* stream) {
  stream->fill('0');
  stream->setf(std::ios_base::uppercase);

  if (group.empty())
    return;

  for (eBlockNumber blocknum : {BLOCK1, BLOCK2, BLOCK3, BLOCK4}) {
    if (group.has(blocknum))
      *stream << std::hex << std::setw(4) << group.block(blocknum);
    else
      *stream << "----";

    if (blocknum != BLOCK4)
      *stream << " ";
  }

  *stream << std::endl;
}

GroupType::GroupType(uint16_t type_code) : num((type_code >> 1) & 0xF),
  ab((type_code & 0x1) == 0 ? VERSION_A : VERSION_B) {}
GroupType::GroupType(const GroupType& obj) : num(obj.num), ab(obj.ab) {}

std::string GroupType::str() const {
  return std::string(std::to_string(num) + (ab == VERSION_A ? "A" : "B"));
}

bool operator==(const GroupType& obj1, const GroupType& obj2) {
  return (obj1.num == obj2.num && obj1.ab == obj2.ab);
}

bool operator<(const GroupType& obj1, const GroupType& obj2) {
  return ((obj1.num < obj2.num) || (obj1.ab < obj2.ab));
}

Group::Group() : has_block_({false, false, false, false, false}), block_(5),
                 has_type_(false), has_pi_(false), has_ci_(false) {
}

uint16_t Group::block(eBlockNumber n) const {
  return block_.at(n);
}

bool Group::has(eBlockNumber n) const {
  return has_block_.at(n);
}

bool Group::empty() const {
  return !(has(BLOCK1) || has(BLOCK2) || has(BLOCK3) || has(BLOCK4));
}

uint16_t Group::pi() const {
  return pi_;
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

void Group::set(eBlockNumber n, uint16_t data) {
  block_[n] = data;
  has_block_[n] = true;

  if (n == BLOCK1) {
    pi_ = data;
    has_pi_ = true;
  }

  if (n == BLOCK3 && has_ci_ && !has_pi_) {
    pi_ = data;
    has_pi_ = true;
  }

  if (n == BLOCK4 && has_ci_ && !has_type_) {
    GroupType potential_type(Bits(data, 11, 5));
    if (potential_type.num == 15 && potential_type.ab == VERSION_B) {
      type_ = potential_type;
      has_type_ = true;
    }
  }

  if (n == BLOCK2) {
    type_ = GroupType(Bits(data, 11, 5));
    has_type_ = true;
  }
}

void Group::set_ci(uint16_t data) {
  has_ci_ = true;
  set(BLOCK3, data);
}

AltFreqList::AltFreqList() : alt_freqs_(), num_alt_freqs_(0),
                             lf_mf_follows_(false) {
}

void AltFreqList::insert(uint8_t af_code) {
  CarrierFrequency freq(af_code, lf_mf_follows_);
  lf_mf_follows_ = false;

  if (freq.valid()) {
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

bool AltFreqList::complete() const {
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
                    , tmc_(options)
#endif
{
  writer_builder_["indentation"] = "";
  writer_builder_["precision"] = 5;
  writer_ =
      std::unique_ptr<Json::StreamWriter>(writer_builder_.newStreamWriter());
}

void Station::UpdateAndPrint(const Group& group, std::ostream* stream) {
  // Allow 1 group with missed PI
  if (group.has_pi()) {
    last_block_had_pi_ = true;
  } else if (!last_block_had_pi_) {
    return;
  } else if (last_block_had_pi_) {
    last_block_had_pi_ = false;
  }

  json_.clear();
  json_["pi"] = "0x" + HexString(group.pi(), 4);

  DecodeBasics(group);

  if (group.has_type()) {
    if      (group.type().num == 0)
      DecodeType0(group);
    else if (group.type().num == 1)
      DecodeType1(group);
    else if (group.type().num == 2)
      DecodeType2(group);
    else if (group.type().num == 3 && group.type().ab == VERSION_A)
      DecodeType3A(group);
    else if (group.type().num == 4 && group.type().ab == VERSION_A)
      DecodeType4A(group);
    else if (group.type().num == 14)
      DecodeType14(group);
    else if (group.type().num == 15 && group.type().ab == VERSION_B)
      DecodeType15B(group);
    else if (oda_app_for_group_.count(group.type()) > 0)
      DecodeODAGroup(group);
    else if (group.type().num == 6)
      DecodeType6(group);
    else
      json_["debug"].append("TODO " + group.type().str());
  }

  writer_->write(json_, stream);

  *stream << std::endl;
}

bool Station::has_ps() const {
  return ps_.complete();
}

std::string Station::ps() const {
  return ps_.last_complete_string();
}

std::string Station::rt() const {
  return rt_.last_complete_string();
}

uint16_t Station::pi() const {
  return pi_;
}

std::string Station::country_code() const {
  return CountryString(pi_, ecc_);
}

void Station::UpdatePS(int pos, int char1, int char2) {
  ps_.set(pos, RDSChar(char1), RDSChar(char2));

  if (ps_.complete())
    json_["ps"] = ps_.last_complete_string();
  else if (options_.show_partial)
    json_["partial_ps"] = ps_.str();
}

void Station::UpdateRadioText(int pos, int char1, int char2) {
  rt_.set(pos, RDSChar(char1), RDSChar(char2));
}

void Station::DecodeBasics(const Group& group) {
  if (group.has(BLOCK2)) {
    is_tp_ = Bits(group.block(BLOCK2), 10, 1);
    pty_   = Bits(group.block(BLOCK2),  5, 5);

    json_["group"] = group.type().str();
    json_["tp"] = is_tp_;
    json_["prog_type"] = PTYNameString(pty_, options_.rbds);
  } else if (group.type().num == 15 && group.type().ab == VERSION_B &&
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
  uint16_t seg_address = Bits(group.block(BLOCK2), 0, 2);
  bool is_di = Bits(group.block(BLOCK2), 2, 1);
  json_["di"][DICodeString(seg_address)] = is_di;

  is_ta_    = Bits(group.block(BLOCK2), 4, 1);
  is_music_ = Bits(group.block(BLOCK2), 3, 1);

  json_["ta"] = is_ta_;
  json_["is_music"] = is_music_;

  if (!group.has(BLOCK3))
    return;

  if (group.type().ab == VERSION_A) {
    alt_freq_list_.insert(Bits(group.block(BLOCK3), 8, 8));
    alt_freq_list_.insert(Bits(group.block(BLOCK3), 0, 8));

    if (alt_freq_list_.complete()) {
      for (auto f : alt_freq_list_.get())
        json_["alt_freqs"].append(f.str());
      alt_freq_list_.clear();
    }
  }

  if (!group.has(BLOCK4))
    return;

  UpdatePS(seg_address * 2,
           Bits(group.block(BLOCK4), 8, 8),
           Bits(group.block(BLOCK4), 0, 8));
}

// Group 1: Programme Item Number and slow labelling codes
void Station::DecodeType1(const Group& group) {
  if (!(group.has(BLOCK3) && group.has(BLOCK4)))
    return;

  pin_ = group.block(BLOCK4);

  if (pin_ != 0x0000) {
    uint16_t dy = Bits(pin_, 11, 5);
    uint16_t hr = Bits(pin_, 6, 5);
    uint16_t mn = Bits(pin_, 0, 6);
    if (dy >= 1 && hr <= 24 && mn <= 59) {
      json_["prog_item_started"]["day"] = dy;
      json_["prog_item_started"]["time"] = HoursMinutesString(hr, mn);
    } else {
      json_["debug"].append("invalid PIN");
    }
  }

  if (group.type().ab == VERSION_A) {
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
      cc_  = Bits(group.block(BLOCK1), 12, 4);

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

  int rt_position = Bits(group.block(BLOCK2), 0, 4) *
    (group.type().ab == VERSION_A ? 4 : 2);
  int prev_textAB = rt_ab_;
  rt_ab_          = Bits(group.block(BLOCK2), 4, 1);

  if (prev_textAB != rt_ab_)
    rt_.clear();

  std::string chars;

  if (group.type().ab == VERSION_A) {
    rt_.resize(64);
    UpdateRadioText(rt_position,
                    Bits(group.block(BLOCK3), 8, 8),
                    Bits(group.block(BLOCK3), 0, 8));
  } else {
    rt_.resize(32);
  }

  if (group.has(BLOCK4)) {
    UpdateRadioText(rt_position + (group.type().ab == VERSION_A ? 2 : 0),
                    Bits(group.block(BLOCK4), 8, 8),
                    Bits(group.block(BLOCK4), 0, 8));
  }

  if (rt_.complete())
    json_["radiotext"] = rt_.last_complete_string_trimmed();
  else if (options_.show_partial && rt_.getTrimmedString().length() > 0)
    json_["partial_radiotext"] = rt_.getTrimmedString();
}

// Group 3A: Application identification for Open Data
void Station::DecodeType3A(const Group& group) {
  if (!(group.has(BLOCK3) && group.has(BLOCK4)))
    return;

  if (group.type().ab != VERSION_A)
    return;

  GroupType oda_group_type(Bits(group.block(BLOCK2), 0, 5));
  uint16_t oda_msg = group.block(BLOCK3);
  uint16_t oda_aid = group.block(BLOCK4);

  oda_app_for_group_[oda_group_type] = oda_aid;

  json_["open_data_app"]["oda_group"] = oda_group_type.str();
  json_["open_data_app"]["app_name"] = AppNameString(oda_aid);

  if (oda_aid == 0xCD46 || oda_aid == 0xCD47) {
#ifdef ENABLE_TMC
    tmc_.SystemGroup(group.block(BLOCK3), &json_);
#else
    json_["debug"].append("redsea compiled without TMC support");
#endif
  } else if (oda_aid == 0x4BD7) {
    has_rt_plus_ = true;
    rt_plus_cb_ = Bits(group.block(BLOCK3), 12, 1);
    rt_plus_scb_ = Bits(group.block(BLOCK3), 8, 4);
    rt_plus_template_num_ = Bits(group.block(BLOCK3), 0, 8);
  } else {
    json_["debug"].append("TODO: Unimplemented ODA app " +
        std::to_string(oda_aid));
    json_["open_data_app"]["message"] = oda_msg;
  }
}

// Group 4A: Clock-time and date
void Station::DecodeType4A(const Group& group) {
  if (!(group.has(BLOCK3) && group.has(BLOCK4)))
    return;

  int mjd = (Bits(group.block(BLOCK2), 0, 2) << 15) +
             Bits(group.block(BLOCK3), 1, 15);
  double lto = 0.0;

  if (group.has(BLOCK4)) {
    lto = (Bits(group.block(BLOCK4), 5, 1) ? -1 : 1) *
           Bits(group.block(BLOCK4), 0, 5) / 2.0;
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

  if (group.has(BLOCK4)) {
    int lto_min = (lto - std::trunc(lto)) * 60;

    int hr = static_cast<int>((Bits(group.block(BLOCK3), 0, 1) << 4) +
        Bits(group.block(BLOCK4), 12, 14) + lto) % 24;
    int mn = Bits(group.block(BLOCK4), 6, 6) + lto_min;

    if (mo >= 1 && mo <= 12 && dy >= 1 && dy <= 31 && hr >= 0 && hr <= 23 &&
        mn >= 0 && mn <= 59 && fabs(std::trunc(lto)) <= 13.0) {
      char buff[100];
      int lto_hour = fabs(std::trunc(lto));

      if (lto_hour == 0 && lto_min == 0) {
        snprintf(buff, sizeof(buff), "%04d-%02d-%02dT%02d:%02d:00Z",
                 yr, mo, dy, hr, mn);
      } else {
        snprintf(buff, sizeof(buff),
                 "%04d-%02d-%02dT%02d:%02d:00%s%02d:%02d",
                 yr, mo, dy, hr, mn, lto > 0 ? "+" : "-", lto_hour,
                 abs(lto_min));
      }
      clock_time_ = buff;
      json_["clock_time"] = clock_time_;
    } else {
      json_["debug"].append("invalid date/time");
    }
  }
}

// Group 6: In-house applications
void Station::DecodeType6(const Group& group) {
  json_["in_house_data"].append(Bits(group.block(BLOCK2), 0, 5));

  if (group.type().ab == VERSION_A) {
    if (group.has(BLOCK3)) {
      json_["in_house_data"].append(Bits(group.block(BLOCK3), 0, 16));
      if (group.has(BLOCK4)) {
        json_["in_house_data"].append(Bits(group.block(BLOCK4), 0, 16));
      }
    }
  } else {
    if (group.has(BLOCK3)) {
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

  if (group.type().ab == VERSION_B) {
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
        json_["other_network"]["alt_freqs"].append(freq.str());
      eon_alt_freqs_[on_pi].clear();
    }

  } else if (eon_variant >= 5 && eon_variant <= 9) {
    CarrierFrequency freq_other(Bits(group.block(BLOCK3), 0, 8));

    if (freq_other.valid())
      json_["other_network"]["frequency"] = freq_other.str();

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
  eBlockNumber block = group.has(BLOCK2) ? BLOCK2 : BLOCK4;

  is_ta_    = Bits(group.block(block), 4, 1);
  is_music_ = Bits(group.block(block), 3, 1);

  json_["ta"] = is_ta_;
  json_["is_music"] = is_music_;
}

/* Open Data Application */
void Station::DecodeODAGroup(const Group& group) {
  if (oda_app_for_group_.count(group.type()) == 0)
    return;

  uint16_t aid = oda_app_for_group_[group.type()];

  if (aid == 0xCD46 || aid == 0xCD47) {
#ifdef ENABLE_TMC
    tmc_.UserGroup(Bits(group.block(BLOCK2), 0, 5), group.block(BLOCK3),
        group.block(BLOCK4), &json_);
#endif
  } else if (aid == 0x4BD7) {
    ParseRadioTextPlus(group);
  }
}

void Station::ParseRadioTextPlus(const Group& group) {
  bool item_toggle  = Bits(group.block(BLOCK2), 4, 1);
  bool item_running = Bits(group.block(BLOCK2), 3, 1);

  if (item_toggle != rt_plus_toggle_ || item_running != rt_plus_item_running_) {
    rt_.clear();
    rt_plus_toggle_ = item_toggle;
    rt_plus_item_running_ = item_running;
  }

  json_["radiotext_plus"]["item_running"] = item_running;

  std::vector<RTPlusTag> tags(2);

  tags[0].content_type = (Bits(group.block(BLOCK2), 0, 3) << 3) +
                          Bits(group.block(BLOCK3), 13, 3);
  tags[0].start  = Bits(group.block(BLOCK3), 7, 6);
  tags[0].length = Bits(group.block(BLOCK3), 1, 6) + 1;

  tags[1].content_type = (Bits(group.block(BLOCK3), 0, 1) << 5) +
                          Bits(group.block(BLOCK4), 11, 5);
  tags[1].start  = Bits(group.block(BLOCK4), 5, 6);
  tags[1].length = Bits(group.block(BLOCK4), 0, 5) + 1;

  for (RTPlusTag tag : tags) {
    std::string text =
      rt_.last_complete_string_trimmed(tag.start, tag.length);

    if (rt_.has_chars(tag.start, tag.length) && text.length() > 0 &&
        tag.content_type != 0)
      json_["radiotext_plus"][RTPlusContentTypeString(tag.content_type)] =
          text;
  }
}

}  // namespace redsea
