#include "groups.h"

#include <cassert>
#include <iostream>
#include <map>
#include <string>

#include "rdsstring.h"
#include "tables.h"
#include "util.h"

namespace redsea {

namespace {

  bool isFMFrequency(uint16_t af_code) {
    return (af_code >= 1 && af_code <= 204);
  }

  float getFMFrequency(uint16_t af_code) {
    return 87.5 + af_code / 10.0;
  }

}

GroupType::GroupType(uint16_t type_code) : num((type_code >> 1) & 0xF),
  ab(type_code & 0x1) {}
GroupType::GroupType(const GroupType& obj) : num(obj.num), ab(obj.ab) {}

std::string GroupType::toString() {
  return std::string(std::to_string(num) + (ab == TYPE_A ? "A" : "B"));
}

bool operator==(const GroupType& obj1, const GroupType& obj2) {
  return (obj1.num == obj2.num && obj1.ab == obj2.ab);
}

bool operator<(const GroupType& obj1, const GroupType& obj2) {
  return ((obj1.num < obj2.num) || (obj1.ab < obj2.ab));
}

Group::Group(std::vector<uint16_t> blockbits) :
    type(blockbits.size() > 1 ? bits(blockbits.at(1), 11, 5) : 0),
    num_blocks(blockbits.size()),
    block1(num_blocks > 0 ? blockbits[0] : 0x00),
    block2(num_blocks > 1 ? blockbits[1] : 0x00),
    block3(num_blocks > 2 ? blockbits[2] : 0x00),
    block4(num_blocks > 3 ? blockbits[3] : 0x00) {

}

void Group::printHex() const {
  if (num_blocks > 0)
    printf("%04X ", block1);
  else
    printf("---- ");

  if (num_blocks > 1)
    printf("%04X ", block2);
  else
    printf("---- ");

  if (num_blocks > 2)
    printf("%04X ", block3);
  else
    printf("---- ");

  if (num_blocks > 3)
    printf("%04X", block4);
  else
    printf("----");

  printf("\n");

  fflush(stdout);
}

Station::Station() : Station(0x0000) {

}

Station::Station(uint16_t _pi) : pi_(_pi), ps_(8), rt_(64), rt_ab_(0), pty_(0),
  is_tp_(false), is_ta_(false), is_music_(false), alt_freqs_(),
  num_alt_freqs_(0), pin_(0), ecc_(0), cc_(0), tmc_id_(0), ews_channel_(0),
  lang_(0), linkage_la_(0), clock_time_(""), has_country_(false),
  oda_app_for_group_(), has_rt_plus_(false), rt_plus_toggle_(false),
  rt_plus_item_running_(false), pager_pac_(0), pager_opc_(0), pager_tng_(0),
  pager_ecc_(0), pager_ccf_(0), pager_interval_(0), tmc_() {

}

void Station::update(Group group) {

  if (group.num_blocks == 0)
    return;

  printf("{\"pi\":\"0x%04x\"", pi_);

  if (group.num_blocks < 2) {
    printf("}\n");
    return;
  }

  printf(",\"group\":\"%s\"", group.type.toString().c_str());

  is_tp_   = bits(group.block2, 10, 1);
  pty_     = bits(group.block2,  5, 5);

  printf(",\"tp\":\"%s\"", is_tp_ ? "true" : "false");
  printf(",\"prog_type\":\"%s\"", getPTYname(pty_).c_str());

  if      (group.type.num == 0)
    decodeType0(group);
  else if (group.type.num == 1)
    decodeType1(group);
  else if (group.type.num == 2)
    decodeType2(group);
  else if (group.type.num == 3 && group.type.ab == TYPE_A)
    decodeType3A(group);
  else if (group.type.num == 4 && group.type.ab == TYPE_A)
    decodeType4A(group);
  else if (group.type.num == 14 && group.type.ab == TYPE_A)
    decodeType14A(group);
  else if (oda_app_for_group_.count(group.type) > 0)
    decodeODAgroup(group);
  else if (group.type.num == 6)
    decodeType6(group);
  else
    printf(" /* TODO */ ");

  printf("}\n");

  fflush(stdout);
}

void Station::addAltFreq(uint8_t af_code) {
  if (isFMFrequency(af_code)) {
    alt_freqs_.insert(getFMFrequency(af_code));
  } else if (af_code == 205) {
    // filler
  } else if (af_code == 224) {
    // no AF exists
  } else if (af_code >= 225 && af_code <= 249) {
    num_alt_freqs_ = af_code - 224;
  } else if (af_code == 250) {
    // AM/LF freq follows
  }
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

  for (int i=pos; i<pos+(int)chars.size(); i++)
    ps_.setAt(i, chars[i-pos]);

  if (ps_.isComplete())
    printf(",\"ps\":\"%s\"",ps_.getLastCompleteString().c_str());

}

void Station::updateRadioText(int pos, std::vector<int> chars) {

  for (int i=pos; i<pos+(int)chars.size(); i++)
    rt_.setAt(i, chars[i-pos]);

}

// Group 0: Basic tuning and switching information
void Station::decodeType0 (const Group& group) {

  // not implemented: Decoder Identification

  is_ta_    = bits(group.block2, 4, 1);
  is_music_ = bits(group.block2, 3, 1);

  printf(",\"ta\":\"%s\"", is_ta_ ? "true" : "false");

  if (group.num_blocks < 3)
    return;

  if (group.type.ab == TYPE_A) {
    for (int i=0; i<2; i++) {
      addAltFreq(bits(group.block3, 8-i*8, 8));
    }

    if ((int)alt_freqs_.size() == num_alt_freqs_ && num_alt_freqs_ > 0) {
      printf(",\"alt_freqs\":[");
      int i = 0;
      for (auto f : alt_freqs_) {
        printf("\"%.1f\"", f);
        if (i < (int)alt_freqs_.size() - 1)
          printf(",");
        i++;
      }
      printf("]");
      alt_freqs_.clear();
    }
  }

  if (group.num_blocks < 4)
    return;

  updatePS(bits(group.block2, 0, 2) * 2,
      { bits(group.block4, 8, 8), bits(group.block4, 0, 8) });

}

// Group 1: Programme Item Number and slow labelling codes
void Station::decodeType1 (const Group& group) {

  if (group.num_blocks < 4)
    return;

  pin_ = group.block4;

  if (pin_ != 0x0000)
    printf(",\"prog_item_started\":{\"day\":%d,\"time\":\"%02d:%02d\"}",
        bits(pin_, 11, 5), bits(pin_, 6, 5), bits(pin_, 0, 6) );

  if (group.type.ab == TYPE_A) {
    pager_tng_ = bits(group.block2, 2, 3);
    if (pager_tng_ != 0) {
      pager_interval_ = bits(group.block2, 0, 2);
    }
    linkage_la_ = bits(group.block3, 15, 1);
    printf(",\"has_linkage\":\"%s\"", linkage_la_ ? "true" : "false");

    int slc_variant = bits(group.block3, 12, 3);

    if (slc_variant == 0) {
      if (pager_tng_ != 0) {
        pager_opc_ = bits(group.block3, 8, 4);
      }

      // No PIN, section M.3.2.4.3
      if (group.num_blocks == 4 && (group.block4 >> 11) == 0) {
        int subtype = bits(group.block4, 10, 1);
        if (subtype == 0) {
          if (pager_tng_ != 0) {
            pager_pac_ = bits(group.block4, 4, 6);
            pager_opc_ = bits(group.block4, 0, 4);
          }
        } else if (subtype == 1) {
          if (pager_tng_ != 0) {
            int b = bits(group.block4, 8, 2);
            if (b == 0) {
              pager_ecc_ = bits(group.block4, 0, 6);
            } else if (b == 3) {
              pager_ccf_ = bits(group.block4, 0, 4);
            }
          }
        }
      }

      ecc_ = bits(group.block3,  0, 8);
      cc_  = bits(group.block1, 12, 4);

      if (ecc_ != 0x00) {
        has_country_ = true;

        printf(",\"country\":\"%s\"", getCountryString(pi_, ecc_).c_str());
      }

    } else if (slc_variant == 1) {
      tmc_id_ = bits(group.block3, 0, 12);
      printf(",\"tmc_id\":\"0x%03x\"", tmc_id_);

    } else if (slc_variant == 2) {
      if (pager_tng_ != 0) {
        pager_pac_ = bits(group.block3, 0, 6);
        pager_opc_ = bits(group.block3, 8, 4);
      }

      // No PIN, section M.3.2.4.3
      if (group.num_blocks == 4 && (group.block4 >> 11) == 0) {
        int subtype = bits(group.block4, 10, 1);
        if (subtype == 0) {
          if (pager_tng_ != 0) {
            pager_pac_ = bits(group.block4, 4, 6);
            pager_opc_ = bits(group.block4, 0, 4);
          }
        } else if (subtype == 1) {
          if (pager_tng_ != 0) {
            int b = bits(group.block4, 8, 2);
            if (b == 0) {
              pager_ecc_ = bits(group.block4, 0, 6);
            } else if (b == 3) {
              pager_ccf_ = bits(group.block4, 0, 4);
            }
          }
        }
      }

    } else if (slc_variant == 3) {
      lang_ = bits(group.block3, 0, 8);
      printf(",\"language\":\"%s\"", getLanguageString(lang_).c_str());

    } else if (slc_variant == 7) {
      ews_channel_ = bits(group.block3, 0, 12);
      printf(",\"ews\":\"0x%03x\"", ews_channel_);

    } else {
      printf(" /* TODO: SLC variant %d */", slc_variant);
    }

  }

}

// Group 2: RadioText
void Station::decodeType2 (const Group& group) {

  if (group.num_blocks < 3)
    return;

  int rt_position = bits(group.block2, 0, 4) *
    (group.type.ab == TYPE_A ? 4 : 2);
  int prev_textAB = rt_ab_;
  rt_ab_          = bits(group.block2, 4, 1);

  if (prev_textAB != rt_ab_)
    rt_.clear();

  std::string chars;

  if (group.type.ab == TYPE_A) {
    updateRadioText(rt_position,
        {bits(group.block3, 8, 8), bits(group.block3, 0, 8)});
  }

  if (group.num_blocks == 4) {
    updateRadioText(rt_position+2,
        {bits(group.block4, 8, 8), bits(group.block4, 0, 8)});
  }

  if (rt_.isComplete())
    printf(",\"radiotext\":\"%s\"",rt_.getLastCompleteStringTrimmed().c_str());

}

// Group 3A: Application identification for Open Data
void Station::decodeType3A (const Group& group) {

  if (group.num_blocks < 4)
    return;

  if (group.type.ab != TYPE_A)
    return;

  GroupType oda_group(bits(group.block2, 0, 5));
  uint16_t oda_msg = group.block3;
  uint16_t oda_aid = group.block4;

  oda_app_for_group_[oda_group] = oda_aid;

  printf(",\"open_data_app\":{\"oda_group\":\"%s\",\"app_name\":\"%s\"",
      oda_group.toString().c_str(), getAppName(oda_aid).c_str());

  if (oda_aid == 0xCD46 || oda_aid == 0xCD47) {
    tmc_.systemGroup(group.block3);
  } else if (oda_aid == 0x4BD7) {
    has_rt_plus_ = true;
    rt_plus_cb_ = bits(group.block3, 12, 1);
    rt_plus_scb_ = bits(group.block3, 8, 4);
    rt_plus_template_num_ = bits(group.block3, 0, 8);
  } else {
    printf(" /* TODO: Unimplemented ODA app */ ,\"message\":\"0x%02x\"",
        oda_msg);
  }

  printf("}");

}

// Group 4A: Clock-time and date
void Station::decodeType4A (const Group& group) {

  if (group.num_blocks < 3 || group.type.ab == TYPE_B)
    return;

  int mjd = (bits(group.block2, 0, 2) << 15) + bits(group.block3, 1, 15);
  double lto;

  if (group.num_blocks == 4) {
    lto = (bits(group.block4, 5, 1) ? -1 : 1) * bits(group.block4, 0, 5) / 2.0;
    mjd = int(mjd + lto / 24.0);
  }

  int yr = int((mjd - 15078.2) / 365.25);
  int mo = int((mjd - 14956.1 - int(yr * 365.25)) / 30.6001);
  int dy = mjd - 14956 - int(yr * 365.25) - int(mo * 30.6001);
  if (mo == 14 || mo == 15) {
    yr += 1;
    mo -= 12;
  }
  yr += 1900;
  mo -= 1;

  if (group.num_blocks == 4) {
    int ltom = (lto - int(lto)) * 60;

    int hr = int((bits(group.block3, 0, 1) << 4) +
        bits(group.block4, 12, 14) + lto) % 24;
    int mn = bits(group.block4, 6, 6) + ltom;

    if (mo >= 1 && mo <= 12 && dy >= 1 && dy <= 31 && hr >= 0 && hr <= 23 &&
        mn >= 0 && mn <= 59) {
      char buff[100];
      snprintf(buff, sizeof(buff),
          "%04d-%02d-%02dT%02d:%02d:00%+03d:%02d",yr,mo,dy,hr,mn,int(lto),ltom);
      clock_time_ = buff;
      printf(",\"clock_time\":\"%s\"", clock_time_.c_str());
    } else {
      printf("/* invalid date/time */");
    }

  }
}

// Group 6: In-house applications
void Station::decodeType6 (const Group& group) {
  printf(", \"in_house_data\":[\"0x%03x\"",
      bits(group.block2, 0, 5));

  if (group.type.ab == TYPE_A) {
    if (group.num_blocks > 2) {
      printf(",\"0x%04x\"", bits(group.block3, 0, 16));
    } else {
      printf(",\"(not received)\"");
    }
    if (group.num_blocks > 3) {
      printf(",\"0x%04x\"", bits(group.block4, 0, 16));
    } else {
      printf(",\"(not received)\"");
    }
  } else {
    if (group.num_blocks > 3) {
      printf(",\"0x%04x\"", bits(group.block4, 0, 16));
    } else {
      printf(",\"(not received)\"");
    }
  }

  printf("]");

}

// Group 14A: Enhanced Other Networks information
void Station::decodeType14A (const Group& group) {

  if (group.num_blocks < 4)
    return;

  uint16_t pi = group.block4;
  bool tp = bits(group.block2, 4, 1);


  printf(",\"other_network\":{\"pi\":\"0x%04x\",\"tp\":\"%s\"",
      pi, tp ? "true" : "false");

  uint16_t eon_variant = bits(group.block2, 0, 4);

  if (eon_variant <= 3) {

    if (eon_ps_names_.count(pi) == 0)
      eon_ps_names_[pi] = RDSString(8);

    eon_ps_names_[pi].setAt(2*eon_variant,   bits(group.block3,8,8));
    eon_ps_names_[pi].setAt(2*eon_variant+1, bits(group.block3,0,8));

    if (eon_ps_names_[pi].isComplete())
      printf(",\"ps\":\"%s\"",
          eon_ps_names_[pi].getLastCompleteString().c_str());

  } else if (eon_variant >= 5 && eon_variant <= 9) {

    uint16_t f_other = bits(group.block3,0,8);

    if (isFMFrequency(f_other)) {
      printf(",\"frequency\":%.1f", getFMFrequency(f_other));
    }

  } else if (eon_variant == 12) {

    bool has_linkage = bits(group.block3, 15, 1);
    uint16_t lsn = bits(group.block3, 0, 12);
    printf(",\"has_linkage\":\"%s\"", has_linkage ? "true" : "false");
    if (has_linkage && lsn != 0)
      printf(",\"linkage_set\":\"0x%03x\"", lsn);

  } else if (eon_variant == 13) {
    uint16_t pty = bits(group.block3, 11, 5);
    bool ta      = bits(group.block3, 0, 1);
    printf(",\"prog_type\":\"%s\"", getPTYname(pty).c_str());
    printf(",\"ta\":\"%s\"", ta ? "true" : "false");

  } else if (eon_variant == 14) {

    uint16_t pin = group.block3;

    if (pin != 0x0000)
      printf(",\"prog_item_started\":{\"day\":%d,\"time\":\"%02d:%02d\"}",
          bits(pin, 11, 5), bits(pin, 6, 5), bits(pin, 0, 6) );

  } else {
    printf(" /* TODO: EON variant %d */", bits(group.block2,0,4));
  }

  printf("}");

}

/* Open Data Application */
void Station::decodeODAgroup (const Group& group) {

  if (oda_app_for_group_.count(group.type) == 0)
    return;

  uint16_t aid = oda_app_for_group_[group.type];

  if (aid == 0xCD46 || aid == 0xCD47) {
    tmc_.userGroup(bits(group.block2, 0, 5), group.block3, group.block4);
  } else if (aid == 0x4BD7) {
    parseRadioTextPlus(group);
  }

}

void Station::parseRadioTextPlus(const Group& group) {
  bool item_toggle  = bits(group.block2, 4, 1);
  bool item_running = bits(group.block2, 3, 1);

  if (item_toggle != rt_plus_toggle_ || item_running != rt_plus_item_running_) {
    rt_.clear();
    rt_plus_toggle_ = item_toggle;
    rt_plus_item_running_ = item_running;
  }

  printf(",\"radiotext_plus\":{\"item_running\":\"%s\"",
      item_running ? "true" : "false");

  std::vector<RTPlusTag> tags(2);

  tags[0].content_type = (bits(group.block2, 0, 3) << 3) +
                          bits(group.block3, 13, 3);
  tags[0].start  = bits(group.block3, 7, 6);
  tags[0].length = bits(group.block3, 1, 6) + 1;

  tags[1].content_type = (bits(group.block3, 0, 1) << 5) +
                          bits(group.block4, 11, 5);
  tags[1].start  = bits(group.block4, 5, 6);
  tags[1].length = bits(group.block4, 0, 5) + 1;

  for (RTPlusTag tag : tags) {
    std::string text =
      rt_.getLastCompleteStringTrimmed(tag.start, tag.length);

    if (rt_.hasChars(tag.start, tag.length) && text.length() > 0 &&
        tag.content_type != 0) {

      printf(",\"%s\":\"%s\"",
          getRTPlusContentTypeName(tag.content_type).c_str(),
          text.c_str());
    }
  }

  printf("}");

}

} // namespace redsea
