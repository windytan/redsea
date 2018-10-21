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
#include "src/tmc/tmc.h"

#include "config.h"
#ifdef ENABLE_TMC

#include <climits>
#include <deque>
#include <fstream>
#include <iomanip>
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
std::map<uint16_t, std::string> g_supplementary_data;
LocationDatabase g_location_database;

uint16_t PopBits(std::deque<int>* bit_deque, int len) {
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
std::vector<FreeformField> GetFreeformFields(
    const std::vector<MessagePart>& parts) {
  static const std::vector<int> field_size(
      {3, 3, 5, 5, 5, 8, 8, 8, 8, 11, 16, 16, 16, 16, 0, 0});

  uint16_t second_gsi = Bits(parts[1].data[0], 12, 2);

  // Concatenate freeform data from used message length (derived from
  // GSI of second group)
  std::deque<int> freeform_data_bits;
  for (size_t i = 1; i < parts.size(); i++) {
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
  std::vector<FreeformField> result;
  while (freeform_data_bits.size() > 4) {
    uint16_t label = PopBits(&freeform_data_bits, 4);
    if (static_cast<int>(freeform_data_bits.size()) < field_size.at(label))
      break;

    uint16_t field_data = PopBits(&freeform_data_bits, field_size.at(label));

    if (label == 0x00 && field_data == 0x00)
      break;

    if (label <= 14)
      result.push_back({static_cast<FieldLabel>(label), field_data});
  }

  return result;
}

std::string UrgencyString(EventUrgency u) {
  switch (u) {
    case EventUrgency::None : return "none"; break;
    case EventUrgency::U    : return "U";    break;
    case EventUrgency::X    : return "X";    break;
  }
}

std::string TimeString(uint16_t field_data) {
  std::stringstream ss;

  if (field_data <= 95) {
    ss << HoursMinutesString(field_data / 4, 15 * (field_data % 4));

  } else if (field_data <= 200) {
    int days = (field_data - 96) / 24;
    int hour = (field_data - 96) % 24;
    if (days == 0)
      ss << "at " << std::setfill('0') << std::setw(2) << hour << ":00";
    else if (days == 1)
      ss << "after 1 day at " << std::setfill('0') << std::setw(2) << hour <<
         ":00";
    else
      ss << "after " << days << " days at " << std::setfill('0') <<
         std::setw(2) << hour << ":00";

  } else if (field_data <= 231) {
    ss << "day " << (field_data - 200) << " of the month";

  } else {
    int mo = (field_data-232) / 2;
    bool end_mid = (field_data-232) % 2;
    std::vector<std::string> month_names({
        "January", "February", "March", "April", "May",
        "June", "July", "August", "September", "October", "November",
        "December"});
    if (mo < 12) {
      ss << (end_mid ? "end of " : "mid-") + month_names.at(mo);
    }
  }

  return ss.str();
}

std::vector<std::string> ScopeStrings(uint16_t mgs) {
  bool mgs_i = Bits(mgs, 3, 1);
  bool mgs_n = Bits(mgs, 2, 1);
  bool mgs_r = Bits(mgs, 1, 1);
  bool mgs_u = Bits(mgs, 0, 1);

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

uint16_t QuantifierSize(QuantifierType qtype) {
  switch (qtype) {
    case QuantifierType::SmallNumber:
    case QuantifierType::Number:
    case QuantifierType::LessThanMetres:
    case QuantifierType::Percent:
    case QuantifierType::UptoKmh:
    case QuantifierType::UptoTime:
      return 5;
      break;

    default:
      return 8;
      break;
  }
}

std::string DescriptionWithQuantifier(const Event& event, uint16_t q_value) {
  std::string text("(Q)");
  std::regex q_regex("\\(Q\\)");

  if (QuantifierSize(event.quantifier_type) == 5 && q_value == 0)
    q_value = 32;

  switch (event.quantifier_type) {
    case QuantifierType::SmallNumber: {
      int num = q_value;
      if (num > 28)
        num += (num - 28);
      text = std::to_string(num);
      break;
    }
    case QuantifierType::Number: {
      int num;
      if (q_value <= 4)
        num = q_value;
      else if (q_value <= 14)
        num = (q_value - 4) * 10;
      else
        num = (q_value - 12) * 50;
      text = std::to_string(num);
      break;
    }
    case QuantifierType::LessThanMetres: {
      text = "less than " + std::to_string(q_value * 10) + " metres";
      break;
    }
    case QuantifierType::Percent: {
      text = std::to_string(q_value == 32 ? 0 : q_value * 5) + " %";
      break;
    }
    case QuantifierType::UptoKmh: {
      text = "of up to " + std::to_string(q_value * 5) + " km/h";
      break;
    }
    case QuantifierType::UptoTime: {
      if (q_value <= 10)
        text = "of up to " + std::to_string(q_value * 5) + " minutes";
      else if (q_value <= 22)
        text = "of up to " + std::to_string(q_value - 10) + " hours";
      else
        text = "of up to " + std::to_string((q_value - 20) * 6) + " hours";
      break;
    }
    case QuantifierType::DegreesCelsius: {
      text = std::to_string(q_value - 51) + " degrees Celsius";
      break;
    }
    case QuantifierType::Time: {
      int minute = (q_value - 1) * 10;
      int hour = minute / 60;
      minute = minute % 60;

      text = HoursMinutesString(hour, minute);
      break;
    }
    case QuantifierType::Tonnes: {
      int decitonnes;
      if (q_value <= 100)
        decitonnes = q_value;
      else
        decitonnes = 100 + (q_value - 100) * 5;

      int whole_tonnes = decitonnes / 10;
      decitonnes = decitonnes % 10;

      text = std::to_string(whole_tonnes) + "." + std::to_string(decitonnes) +
        " tonnes";
      break;
    }
    case QuantifierType::Metres: {
      int decimetres;
      if (q_value <= 100)
        decimetres = q_value;
      else
        decimetres = 100 + (q_value - 100) * 5;

      int whole_metres = decimetres / 10;
      decimetres = decimetres % 10;

      text = std::to_string(whole_metres) + "." + std::to_string(decimetres) +
        " metres";
      break;
    }
    case QuantifierType::UptoMillimetres: {
      text = "of up to " + std::to_string(q_value) + " millimetres";
      break;
    }
    case QuantifierType::MHz: {
      CarrierFrequency freq(q_value);
      text = freq.str();
      break;
    }
    case QuantifierType::kHz: {
      CarrierFrequency freq(q_value, kFrequencyIsLFMF);
      text = freq.str();
      break;
    }
    default: {
      text = "TODO";
    }
  }

  std::string desc = std::regex_replace(event.description_with_quantifier,
      q_regex, text);
  return desc;
}

std::string ucfirst(std::string in) {
  if (in.size() > 0)
    in[0] = std::toupper(in[0]);
  return in;
}

void LoadEventData() {
  for (CSVRow row : ReadCSVWithTitles(tmc_data_events, ';')) {
    try {
      uint16_t code = std::stoi(row.at("Code"));
      Event event;
      event.description = row.at("Description");
      event.description_with_quantifier = row.at("Description with Q");

      if (row.at("N") == "F")
        event.nature = EventNature::Forecast;
      else if (row.at("N") == "S")
        event.nature = EventNature::Silent;

      if (!row.at("Q").empty()) {
        int qt = std::stoi(row.at("Q"));
        if (qt >= 0 && qt <= 12)
          event.quantifier_type = static_cast<QuantifierType>(qt);
      }
      event.allows_quantifier = !event.description_with_quantifier.empty();

      if (row.at("U") == "U")
        event.urgency = EventUrgency::U;
      else if (row.at("U") == "X")
        event.urgency = EventUrgency::X;

      if (std::regex_match(row.at("T"), std::regex(".?D.?")))
        event.duration_type = DurationType::Dynamic;
      else if (std::regex_match(row.at("T"), std::regex(".?L.?")))
        event.duration_type = DurationType::LongerLasting;

      if (std::regex_match(row.at("T"), std::regex("\\(")))
        event.show_duration = false;

      if (!row.at("D").empty() && std::stoi(row.at("D")) == 2)
        event.directionality = EventDirectionality::Both;

      event.update_class = std::stoi(row.at("C"));

      g_event_data[code] = event;
    } catch (std::exception& e) {
      continue;
    }
  }

  for (std::vector<std::string> fields : ReadCSV(tmc_data_suppl, ';')) {
    if (fields.size() < 2)
      continue;

    uint16_t code = std::stoi(fields[0]);
    std::string desc = fields[1];

    g_supplementary_data.insert({code, desc});
  }
}

std::map<uint16_t, ServiceKey> LoadServiceKeyTable() {
  std::map<uint16_t, ServiceKey> result;

  for (std::vector<std::string> fields :
       ReadCSV("service_key_table.csv", ',')) {
    if (fields.size() < 4)
      continue;

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

void DecodeLocation(const LocationDatabase& db, uint16_t ltn,
                    Json::Value* jsonroot) {
  if (db.ltn != ltn || db.ltn == 0 ||
      !(*jsonroot)["tmc"]["message"].isMember("location"))
    return;

  uint16_t lcd = (*jsonroot)["tmc"]["message"]["location"].asUInt();
  int extent = std::stoi((*jsonroot)["tmc"]["message"]["extent"].asString());
  bool is_pos = (extent >= 0);

  if (db.points.count(lcd) > 0) {
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
//        (*jsonroot)["tmc"]["message"]["locations"].append(pts[i].lcd);
      (*jsonroot)["tmc"]["message"]["coordinates"][i]["lat"] = pts[i].lat;
      (*jsonroot)["tmc"]["message"]["coordinates"][i]["lon"] = pts[i].lon;
    }

    if (pts.size() > 1 && pts.at(0).name1.length() > 0 &&
        pts.at(pts.size()-1).name1.length() > 0) {
      (*jsonroot)["tmc"]["message"]["span_from"] = pts.at(0).name1;
      (*jsonroot)["tmc"]["message"]["span_to"] = pts.at(pts.size()-1).name1;
    }
    uint16_t roa_lcd = db.points.at(lcd).roa_lcd;
//      uint16_t seg_lcd = db.points.at(lcd).seg_lcd;
//      (*jsonroot)["tmc"]["message"]["seg_lcd"] = seg_lcd;
//      (*jsonroot)["tmc"]["message"]["roa_lcd"] = roa_lcd;
    if (db.roads.count(roa_lcd) > 0) {
      Road road = db.roads.at(roa_lcd);
      if (!road.road_number.empty())
        (*jsonroot)["tmc"]["message"]["road_number"] = road.road_number;
      if (road.name.length() > 0)
        (*jsonroot)["tmc"]["message"]["road_name"] = road.name;
      else if (!db.points.at(lcd).road_name.empty())
        (*jsonroot)["tmc"]["message"]["road_name"] =
            db.points.at(lcd).road_name;
    }
  }
}

bool IsValidEventCode(uint16_t code) {
  return g_event_data.count(code) != 0;
}

bool IsValidSupplementaryCode(uint16_t code) {
  return g_supplementary_data.count(code) != 0;
}

}  // namespace

Event getEvent(uint16_t code) {
  if (g_event_data.find(code) != g_event_data.end())
    return g_event_data.find(code)->second;
  else
    return Event();
}

TMC::TMC(const Options& options) : is_initialized_(false), is_encrypted_(false),
    has_encid_(false), is_enhanced_mode_(false), ltn_(0), sid_(0), encid_(0),
    message_(is_encrypted_), service_key_table_(LoadServiceKeyTable()), ps_(8) {
  if (!options.loctable_dir.empty() && g_location_database.ltn == 0) {
    g_location_database = LoadLocationDatabase(options.loctable_dir);
    if (options.feed_thru)
      std::cerr << g_location_database;
    else
      std::cout << g_location_database;
  }
}

void TMC::SystemGroup(uint16_t message, Json::Value* jsonroot) {
  uint16_t variant = Bits(message, 14, 2);

  if (variant == 0) {
    if (g_event_data.empty())
      LoadEventData();

    is_initialized_ = true;
    uint16_t ltn = Bits(message, 6, 6);

    is_encrypted_ = (ltn == 0);
    (*jsonroot)["tmc"]["system_info"]["is_encrypted"] = is_encrypted_;

    if (!is_encrypted_) {
      ltn_ = ltn;
      (*jsonroot)["tmc"]["system_info"]["location_table"] = ltn_;
    }

    bool     afi = Bits(message, 5, 1);
    uint16_t mgs = Bits(message, 0, 4);
    is_enhanced_mode_ = Bits(message, 4, 1);

    (*jsonroot)["tmc"]["system_info"]["is_on_alt_freqs"] = afi;

    for (std::string s : ScopeStrings(mgs))
      (*jsonroot)["tmc"]["system_info"]["scope"].append(s);
  } else if (variant == 1) {
    sid_ = Bits(message, 6, 6);
    (*jsonroot)["tmc"]["system_info"]["service_id"] = sid_;

    uint16_t g = Bits(message, 12, 2);
    static const int gap_values[4] = {3, 5, 8, 11};
    (*jsonroot)["tmc"]["system_info"]["gap"] = gap_values[g];

    if (is_enhanced_mode_) {
      uint16_t t_d = Bits(message, 0, 2);
      uint16_t t_w = Bits(message, 2, 2);
      uint16_t t_a = Bits(message, 4, 2);

      (*jsonroot)["tmc"]["system_info"]["delay_time"] = t_d;
      (*jsonroot)["tmc"]["system_info"]["activity_time"] = 1 << t_a;
      (*jsonroot)["tmc"]["system_info"]["window_time"] = 1 << t_w;
    }
  }
}

void TMC::UserGroup(uint16_t x, uint16_t y, uint16_t z, Json::Value *jsonroot) {
  if (!is_initialized_)
    return;

  bool t = Bits(x, 4, 1);

  // Encryption administration group
  if (Bits(x, 0, 5) == 0x00) {
    sid_   = Bits(y, 5, 6);
    encid_ = Bits(y, 0, 5);
    ltn_   = Bits(z, 10, 6);
    has_encid_ = true;

    (*jsonroot)["tmc"]["system_info"]["service_id"] = sid_;
    (*jsonroot)["tmc"]["system_info"]["encryption_id"] = encid_;
    (*jsonroot)["tmc"]["system_info"]["location_table"] = ltn_;

  // Tuning information
  } else if (t) {
    uint16_t variant = Bits(x, 0, 4);

    switch (variant) {
      case 4:
      case 5: {
        int pos = 4 * (variant - 4);

        ps_.set(pos,   RDSChar(Bits(y, 8, 8)));
        ps_.set(pos+1, RDSChar(Bits(y, 0, 8)));
        ps_.set(pos+2, RDSChar(Bits(z, 8, 8)));
        ps_.set(pos+3, RDSChar(Bits(z, 0, 8)));

        if (ps_.complete())
          (*jsonroot)["tmc"]["service_provider"] = ps_.last_complete_string();
        break;
      }

      case 6: {
        uint16_t on_pi = z;
        if (other_network_freqs_.count(on_pi) == 0)
          other_network_freqs_.insert({on_pi, AltFreqList()});

        other_network_freqs_.at(on_pi).insert(Bits(y, 8, 8));
        other_network_freqs_.at(on_pi).insert(Bits(y, 0, 8));

        /* Here, the alternative frequencies are printed out right away -
           DKULTUR, for example, does not transmit information about the total
           length of the list */
        (*jsonroot)["tmc"]["other_network"]["pi"] = "0x" + HexString(on_pi, 4);
        for (CarrierFrequency f : other_network_freqs_.at(on_pi).get())
          (*jsonroot)["tmc"]["other_network"]["frequencies"].append(f.str());
        other_network_freqs_.clear();
        break;
      }

      case 9: {
        uint16_t on_pi = z;
        uint16_t on_sid = Bits(y, 0, 6);
        uint16_t on_mgs = Bits(y, 6, 4);
        uint16_t on_ltn = Bits(y, 10, 6);

        (*jsonroot)["tmc"]["other_network"]["pi"] = HexString(on_pi, 4);
        (*jsonroot)["tmc"]["other_network"]["service_id"] = on_sid;
        (*jsonroot)["tmc"]["other_network"]["location_table"] = on_ltn;

        for (std::string s : ScopeStrings(on_mgs))
          (*jsonroot)["tmc"]["other_network"]["scope"].append(s);
        break;
      }

      default: {
        (*jsonroot)["debug"].append("TODO: TMC tuning info variant " +
            std::to_string(variant));
        break;
      }
    }

  // User message
  } else {
    if (is_encrypted_ && !has_encid_)
      return;

    bool f = Bits(x, 3, 1);

    // Single-group message
    if (f) {
      Message single_message(is_encrypted_);
      single_message.PushSingle(x, y, z);

      if (is_encrypted_ && service_key_table_.count(encid_) > 0)
        single_message.Decrypt(service_key_table_[encid_]);

      if (!single_message.json().empty()) {
        (*jsonroot)["tmc"]["message"] = single_message.json();
        DecodeLocation(g_location_database, ltn_, jsonroot);
      }

    // Part of multi-group message
    } else {
      uint16_t continuity_index = Bits(x, 0, 3);

      if (continuity_index != message_.continuity_index())
       message_ = Message(is_encrypted_);

      message_.PushMulti(x, y, z);
      if (message_.complete()) {
        if (is_encrypted_ && service_key_table_.count(encid_) > 0)
          message_.Decrypt(service_key_table_[encid_]);

        if (!message_.json().empty()) {
          (*jsonroot)["tmc"]["message"] = message_.json();
          DecodeLocation(g_location_database, ltn_, jsonroot);
        }
        message_ = Message(is_encrypted_);
      }
    }
  }
}

Message::Message(bool is_loc_encrypted) : is_encrypted_(is_loc_encrypted),
    was_encrypted_(is_encrypted_),
    duration_(0), duration_type_(DurationType::Dynamic), divertadv_(false),
    direction_(Direction::Positive),
    extent_(0), events_(), supplementary_(), quantifiers_(), diversion_(),
    location_(0), encrypted_location_(0), is_complete_(false),
    has_length_affected_(false),
    length_affected_(0), has_time_until_(false), time_until_(0),
    has_time_starts_(false), time_starts_(0), has_speed_limit_(false),
    speed_limit_(0), directionality_(EventDirectionality::Single),
    urgency_(EventUrgency::None),
    continuity_index_(0), parts_(5) {
}

bool Message::complete() const {
  return is_complete_;
}

uint16_t Message::continuity_index() const {
  return continuity_index_;
}

void Message::PushSingle(uint16_t x, uint16_t y, uint16_t z) {
  duration_  = Bits(x, 0, 3);
  divertadv_ = Bits(y, 15, 1);
  direction_ = Bits(y, 14, 1) ? Direction::Negative : Direction::Positive;
  extent_    = Bits(y, 11, 3);
  events_.push_back(Bits(y, 0, 11));
  if (is_encrypted_)
    encrypted_location_ = z;
  else
    location_  = z;
  directionality_ = getEvent(events_[0]).directionality;
  urgency_   = getEvent(events_[0]).urgency;
  duration_type_ = getEvent(events_[0]).duration_type;

  is_complete_ = true;
}

void Message::PushMulti(uint16_t x, uint16_t y, uint16_t z) {
  uint16_t new_continuity_index = Bits(x, 0, 3);
  if (continuity_index_ != new_continuity_index && continuity_index_ != 0) {
    //*stream_ << jsonVal("debug", "ERR: wrong continuity index!");
  }
  continuity_index_ = new_continuity_index;
  bool is_first_group = Bits(y, 15, 1);
  int current_group;
  int group_sequence_indicator = -1;

  if (is_first_group) {
    current_group = 0;
  } else if (Bits(y, 14, 1)) {  // SG
    group_sequence_indicator = Bits(y, 12, 2);
    current_group = 1;
  } else {
    group_sequence_indicator = Bits(y, 12, 2);
    current_group = 4 - group_sequence_indicator;
  }

  bool is_last_group = (group_sequence_indicator == 0);

  parts_.at(current_group) = MessagePart(kMessagePartIsReceived, {y, z});

  if (is_last_group) {
    DecodeMulti();
    clear();
  }
}

void Message::DecodeMulti() {
  // Need at least the first group
  if (!parts_[0].is_received)
    return;

  is_complete_ = true;

  // First group
  direction_ = Bits(parts_[0].data[0], 14, 1) ? Direction::Negative :
                                                Direction::Positive;
  extent_    = Bits(parts_[0].data[0], 11, 3);
  events_.push_back(Bits(parts_[0].data[0], 0, 11));
  if (is_encrypted_)
    encrypted_location_ = parts_[0].data[1];
  else
    location_ = parts_[0].data[1];
  directionality_ = getEvent(events_[0]).directionality;
  urgency_ = getEvent(events_[0]).urgency;
  duration_type_ = getEvent(events_[0]).duration_type;

  // Subsequent parts
  if (parts_[1].is_received) {
    for (FreeformField field : GetFreeformFields(parts_)) {
      switch (field.label) {
        case FieldLabel::Duration :
          duration_ = field.data;
          break;

        case FieldLabel::ControlCode :
          if (field.data > 7)
            break;
          switch (static_cast<ControlCode>(field.data)) {
            case ControlCode::IncreaseUrgency :
              switch (urgency_) {
                case EventUrgency::None : urgency_ = EventUrgency::U;    break;
                case EventUrgency::U    : urgency_ = EventUrgency::X;    break;
                case EventUrgency::X    : urgency_ = EventUrgency::None; break;
              }
              break;

            case ControlCode::ReduceUrgency :
              switch (urgency_) {
                case EventUrgency::None : urgency_ = EventUrgency::X;    break;
                case EventUrgency::U    : urgency_ = EventUrgency::None; break;
                case EventUrgency::X    : urgency_ = EventUrgency::U;    break;
              }
              break;

            case ControlCode::ChangeDirectionality :
              directionality_ =
                (directionality_ == EventDirectionality::Single ?
                                    EventDirectionality::Both :
                                    EventDirectionality::Single);
              break;

            case ControlCode::ChangeDurationType :
              duration_type_ =
                (duration_type_ == DurationType::Dynamic ?
                                   DurationType::LongerLasting :
                                   DurationType::Dynamic);
              break;

            case ControlCode::SetDiversion :
              divertadv_ = true;
              break;

            case ControlCode::IncreaseExtentBy8 :
              extent_ += 8;
              break;

            case ControlCode::IncreaseExtentBy16 :
              extent_ += 16;
              break;

            // default :
            // *stream_ << jsonVal("debug", "TODO: TMC control code " +
            //    std::to_string(field_data));
          }
          break;

        case FieldLabel::AffectedLength :
          length_affected_ = field.data;
          has_length_affected_ = true;
          break;

        case FieldLabel::SpeedLimit :
          speed_limit_ = field.data * 5;
          has_speed_limit_ = true;
          break;

        case FieldLabel::Quantifier5bit :
          if (events_.size() > 0 && quantifiers_.count(events_.size()-1) == 0 &&
              getEvent(events_.back()).allows_quantifier &&
              QuantifierSize(getEvent(events_.back()).quantifier_type) == 5) {
            quantifiers_.insert({events_.size()-1, field.data});
          } else {
            // *stream_ << jsonVal("debug", "invalid quantifier");
          }
          break;

        case FieldLabel::Quantifier8bit :
          if (events_.size() > 0 && quantifiers_.count(events_.size()-1) == 0 &&
              getEvent(events_.back()).allows_quantifier &&
              QuantifierSize(getEvent(events_.back()).quantifier_type) == 8) {
            quantifiers_.insert({events_.size()-1, field.data});
          } else {
            // *stream_ << jsonVal("debug", "invalid quantifier");
          }
          break;

        case FieldLabel::Supplementary :
          supplementary_.push_back(field.data);
          break;

        case FieldLabel::StartTime :
          time_starts_ = field.data;
          has_time_starts_ = true;
          break;

        case FieldLabel::StopTime :
          time_until_ = field.data;
          has_time_until_ = true;
          break;

        case FieldLabel::AdditionalEvent :
          events_.push_back(field.data);
          break;

        case FieldLabel::DetailedDiversion :
          diversion_.push_back(field.data);
          break;

        case FieldLabel::Destination :
        case FieldLabel::CrossLinkage :
        case FieldLabel::Separator :
          break;
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
  Json::Value element;

  if (!is_complete_ || events_.empty())
    return element;

  for (uint16_t code : events_)
    element["event_codes"].append(code);

  if (supplementary_.size() > 0)
    for (uint16_t code : supplementary_)
      element["supplementary_codes"].append(code);

  std::vector<std::string> sentences;
  for (size_t i=0; i < events_.size(); i++) {
    std::string description;
    if (IsValidEventCode(events_[i])) {
      Event event = getEvent(events_[i]);
      if (quantifiers_.count(i) == 1) {
        description = DescriptionWithQuantifier(event, quantifiers_.at(i));
      } else {
        description = event.description;
      }
      sentences.push_back(ucfirst(description));
    }
  }

  if (IsValidEventCode(events_[0]))
    element["update_class"] = getEvent(events_[0]).update_class;

  for (uint16_t code : supplementary_) {
    if (IsValidSupplementaryCode(code))
      sentences.push_back(ucfirst(g_supplementary_data.find(code)->second));
  }

  if (!sentences.empty())
    element["description"] = Join(sentences, ". ") + ".";

  if (!diversion_.empty())
    for (uint16_t code : diversion_)
      element["diversion_route"].append(code);

  if (has_speed_limit_)
    element["speed_limit"] =
        std::to_string(speed_limit_) + " km/h";

  if (was_encrypted_)
    element["encrypted_location"] = encrypted_location_;

  if (!is_encrypted_)
    element["location"] = location_;

  element["direction"] =
      directionality_ == EventDirectionality::Single ? "single" : "both";

  element["extent"] = (direction_ == Direction::Negative ? "-" : "+") +
      std::to_string(extent_);

  if (has_time_starts_)
    element["starts"] = TimeString(time_starts_);
  if (has_time_until_)
    element["until"] = TimeString(time_until_);

  element["urgency"] = UrgencyString(urgency_);

  return element;
}

void Message::Decrypt(const ServiceKey& key) {
  if (!is_encrypted_)
    return;

  location_ = rotl16(encrypted_location_ ^ (key.xorval << key.xorstart),
                     key.nrot);
  is_encrypted_ = false;
}

}  // namespace tmc
}  // namespace redsea

#endif  // ENABLE_TMC
