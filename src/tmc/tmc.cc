#include "src/tmc/tmc.h"

#include "config.h"
#ifdef ENABLE_TMC

#include <climits>
#include <deque>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>
#include <utility>

#include <json/json.h>

#include "src/common.h"
#include "src/tmc/event_list.h"
#include "src/tmc/locationdb.h"
#include "src/util.h"

namespace redsea {

namespace tmc {

namespace {

std::map<uint16_t, Event> g_event_data;
std::map<uint16_t, std::string> g_suppl_data;

uint16_t popBits(std::deque<int>* bit_deque, int len) {
  uint16_t result = 0x00;
  if (static_cast<int>(bit_deque->size()) >= len) {
    for (int i=0; i < len; i++) {
      result = (result << 1) | bit_deque->at(0);
      bit_deque->pop_front();
    }
  }
  return result;
}

uint16_t rotl16(uint16_t value, unsigned int count) {
  const unsigned int mask = (CHAR_BIT*sizeof(value)-1);
  count &= mask;
  return (value << count) | (value >> ( (-count) & mask ));
}

// label, field_data (ISO 14819-1: 5.5)
std::vector<std::pair<uint16_t, uint16_t>>
    getFreeformFields(std::vector<MessagePart> parts) {
  static const std::vector<int> field_size(
      {3, 3, 5, 5, 5, 8, 8, 8, 8, 11, 16, 16, 16, 16, 0, 0});

  uint16_t second_gsi = bits(parts[1].data[0], 12, 2);

  // Concatenate freeform data from used message length (derived from
  // GSI of second group)
  std::deque<int> freeform_data_bits;
  for (size_t i=1; i < parts.size(); i++) {
    if (!parts[i].is_received)
      break;

    if (i == 1 || i >= parts.size() - second_gsi) {
      for (int b=0; b < 12; b++)
        freeform_data_bits.push_back((parts[i].data[0] >> (11-b)) & 0x1);
      for (int b=0; b < 16; b++)
        freeform_data_bits.push_back((parts[i].data[1] >> (15-b)) & 0x1);
    }
  }

  // Separate freeform data into fields
  std::vector<std::pair<uint16_t, uint16_t>> result;
  while (freeform_data_bits.size() > 4) {
    uint16_t label = popBits(&freeform_data_bits, 4);
    if (static_cast<int>(freeform_data_bits.size()) < field_size.at(label))
      break;

    uint16_t field_data = popBits(&freeform_data_bits, field_size.at(label));

    if (label == 0x00 && field_data == 0x00)
      break;

    result.push_back({label, field_data});
  }

  return result;
}

std::string timeString(uint16_t field_data) {
  std::string time_string("");

  if (field_data <= 95) {
    char t[6];
    std::snprintf(t, sizeof(t), "%02d:%02d", field_data/4, 15*(field_data % 4));
    time_string = t;

  } else if (field_data <= 200) {
    int days = (field_data - 96) / 24;
    int hour = (field_data - 96) % 24;
    char t[25];
    if (days == 0)
      std::snprintf(t, sizeof(t), "at %02d:00", hour);
    else if (days == 1)
      std::snprintf(t, sizeof(t), "after 1 day at %02d:00", hour);
    else
      std::snprintf(t, sizeof(t), "after %d days at %02d:00", days, hour);
    time_string = t;

  } else if (field_data <= 231) {
    char t[20];
    std::snprintf(t, sizeof(t), "day %d of the month", field_data-200);
    time_string = t;

  } else {
    int mo = (field_data-232) / 2;
    bool end_mid = (field_data-232) % 2;
    std::vector<std::string> month_names({
        "January", "February", "March", "April", "May",
        "June", "July", "August", "September", "October", "November",
        "December"});
    if (mo < 12) {
      time_string = (end_mid ? "end of " : "mid-") + month_names.at(mo);
    }
  }

  return time_string;
}

std::vector<std::string> getScopeStrings(uint16_t mgs) {
  bool mgs_i = bits(mgs, 3, 1);
  bool mgs_n = bits(mgs, 2, 1);
  bool mgs_r = bits(mgs, 1, 1);
  bool mgs_u = bits(mgs, 0, 1);

  std::vector<std::string> scope;
  if (mgs_i)
    scope.push_back("inter-road");
  if (mgs_n)
    scope.push_back("national");
  if (mgs_r)
    scope.push_back("regional");
  if (mgs_u)
    scope.push_back("urban");

  return scope;
}

uint16_t getQuantifierSize(uint16_t code) {
  if (code <= 5)
    return 5;
  else if (code <= 12)
    return 8;
  else
    return 0;
}

std::string getDescWithQuantifier(const Event& event, uint16_t q_value) {
  std::string q("_");
  std::regex q_re("_");

  if (getQuantifierSize(event.quantifier_type) == 5 && q_value == 0)
    q_value = 32;

  switch (event.quantifier_type) {
    case Q_SMALL_NUMBER: {
      int num = q_value;
      if (num > 28)
        num += (num - 28);
      q = std::to_string(num);
      break;
    }
    case Q_NUMBER: {
      int num;
      if (q_value <= 4)
        num = q_value;
      else if (q_value <= 14)
        num = (q_value - 4) * 10;
      else
        num = (q_value - 12) * 50;
      q = std::to_string(num);
      break;
    }
    case Q_LESS_THAN_METRES: {
      q = "less than " + std::to_string(q_value * 10) + " metres";
      break;
    }
    case Q_PERCENT: {
      q = std::to_string(q_value == 32 ? 0 : q_value * 5) + " %";
      break;
    }
    case Q_UPTO_KMH: {
      q = "of up to " + std::to_string(q_value * 5) + " km/h";
      break;
    }
    case Q_UPTO_TIME: {
      if (q_value <= 10)
        q = "of up to " + std::to_string(q_value * 5) + " minutes";
      else if (q_value <= 22)
        q = "of up to " + std::to_string(q_value - 10) + " hours";
      else
        q = "of up to " + std::to_string((q_value - 20) * 6) + " hours";
      break;
    }
    case Q_DEG_CELSIUS: {
      q = std::to_string(q_value - 51) + " degrees Celsius";
      break;
    }
    case Q_TIME: {
      int m = (q_value - 1) * 10;
      int h = m / 60;
      m = m % 60;

      char t[6];
      std::snprintf(t, sizeof(t), "%02d:%02d", m, h);
      q = t;
      break;
    }
    case Q_TONNES: {
      int decitonnes;
      if (q_value <= 100)
        decitonnes = q_value;
      else
        decitonnes = 100 + (q_value - 100) * 5;

      int whole_tonnes = decitonnes / 10;
      decitonnes = decitonnes % 10;

      q = std::to_string(whole_tonnes) + "." + std::to_string(decitonnes) +
        " tonnes";
      break;
    }
    case Q_METRES: {
      int decimetres;
      if (q_value <= 100)
        decimetres = q_value;
      else
        decimetres = 100 + (q_value - 100) * 5;

      int whole_metres = decimetres / 10;
      decimetres = decimetres % 10;

      q = std::to_string(whole_metres) + "." + std::to_string(decimetres) +
        " metres";
      break;
    }
    case Q_UPTO_MILLIMETRES: {
      q = "of up to " + std::to_string(q_value) + " millimetres";
      break;
    }
    case Q_MHZ: {
      CarrierFrequency freq(q_value);
      q = freq.getString();
      break;
    }
    case Q_KHZ: {
      CarrierFrequency freq(q_value, true);
      q = freq.getString();
      break;
    }
    default: {
      //*stream_ << jsonVal("debug", "q_value =" + std::to_string(q_value) +
      //    ", q_type=" + std::to_string(event.quantifier_type));
      q = "TODO";
    }
  }

  std::string desc = std::regex_replace(event.description_with_quantifier,
      q_re, q);
  return desc;
}

std::string ucfirst(std::string in) {
  if (in.size() > 0)
    in[0] = std::toupper(in[0]);
  return in;
}

void loadEventData() {
  for (std::vector<std::string> fields : readCSV(tmc_data_events, ';')) {
    if (fields.size() != 9)
      continue;

    uint16_t code = std::stoi(fields[0]);
    std::vector<std::string> strings(2);
    std::vector<uint16_t> nums(6);

    for (int col=1; col < 3; col++)
      strings[col-1] = fields[col];

    for (int col=3; col < 9; col++)
      nums[col-3] = std::stoi(fields[col]);

    bool allow_q = (strings[1].size() > 0);

    g_event_data.insert({code, {strings[0], strings[1], nums[0], nums[1],
        nums[2], nums[3], nums[4], nums[5], allow_q}});
  }

  for (std::vector<std::string> fields : readCSV(tmc_data_suppl, ';')) {
    uint16_t code = std::stoi(fields[0]);
    std::string desc = fields[1];

    g_suppl_data.insert({code, desc});
  }
}

std::map<uint16_t, ServiceKey> loadServiceKeyTable() {
  std::map<uint16_t, ServiceKey> result;

  for (std::vector<std::string> fields :
       readCSV("service_key_table.csv", ',')) {
    uint16_t encid;

    std::vector<uint8_t> nums(3);

    try {
      encid = std::stoi(fields.at(0));
      nums[0] = std::stoi(fields.at(1));
      nums[1] = std::stoi(fields.at(2));
      nums[2] = std::stoi(fields.at(3));
    } catch (const std::exception& e) {
      continue;
    }

    result.insert({encid, {nums[0], nums[1], nums[2]}});
  }

  return result;
}

void decodeLocation(const LocationDatabase& db, uint16_t ltn,
                    Json::Value* jsroot) {
  if ((*jsroot)["tmc"]["message"].isMember("location")) {
    uint16_t lcd = (*jsroot)["tmc"]["message"]["location"].asUInt();
    int extent = std::stoi((*jsroot)["tmc"]["message"]["extent"].asString());
    bool is_pos = (extent >= 0);

    if (db.points.count(lcd) > 0 && db.ltn == ltn) {

      (*jsroot)["tmc"]["message"].removeMember("location");
      (*jsroot)["tmc"]["message"].removeMember("extent");
      std::vector<Point> pts;
      int points_left = abs(extent) + 1;
      uint16_t this_lcd = lcd;
      while (points_left > 0 && db.points.count(this_lcd) > 0) {
        pts.push_back(db.points.at(this_lcd));
        this_lcd = (is_pos ? db.points.at(this_lcd).pos_off :
                             db.points.at(this_lcd).neg_off);
        points_left--;
      }

      for (int i=0; i < static_cast<int>(pts.size()); i++) {
//        (*jsroot)["tmc"]["message"]["locations"].append(pts[i].lcd);
        (*jsroot)["tmc"]["message"]["coordinates"][i].append(pts[i].lat);
        (*jsroot)["tmc"]["message"]["coordinates"][i].append(pts[i].lon);
      }

      if (pts.size() > 1 && pts.at(0).name1.length() > 0 &&
          pts.at(pts.size()-1).name1.length() > 0) {
        (*jsroot)["tmc"]["message"]["span_from"] = pts.at(0).name1;
        (*jsroot)["tmc"]["message"]["span_to"] = pts.at(pts.size()-1).name1;
      }
      uint16_t roa_lcd = db.points.at(lcd).roa_lcd;
//      uint16_t seg_lcd = db.points.at(lcd).seg_lcd;
//      (*jsroot)["tmc"]["message"]["seg_lcd"] = seg_lcd;
//      (*jsroot)["tmc"]["message"]["roa_lcd"] = roa_lcd;
      if (db.roads.count(roa_lcd) > 0) {
        (*jsroot)["tmc"]["message"]["road_number"] =
            db.roads.at(roa_lcd).road_number;
        if (db.roads.at(roa_lcd).name.length() > 0)
          (*jsroot)["tmc"]["message"]["road_name"] = db.roads.at(roa_lcd).name;
      }
    }
  }
}

bool isValidEventCode(uint16_t code) {
  return g_event_data.count(code) != 0;
}

bool isValidSupplementaryCode(uint16_t code) {
  return g_suppl_data.count(code) != 0;
}

} // namespace

Event::Event() : description(""), description_with_quantifier(""), nature(0),
  quantifier_type(0), duration_type(0), directionality(0), urgency(0),
  update_class(0), allows_quantifier(false) {
}

Event::Event(std::string _desc, std::string _desc_q, uint16_t _nature,
    uint16_t _qtype, uint16_t _dur, uint16_t _dir, uint16_t _urg,
    uint16_t _class, bool _allow_q) : description(_desc),
    description_with_quantifier(_desc_q), nature(_nature),
    quantifier_type(_qtype), duration_type(_dur), directionality(_dir),
    urgency(_urg), update_class(_class), allows_quantifier(_allow_q) {
}

Event getEvent(uint16_t code) {
  if (g_event_data.find(code) != g_event_data.end())
    return g_event_data.find(code)->second;
  else
    return Event();
}

TMC::TMC(Options options) : is_initialized_(false), is_encrypted_(false),
  has_encid_(false), ltn_(0), sid_(0), encid_(0), message_(is_encrypted_),
  service_key_table_(loadServiceKeyTable()), ps_(8), locdb_(0) {
  locdb_ = loadLocationDatabase(options.loctable_dir);
}

void TMC::systemGroup(uint16_t message, Json::Value* jsroot) {
  if (bits(message, 14, 1) == 0) {
    if (g_event_data.empty())
      loadEventData();

    is_initialized_ = true;
    uint16_t ltn = bits(message, 6, 6);

    is_encrypted_ = (ltn == 0);
    (*jsroot)["tmc"]["system_info"]["is_encrypted"] = is_encrypted_;

    if (!is_encrypted_) {
      ltn_ = ltn;
      (*jsroot)["tmc"]["system_info"]["location_table"] = ltn_;
    }

    bool afi   = bits(message, 5, 1);
    //bool m     = bits(message, 4, 1);
    bool mgs   = bits(message, 0, 4);

    (*jsroot)["tmc"]["system_info"]["is_on_alt_freqs"] = afi;

    for (std::string s : getScopeStrings(mgs))
      (*jsroot)["tmc"]["system_info"]["scope"].append(s);
  }
}

void TMC::userGroup(uint16_t x, uint16_t y, uint16_t z, Json::Value *jsroot) {

  if (!is_initialized_)
    return;

  bool t = bits(x, 4, 1);

  // Encryption administration group
  if (bits(x, 0, 5) == 0x00) {
    sid_   = bits(y, 5, 6);
    encid_ = bits(y, 0, 5);
    ltn_   = bits(z, 10, 6);
    has_encid_ = true;

    (*jsroot)["tmc"]["encryption_info"]["service_id"] = sid_;
    (*jsroot)["tmc"]["encryption_info"]["encryption_id"] = encid_;
    (*jsroot)["tmc"]["system_info"]["location_table"] = ltn_;

  // Tuning information
  } else if (t) {
    uint16_t variant = bits(x, 0, 4);

    if (variant == 4 || variant == 5) {

      int pos = 4 * (variant - 4);

      ps_.setAt(pos,   bits(y, 8, 8));
      ps_.setAt(pos+1, bits(y, 0, 8));
      ps_.setAt(pos+2, bits(z, 8, 8));
      ps_.setAt(pos+3, bits(z, 0, 8));

      if (ps_.isComplete())
        (*jsroot)["tmc"]["service_provider"] = ps_.getLastCompleteString();

    } else if (variant == 9) {

      uint16_t on_pi = z;
      uint16_t on_sid = bits(y, 0, 6);
      uint16_t on_mgs = bits(y, 6, 4);
      uint16_t on_ltn = bits(y, 10, 6);

      (*jsroot)["tmc"]["other_network"]["pi"] = hexString(on_pi, 4);
      (*jsroot)["tmc"]["other_network"]["service_id"] = on_sid;
      (*jsroot)["tmc"]["other_network"]["location_table"] = on_ltn;

      for (std::string s : getScopeStrings(on_mgs))
        (*jsroot)["tmc"]["other_network"]["scope"].append(s);

    } else {
      (*jsroot)["debug"].append("TODO: TMC tuning info variant " +
          std::to_string(variant));
    }

  // User message
  } else {

    if (is_encrypted_ && !has_encid_)
      return;

    bool f = bits(x, 3, 1);

    // Single-group message
    if (f) {
      Message message(is_encrypted_);
      message.pushSingle(x, y, z);

      if (is_encrypted_ && service_key_table_.count(encid_) > 0)
        message.decrypt(service_key_table_[encid_]);

      if (!message.json().empty()) {
        (*jsroot)["tmc"]["message"] = message.json();
        decodeLocation(locdb_, ltn_, jsroot);
      }

    // Part of multi-group message
    } else {

      uint16_t continuity_index = bits(x, 0, 3);

      if (continuity_index != message_.getContinuityIndex()) {
        /* Message changed; print previous unfinished message
         * TODO 15-second limit */
        if (!message_.json().empty()) {
          (*jsroot)["tmc"]["message"] = message_.json();
          decodeLocation(locdb_, ltn_, jsroot);
        }
        message_ = Message(is_encrypted_);
      }

      message_.pushMulti(x, y, z);
      if (message_.isComplete()) {

        if (is_encrypted_ && service_key_table_.count(encid_) > 0)
          message_.decrypt(service_key_table_[encid_]);

        if (!message_.json().empty()) {
          (*jsroot)["tmc"]["message"] = message_.json();
          decodeLocation(locdb_, ltn_, jsroot);
        }
        message_ = Message(is_encrypted_);
      }
    }
  }
}

Message::Message(bool is_loc_encrypted) : is_encrypted_(is_loc_encrypted),
    duration_(0), duration_type_(0), divertadv_(false), direction_(0),
    extent_(0), events_(), supplementary_(), quantifiers_(), diversion_(),
    location_(0), is_complete_(false), has_length_affected_(false),
    length_affected_(0), has_time_until_(false), time_until_(0),
    has_time_starts_(false), time_starts_(0), has_speed_limit_(false),
    speed_limit_(0), directionality_(DIR_SINGLE), urgency_(URGENCY_NONE),
    continuity_index_(0), parts_(5) {
}

bool Message::isComplete() const {
  return is_complete_;
}

uint16_t Message::getContinuityIndex() const {
  return continuity_index_;
}

void Message::pushSingle(uint16_t x, uint16_t y, uint16_t z) {
  duration_  = bits(x, 0, 3);
  divertadv_ = bits(y, 15, 1);
  direction_ = bits(y, 14, 1);
  extent_    = bits(y, 11, 3);
  events_.push_back(bits(y, 0, 11));
  location_  = z;
  directionality_ = getEvent(events_[0]).directionality;
  urgency_   = getEvent(events_[0]).urgency;
  duration_type_ = getEvent(events_[0]).duration_type;

  is_complete_ = true;
}

void Message::pushMulti(uint16_t x, uint16_t y, uint16_t z) {

  uint16_t new_ci = bits(x, 0, 3);
  if (continuity_index_ != new_ci && continuity_index_ != 0) {
    //*stream_ << jsonVal("debug", "ERR: wrong continuity index!");
  }
  continuity_index_ = new_ci;
  bool is_first_group = bits(y, 15, 1);
  int cur_grp;
  int gsi = -1;

  if (is_first_group) {
    cur_grp = 0;
  } else if (bits(y, 14, 1)) {  // SG
    gsi = bits(y, 12, 2);
    cur_grp = 1;
  } else {
    gsi = bits(y, 12, 2);
    cur_grp = 4 - gsi;
  }

  bool is_last_group = (gsi == 0);

  parts_.at(cur_grp) = {true, {y, z}};

  if (is_last_group) {
    decodeMulti();
    clear();
  }
}

void Message::decodeMulti() {
  // Need at least the first group
  if (!parts_[0].is_received)
    return;

  is_complete_ = true;

  // First group
  direction_ = bits(parts_[0].data[0], 14, 1);
  extent_    = bits(parts_[0].data[0], 11, 3);
  events_.push_back(bits(parts_[0].data[0], 0, 11));
  location_  = parts_[0].data[1];
  directionality_ = getEvent(events_[0]).directionality;
  urgency_   = getEvent(events_[0]).urgency;
  duration_type_ = getEvent(events_[0]).duration_type;

  // Subsequent parts
  if (parts_[1].is_received) {
    auto freeform = getFreeformFields(parts_);

    for (std::pair<uint16_t, uint16_t> p : freeform) {
      uint16_t label = p.first;
      uint16_t field_data = p.second;

      // Duration
      if (label == 0) {
        duration_ = field_data;

        // Control code
      } else if (label == 1) {
        if (field_data == 0) {
          urgency_ = (urgency_ + 1) % 3;
        } else if (field_data == 1) {
          if (urgency_ == URGENCY_NONE)
            urgency_ = URGENCY_X;
          else
            urgency_--;
        } else if (field_data == 2) {
          directionality_ ^= 1;
        } else if (field_data == 3) {
          duration_type_ ^= 1;
        } else if (field_data == 5) {
          divertadv_ = true;
        } else if (field_data == 6) {
          extent_ += 8;
        } else if (field_data == 7) {
          extent_ += 16;
        } else {
          // *stream_ << jsonVal("debug", "TODO: TMC control code " +
          //    std::to_string(field_data));
        }

        // Length of route affected
      } else if (label == 2) {
        length_affected_ = field_data;
        has_length_affected_ = true;

        // speed limit advice
      } else if (label == 3) {
        speed_limit_ = field_data * 5;
        has_speed_limit_ = true;

        // 5-bit quantifier
      } else if (label == 4) {
        if (events_.size() > 0 && quantifiers_.count(events_.size()-1) == 0 &&
            getEvent(events_.back()).allows_quantifier &&
            getQuantifierSize(getEvent(events_.back()).quantifier_type) == 5) {
          quantifiers_.insert({events_.size()-1, field_data});
        } else {
          // *stream_ << jsonVal("debug", "invalid quantifier");
        }

        // 8-bit quantifier
      } else if (label == 5) {
        if (events_.size() > 0 && quantifiers_.count(events_.size()-1) == 0 &&
            getEvent(events_.back()).allows_quantifier &&
            getQuantifierSize(getEvent(events_.back()).quantifier_type) == 8) {
          quantifiers_.insert({events_.size()-1, field_data});
        } else {
          // *stream_ << jsonVal("debug", "invalid quantifier");
        }

        // Supplementary info
      } else if (label == 6) {
        supplementary_.push_back(field_data);

        // Start / stop time
      } else if (label == 7) {
        time_starts_ = field_data;
        has_time_starts_ = true;

      } else if (label == 8) {
        time_until_ = field_data;
        has_time_until_ = true;

        // Multi-event message
      } else if (label == 9) {
        events_.push_back(field_data);

        // Detailed diversion
      } else if (label == 10) {
        diversion_.push_back(field_data);

        // Separator
      } else if (label == 14) {

      } else {
        //printf(",\"debug\":\"TODO label=%d data=0x%04x\"", label, field_data);
      }
    }
  }
}

void Message::clear() {
  for (MessagePart& part : parts_)
    part.is_received = false;

  continuity_index_ = 0;
}

Json::Value Message::json() const {
  Json::Value json;

  if (!is_complete_ || events_.empty())
    return json;

  for (auto code : events_)
    json["event_codes"].append(code);

  if (supplementary_.size() > 0)
    for (auto code : supplementary_)
      json["supplementary_codes"].append(code);

  std::vector<std::string> sentences;
  for (size_t i=0; i < events_.size(); i++) {
    std::string desc;
    if (isValidEventCode(events_[i])) {
      Event event = getEvent(events_[i]);
      if (quantifiers_.count(i) == 1) {
        desc = getDescWithQuantifier(event, quantifiers_.at(i));
      } else {
        desc = event.description;
      }
      sentences.push_back(ucfirst(desc));
    }
  }

  for (uint16_t code : supplementary_) {
    if (isValidSupplementaryCode(code))
      sentences.push_back(ucfirst(g_suppl_data.find(code)->second));
  }

  if (!sentences.empty())
    json["description"] = join(sentences, ". ") + ".";

  if (!diversion_.empty())
    for (auto code : diversion_)
      json["diversion_route"].append(code);

  if (has_speed_limit_)
    json["speed_limit"] =
        std::to_string(speed_limit_) + " km/h";

  if (is_encrypted_)
    json["encrypted_location"] = location_;
  else
    json["location"] = location_;

  json["direction"] =
      directionality_ == DIR_SINGLE ? "single" : "both";

  json["extent"] = (direction_ ? "-" : "+") +
      std::to_string(extent_);

  if (has_time_starts_)
    json["starts"] = timeString(time_starts_);
  if (has_time_until_)
    json["until"] = timeString(time_until_);

  return json;
}

void Message::decrypt(ServiceKey key) {
  if (!is_encrypted_)
    return;

  location_ = rotl16(location_ ^ (key.xorval << key.xorstart), key.nrot);
  is_encrypted_ = false;
}

}  // namespace tmc
}  // namespace redsea

#endif  // ENABLE_TMC
