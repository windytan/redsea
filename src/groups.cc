#include "groups.h"

#include <iostream>
#include <string>
#include <vector>
#include <cassert>

namespace redsea {

std::string getLCDchar(int code) {
  assert (code >= 32);
  const std::vector<std::string> char_map ({
      " ","!","\\\"","#","¤","%","&","'","(",")","*","+",",","-",".","/",
      "0","1","2","3","4","5","6","7","8","9",":",";","<","=",">","?",
      "@","A","B","C","D","E","F","G","H","I","J","K","L","M","N","O",
      "P","Q","R","S","T","U","V","W","X","Y","Z","[","\\","]","―","_",
      "‖","a","b","c","d","e","f","g","h","i","j","k","l","m","n","o",
      "p","q","r","s","t","u","v","w","x","y","z","{","|","}","¯"," ",
      "á","à","é","è","í","ì","ó","ò","ú","ù","Ñ","Ç","Ş","β","¡","Ĳ",
      "â","ä","ê","ë","î","ï","ô","ö","û","ü","ñ","ç","ş","ǧ","ı","ĳ",
      "ª","α","©","‰","Ǧ","ě","ň","ő","π","€","£","$","←","↑","→","↓",
      "º","¹","²","³","±","İ","ń","ű","µ","¿","÷","°","¼","½","¾","§",
      "Á","À","É","È","Í","Ì","Ó","Ò","Ú","Ù","Ř","Č","Š","Ž","Ð","Ŀ",
      "Â","Ä","Ê","Ë","Î","Ï","Ô","Ö","Û","Ü","ř","č","š","ž","đ","ŀ",
      "Ã","Å","Æ","Œ","ŷ","Ý","Õ","Ø","Þ","Ŋ","Ŕ","Ć","Ś","Ź","Ŧ","ð",
      "ã","å","æ","œ","ŵ","ý","õ","ø","þ","ŋ","ŕ","ć","ś","ź","ŧ"," "});
  return char_map[code - 32];
}

std::string getPTYname(int pty) {
  assert (pty >= 0 && pty <= 32);
  const std::vector<std::string> pty_names ({
   "No PTY", "News", "Current Affairs", "Information",
   "Sport", "Education", "Drama", "Cultures",
   "Science", "Varied Speech","Pop Music", "Rock Music",
   "Easy Listening","Light Classics M","Serious Classics","Other Music",
   "Weather & Metr", "Finance", "Children's Progs", "Social Affairs",
   "Religion", "Phone In", "Travel & Touring", "Leisure & Hobby",
   "Jazz Music", "Country Music", "National Music", "Oldies Music",
   "Folk Music", "Documentary", "Alarm Test", "Alarm - Alarm !"
  });
  return pty_names[pty];

}

// extract len bits from bitstring, starting at starting_at from the right
uint16_t bits (uint16_t bitstring, int starting_at, int len) {
  return ((bitstring >> starting_at) & ((1<<len) - 1));
}

RDSString::RDSString(int len) : chars_(len), is_char_sequential_(len), prev_pos_(-1) {
  last_complete_string_ = getString();
}

void RDSString::setAt(int pos, int chr) {
  if (pos < 0 || pos >= (int)chars_.size())
    return;

  chars_[pos] = chr;

  if (pos != prev_pos_ + 1) {
    for (size_t i=0; i<is_char_sequential_.size(); i++)
      is_char_sequential_[i] = false;
  }

  is_char_sequential_[pos] = true;

  if (isComplete())
    last_complete_string_ = getString();

  prev_pos_ = pos;

}

size_t RDSString::lengthReceived() const {

  size_t result = 0;
  for (size_t i=0; i<is_char_sequential_.size(); i++) {
    if (!is_char_sequential_[i])
      break;
    result = i+1;
  }

  return result;
}

size_t RDSString::lengthExpected() const {

  size_t result = chars_.size();

  for (size_t i=0; i<chars_.size(); i++) {
    if (chars_[i] == 0x0D) {
      result = i;
      break;
    }
  }

  return result;
}

std::string RDSString::getString() const {
  std::string result;
  size_t len = lengthExpected();
  for (size_t i=0; i<len; i++) {
    result += (is_char_sequential_[i] ? getLCDchar(chars_[i]) : " ");
  }

  return result;

}


std::string RDSString::getLastCompleteString() const {
  return last_complete_string_;
}

bool RDSString::isComplete() const {
  return lengthReceived() >= lengthExpected();
}

void RDSString::clear() {
  for (size_t i=0; i<chars_.size(); i++) {
    is_char_sequential_[i] = false;
  }
  last_complete_string_ = getString();
}

Station::Station() : Station(0x0000) {

}

Station::Station(uint16_t _pi) : pi_(_pi), ps_(8), rt_(64) {

}

void Station::update(Group group) {

  is_tp_   = bits(group.block2, 10, 1);
  pty_     = bits(group.block2,  5, 5);

  printf(":%d%s\n",group.type, group.type_ab == TYPE_A ? "A" : "B");

  printf("tp: %s, ", is_tp_ ? "true" : "false");
  printf("prog_type: \"%s\", ", getPTYname(pty_).c_str());

  if      (group.type == 0)  { decodeType0(group); }
  else if (group.type == 1)  { decodeType1(group); }
  else if (group.type == 2)  { decodeType2(group); }
  else if (group.type == 4)  { decodeType4(group); }
  else if (group.type == 14) { decodeType14(group); }

  printf("}\n");
}

void Station::addAltFreq(uint8_t af_code) {
  if (af_code >= 1 && af_code <= 204) {
    alt_freqs_.insert(87.5 + af_code / 10.0);
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

}

void Station::updatePS(int pos, std::vector<int> chars) {

  for (int i=pos; i<pos+(int)chars.size(); i++)
    ps_.setAt(i, chars[i-pos]);

}

void Station::updateRadioText(int pos, std::vector<int> chars) {

  for (int i=pos; i<pos+(int)chars.size(); i++)
    rt_.setAt(i, chars[i-pos]);

}

void Station::decodeType0 (Group group) {

  // not implemented: Decoder Identification

  is_ta_    = bits(group.block2, 4, 1);
  is_music_ = bits(group.block2, 3, 1);

  if (group.num_blocks < 3)
    return;

  if (group.type_ab == TYPE_A) {
    for (int i=0; i<2; i++) {
      addAltFreq(bits(group.block3, 8-i*8, 8));
    }
  }

  if (group.num_blocks < 4)
    return;

  updatePS(bits(group.block2, 0, 2) * 2,
      { bits(group.block4, 8, 8), bits(group.block4, 0, 8) });

}

void Station::decodeType1 (Group group) {

  if (group.num_blocks < 4)
    return;

  pin_ = group.block4;

  if (group.type_ab == TYPE_A) {
    pager_tng_ = bits(group.block2, 2, 3);
    if (pager_tng_ != 0) {
      pager_interval_ = bits(group.block2, 0, 2);
    }
    linkage_la_ = bits(group.block3, 15, 1);

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

      has_country_ = true;

    } else if (slc_variant == 1) {
      tmc_id_ = bits(group.block3, 0, 12);

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

    } else if (slc_variant == 6) {
      // TODO:
      // broadcaster data

    } else if (slc_variant == 7) {
      ews_channel_ = bits(group.block3, 0, 12);
    }

  }

}

void Station::decodeType2 (Group group) {

  if (group.num_blocks < 3)
    return;

  int rt_position = bits(group.block2, 0, 4) * (group.type_ab == TYPE_A ? 4 : 2);
  int prev_textAB = rt_ab_;
  rt_ab_          = bits(group.block2, 4, 1);

  if (prev_textAB != rt_ab_)
    rt_.clear();

  std::string chars;

  if (group.type_ab == TYPE_A) {
    updateRadioText(rt_position, {bits(group.block3, 8, 8), bits(group.block3, 0, 8)});
  }

  if (group.num_blocks == 4) {
    updateRadioText(rt_position+2, {bits(group.block4, 8, 8), bits(group.block4, 0, 8)});
  }

}

void Station::decodeType4 (Group group) {

  if (group.num_blocks < 3 || group.type_ab == TYPE_B)
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

    char buff[100];
    snprintf(buff, sizeof(buff),
        "%04d-%02d-%02dT%02d:%02d:00%+03d:%02d\n",yr,mo,dy,hr,mn,int(lto),ltom);
    clock_time_ = buff;
    std::cout<<"clock_time:"<<clock_time_<<"\n";

  }
}

} // namespace redsea
