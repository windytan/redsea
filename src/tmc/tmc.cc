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
#include "src/tmc/tmc.hh"

#include <array>
#include <cassert>
#include <cctype>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <exception>
#include <iomanip>
#include <iostream>
#include <map>
#include <regex>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "src/options.hh"
#include "src/tables.hh"
#include "src/tmc/csv.hh"
#include "src/tmc/events.hh"
#include "src/tmc/locationdb.hh"
#include "src/util.hh"

namespace redsea {

namespace tmc {

namespace {

std::map<std::uint16_t, Event> g_event_data;
std::map<std::uint16_t, std::string> g_supplementary_data;
std::map<std::uint16_t, LocationDatabase> g_location_databases;

std::uint16_t popBits(std::deque<bool>& bit_deque, std::size_t len) {
  assert(len <= 16);
  std::uint16_t result = 0x00;
  if (bit_deque.size() >= len) {
    for (std::size_t i = 0; i < len; i++) {
      result = static_cast<std::uint16_t>(result << 1U) | bit_deque.at(0);
      bit_deque.pop_front();
    }
  }
  return result;
}

std::uint16_t rotl16(std::uint16_t value, std::uint32_t count) {
  const std::uint32_t mask = (CHAR_BIT * sizeof(value) - 1);
  count &= mask;
  return static_cast<std::uint16_t>(value << count) |
         static_cast<std::uint16_t>(value >> static_cast<std::uint16_t>(((-count) & mask)));
}

// label, field_data (ISO 14819-1: 5.5)
std::vector<FreeformField> getFreeformFields(const std::array<MessagePart, 5>& parts) {
  constexpr std::array<std::uint32_t, 16> field_size{3, 3,  5,  5,  5,  8,  8, 8,
                                                     8, 11, 16, 16, 16, 16, 0, 0};

  const auto second_gsi = getBits<2>(parts[1].data[0], 12);

  // Concatenate freeform data from used message length (derived from
  // GSI of second group)
  std::deque<bool> freeform_data_bits;
  for (std::size_t i = 1; i < parts.size(); i++) {
    if (!parts[i].is_received)
      break;

    if (i == 1 || i >= parts.size() - second_gsi) {
      for (int b = 0; b < 12; b++)
        freeform_data_bits.push_back(static_cast<std::uint16_t>(parts[i].data[0] >> (11U - b)) &
                                     1U);
      for (int b = 0; b < 16; b++)
        freeform_data_bits.push_back(static_cast<std::uint16_t>(parts[i].data[1] >> (15U - b)) &
                                     1U);
    }
  }

  // Separate freeform data into fields
  std::vector<FreeformField> result;
  while (freeform_data_bits.size() > 4) {
    const std::uint16_t label = popBits(freeform_data_bits, 4);
    if (freeform_data_bits.size() < field_size.at(label))
      break;

    const std::uint16_t field_data = popBits(freeform_data_bits, field_size.at(label));

    if (label == 0x00 && field_data == 0x00)
      break;

    if (label <= 14)
      result.push_back({static_cast<FieldLabel>(label), field_data});
  }

  return result;
}

std::string getUrgencyString(EventUrgency u) {
  switch (u) {
    case EventUrgency::None: return "none";
    case EventUrgency::U:    return "U";
    case EventUrgency::X:    return "X";
  }
  return "none";
}

std::string getTimeString(std::uint16_t field_data) {
  std::stringstream ss;

  if (field_data <= 95) {
    ss << getHoursMinutesString(field_data / 4, 15 * (field_data % 4));

  } else if (field_data <= 200) {
    const int days = (field_data - 96) / 24;
    const int hour = (field_data - 96) % 24;
    if (days == 0)
      ss << "at " << std::setfill('0') << std::setw(2) << hour << ":00";
    else if (days == 1)
      ss << "after 1 day at " << std::setfill('0') << std::setw(2) << hour << ":00";
    else
      ss << "after " << days << " days at " << std::setfill('0') << std::setw(2) << hour << ":00";

  } else if (field_data <= 231) {
    ss << "day " << (field_data - 200) << " of the month";

  } else {
    const std::uint16_t month = (field_data - 232) / 2;
    const bool end_or_mid     = (field_data - 232) % 2;
    const std::array<std::string, 12> month_names{"January",   "February", "March",    "April",
                                                  "May",       "June",     "July",     "August",
                                                  "September", "October",  "November", "December"};
    if (month < 12)
      ss << (end_or_mid ? "end of " : "mid-") + month_names[month];
  }

  return ss.str();
}

std::vector<std::string> getScopeStrings(std::uint16_t mgs) {
  const bool mgs_i{getBool(mgs, 3)};
  const bool mgs_n{getBool(mgs, 2)};
  const bool mgs_r{getBool(mgs, 1)};
  const bool mgs_u{getBool(mgs, 0)};

  std::vector<std::string> scope;
  if (mgs_i)
    scope.emplace_back("inter-road");
  if (mgs_n)
    scope.emplace_back("national");
  if (mgs_r)
    scope.emplace_back("regional");
  if (mgs_u)
    scope.emplace_back("urban");

  return scope;
}

std::uint16_t getQuantifierSize(QuantifierType qtype) {
  switch (qtype) {
    case QuantifierType::SmallNumber:
    case QuantifierType::Number:
    case QuantifierType::LessThanMetres:
    case QuantifierType::Percent:
    case QuantifierType::UptoKmh:
    case QuantifierType::UptoTime:        return 5;

    case QuantifierType::DegreesCelsius:
    case QuantifierType::Time:
    case QuantifierType::Tonnes:
    case QuantifierType::Metres:
    case QuantifierType::UptoMillimetres:
    case QuantifierType::MHz:
    case QuantifierType::kHz:             return 8;
  }
  return 8;
}

std::string getDescriptionWithQuantifier(const Event& event, std::uint16_t q_value) {
  std::string text("(Q)");
  const std::regex q_regex("\\(Q\\)");

  if (getQuantifierSize(event.quantifier_type) == 5 && q_value == 0)
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
      int num{};
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
      int minute     = (q_value - 1) * 10;
      const int hour = minute / 60;
      minute         = minute % 60;

      text = getHoursMinutesString(hour, minute);
      break;
    }
    case QuantifierType::Tonnes: {
      int decitonnes         = (q_value <= 100) ? q_value : 100 + (q_value - 100) * 5;
      const int whole_tonnes = decitonnes / 10;
      decitonnes             = decitonnes % 10;

      text = std::to_string(whole_tonnes) + "." + std::to_string(decitonnes) + " tonnes";
      break;
    }
    case QuantifierType::Metres: {
      int decimetres         = (q_value <= 100) ? q_value : 100 + (q_value - 100) * 5;
      const int whole_metres = decimetres / 10;
      decimetres             = decimetres % 10;

      text = std::to_string(whole_metres) + "." + std::to_string(decimetres) + " metres";
      break;
    }
    case QuantifierType::UptoMillimetres: {
      text = "of up to " + std::to_string(q_value) + " millimetres";
      break;
    }
    case QuantifierType::MHz: {
      const CarrierFrequency freq(q_value, CarrierFrequency::Band::FM);
      text = freq.str();
      break;
    }
    case QuantifierType::kHz: {
      const CarrierFrequency freq(q_value, CarrierFrequency::Band::LF_MF);
      text = freq.str();
      break;
    }
  }

  return std::regex_replace(event.description_with_quantifier, q_regex, text);
}

std::string ucfirst(std::string in) {
  if (in.size() > 0)
    in[0] = static_cast<char>(std::toupper(in[0]));
  return in;
}

void loadEventData() {
  const CSVTable table{readCSVContainerWithTitles(tmc_data_events, ';')};
  for (const CSVRow& row : table.rows) {
    try {
      const std::uint16_t code = get_uint16(table, row, "Code");
      Event event;
      event.description                 = get_string(table, row, "Description");
      event.description_with_quantifier = get_string(table, row, "Description with Q");

      if (get_string(table, row, "N") == "F")
        event.nature = EventNature::Forecast;
      else if (get_string(table, row, "N") == "S")
        event.nature = EventNature::Silent;

      if (row_contains(table, row, "Q")) {
        const int qt = get_int(table, row, "Q");
        if (qt >= 0 && qt <= 12)
          event.quantifier_type = static_cast<QuantifierType>(qt);
      }
      event.allows_quantifier = !event.description_with_quantifier.empty();

      if (get_string(table, row, "U") == "U")
        event.urgency = EventUrgency::U;
      else if (get_string(table, row, "U") == "X")
        event.urgency = EventUrgency::X;

      if (std::regex_match(get_string(table, row, "T"), std::regex(".?D.?")))
        event.duration_type = DurationType::Dynamic;
      else if (std::regex_match(get_string(table, row, "T"), std::regex(".?L.?")))
        event.duration_type = DurationType::LongerLasting;

      if (std::regex_match(get_string(table, row, "T"), std::regex("\\(")))
        event.show_duration = false;

      if (row_contains(table, row, "D") && get_int(table, row, "D") == 2)
        event.directionality = EventDirectionality::Both;

      event.update_class = get_uint16(table, row, "C");

      g_event_data[code] = std::move(event);
    } catch (const std::exception&) {
      continue;
    }
  }

  for (const CSVRow& row : readCSVContainer(tmc_data_suppl, ';')) {
    if (row.lengths.size() < 2)
      continue;

    const auto code         = static_cast<std::uint16_t>(std::stoi(row.at(0)));
    const std::string& desc = row.at(1);

    g_supplementary_data.insert({code, desc});
  }
}

std::map<std::uint16_t, ServiceKey> loadServiceKeyTable() {
  std::map<std::uint16_t, ServiceKey> result;

  for (const CSVRow& row : readCSV("service_key_table.csv", ',')) {
    if (row.lengths.size() < 4)
      continue;

    std::uint16_t encid{};
    ServiceKey key;

    try {
      encid        = static_cast<std::uint16_t>(std::stoi(row.at(0)));
      key.xorval   = static_cast<std::uint8_t>(std::stoi(row.at(1)));
      key.xorstart = static_cast<std::uint8_t>(std::stoi(row.at(2)));
      key.nrot     = static_cast<std::uint8_t>(std::stoi(row.at(3)));
    } catch (const std::exception&) {
      continue;
    }

    result.insert({encid, key});
  }

  return result;
}

void decodeLocation(const LocationDatabase& db, std::uint16_t ltn,
                    nlohmann::ordered_json* jsonroot) {
  if (db.ltn != ltn || db.ltn == 0 || !(*jsonroot)["tmc"]["message"].contains("location"))
    return;

  const auto lcd =
      static_cast<std::uint16_t>((*jsonroot)["tmc"]["message"]["location"].get<std::uint16_t>());
  const int extent       = std::stoi((*jsonroot)["tmc"]["message"]["extent"].get<std::string>());
  const bool is_positive = (extent >= 0);

  if (db.points.find(lcd) != db.points.end()) {
    std::vector<Point> points;
    int points_left        = std::abs(extent) + 1;
    std::uint16_t this_lcd = lcd;
    while (points_left > 0 && db.points.find(this_lcd) != db.points.end()) {
      points.push_back(db.points.at(this_lcd));
      this_lcd = (is_positive ? db.points.at(this_lcd).pos_off : db.points.at(this_lcd).neg_off);
      points_left--;
    }

    for (std::size_t i = 0; i < points.size(); i++) {
      //        (*jsonroot)["tmc"]["message"]["locations"].append(pts[i].lcd);
      (*jsonroot)["tmc"]["message"]["coordinates"][i]["lat"] = static_cast<double>(points[i].lat);
      (*jsonroot)["tmc"]["message"]["coordinates"][i]["lon"] = static_cast<double>(points[i].lon);
    }

    if (points.size() > 1 && points.at(0).name1.length() > 0 &&
        points.at(points.size() - 1).name1.length() > 0) {
      (*jsonroot)["tmc"]["message"]["span_from"] = points.at(0).name1;
      (*jsonroot)["tmc"]["message"]["span_to"]   = points.at(points.size() - 1).name1;
    }
    const std::uint16_t roa_lcd = db.points.at(lcd).roa_lcd;
    //      std::uint16_t seg_lcd = db.points.at(lcd).seg_lcd;
    //      (*jsonroot)["tmc"]["message"]["seg_lcd"] = seg_lcd;
    //      (*jsonroot)["tmc"]["message"]["roa_lcd"] = roa_lcd;
    if (db.roads.find(roa_lcd) != db.roads.end()) {
      const Road road = db.roads.at(roa_lcd);
      if (!road.road_number.empty())
        (*jsonroot)["tmc"]["message"]["road_number"] = road.road_number;
      if (road.name.length() > 0)
        (*jsonroot)["tmc"]["message"]["road_name"] = road.name;
      else if (!db.points.at(lcd).road_name.empty())
        (*jsonroot)["tmc"]["message"]["road_name"] = db.points.at(lcd).road_name;
    }
  }
}

// Does this event code match an actual TMC event?
bool isValidEventCode(std::uint16_t code) {
  return g_event_data.find(code) != g_event_data.end();
}

bool isValidSupplementaryCode(std::uint16_t code) {
  return g_supplementary_data.find(code) != g_supplementary_data.end();
}

}  // namespace

// Return a predefined TMC event by its code.
Event getEvent(std::uint16_t code) {
  if (g_event_data.find(code) != g_event_data.end())
    return g_event_data.find(code)->second;
  else
    return {};
}

TMCService::TMCService(const Options& options)
    : message_(is_encrypted_), service_key_table_(loadServiceKeyTable()), ps_(8) {
  if (!options.loctable_dirs.empty() && g_location_databases.empty()) {
    for (const std::string& loctable_dir : options.loctable_dirs) {
      const auto ltn            = readLTN(loctable_dir);
      g_location_databases[ltn] = loadLocationDatabase(loctable_dir);
      if (options.feed_thru)
        std::cerr << g_location_databases[ltn];
      else
        std::cout << g_location_databases[ltn];
    }
  }
}

void TMCService::receiveSystemGroup(std::uint16_t message, nlohmann::ordered_json* jsonroot) {
  const auto variant = getBits<2>(message, 14);

  if (variant == 0) {
    if (g_event_data.empty())
      loadEventData();

    is_initialized_ = true;
    const auto ltn  = getBits<6>(message, 6);

    is_encrypted_                                     = (ltn == 0);
    (*jsonroot)["tmc"]["system_info"]["is_encrypted"] = is_encrypted_;

    if (!is_encrypted_) {
      ltn_                                                = ltn;
      (*jsonroot)["tmc"]["system_info"]["location_table"] = ltn_;
    }

    const bool afi = getBool(message, 5);
    const auto mgs = getBits<4>(message, 0);

    (*jsonroot)["tmc"]["system_info"]["is_on_alt_freqs"] = afi;

    for (const std::string& s : getScopeStrings(mgs))
      (*jsonroot)["tmc"]["system_info"]["scope"].push_back(s);
  } else if (variant == 1) {
    sid_                                            = getBits<6>(message, 6);
    (*jsonroot)["tmc"]["system_info"]["service_id"] = sid_;

    const auto g                             = getBits<2>(message, 12);
    const std::array<int, 4> gap_values      = {3, 5, 8, 11};
    (*jsonroot)["tmc"]["system_info"]["gap"] = gap_values[g];

    ltcc_ = getBits<4>(message, 0);
    if (ltcc_ > 0)
      (*jsonroot)["tmc"]["system_info"]["ltcc"] = ltcc_;
  } else if (variant == 2) {
    const auto ltecc = getUint8(message, 0);
    if (ltecc > 0) {
      (*jsonroot)["tmc"]["system_info"]["ltecc"] = ltecc;
      if (ltcc_ > 0) {
        (*jsonroot)["tmc"]["system_info"]["country"] = getCountryString(ltcc_, ltecc);
      }
    }
  }
}

void TMCService::receiveUserGroup(std::uint16_t x, std::uint16_t y, std::uint16_t z,
                                  nlohmann::ordered_json* jsonroot) {
  if (!is_initialized_)
    return;

  const bool t = getBool(x, 4);

  // Encryption administration group
  if (getBits<5>(x, 0) == 0x00) {
    sid_       = getBits<6>(y, 5);
    encid_     = getBits<5>(y, 0);
    ltn_       = getBits<6>(z, 10);
    has_encid_ = true;

    (*jsonroot)["tmc"]["system_info"]["service_id"]     = sid_;
    (*jsonroot)["tmc"]["system_info"]["encryption_id"]  = encid_;
    (*jsonroot)["tmc"]["system_info"]["location_table"] = ltn_;

    // Tuning information
  } else if (t) {
    const auto variant = getBits<4>(x, 0);

    switch (variant) {
      case 4:
      case 5: {
        const std::size_t pos = 4U * (variant - 4U);

        ps_.set(pos + 0, getUint8(y, 8));
        ps_.set(pos + 1, getUint8(y, 0));
        ps_.set(pos + 2, getUint8(z, 8));
        ps_.set(pos + 3, getUint8(z, 0));

        if (ps_.isComplete())
          (*jsonroot)["tmc"]["service_provider"] = ps_.getLastCompleteString();
        break;
      }

      case 6: {
        const std::uint16_t on_pi = z;
        if (other_network_freqs_.find(on_pi) == other_network_freqs_.end())
          other_network_freqs_.insert({on_pi, AltFreqList()});

        other_network_freqs_.at(on_pi).insert(getUint8(y, 8));
        other_network_freqs_.at(on_pi).insert(getUint8(y, 0));

        /* Here, the alternative frequencies are printed out right away -
           DKULTUR, for example, does not transmit information about the total
           length of the list */
        (*jsonroot)["tmc"]["other_network"]["pi"] = getPrefixedHexString<4>(on_pi);
        for (const int frequency : other_network_freqs_.at(on_pi).getRawList())
          (*jsonroot)["tmc"]["other_network"]["frequencies_khz"].push_back(frequency);
        other_network_freqs_.clear();
        break;
      }

      case 8: {
        if (y == 0 || z == 0 || y == z) {
          (*jsonroot)["tmc"]["other_network"]["pi"] = getPrefixedHexString<4>(y);
        } else {
          (*jsonroot)["tmc"]["other_network"]["pi_codes"].push_back(getPrefixedHexString<4>(y));
          (*jsonroot)["tmc"]["other_network"]["pi_codes"].push_back(getPrefixedHexString<4>(z));
        }
        break;
      }

      case 9: {
        const auto on_pi  = z;
        const auto on_sid = getBits<6>(y, 0);
        const auto on_mgs = getBits<4>(y, 6);
        const auto on_ltn = getBits<6>(y, 10);

        (*jsonroot)["tmc"]["other_network"]["pi"]             = getPrefixedHexString<4>(on_pi);
        (*jsonroot)["tmc"]["other_network"]["service_id"]     = on_sid;
        (*jsonroot)["tmc"]["other_network"]["location_table"] = on_ltn;

        for (const std::string& s : getScopeStrings(on_mgs))
          (*jsonroot)["tmc"]["other_network"]["scope"].push_back(s);
        break;
      }

      default: {
        (*jsonroot)["debug"].push_back("TODO: TMC tuning info variant " + std::to_string(variant));
        break;
      }
    }

    // User message
  } else {
    if (is_encrypted_ && !has_encid_)
      return;

    const bool f = getBool(x, 3);

    // Single-group message
    if (f) {
      Message single_message(is_encrypted_);
      single_message.pushSingle(x, y, z);

      if (is_encrypted_ && service_key_table_.find(encid_) != service_key_table_.end())
        single_message.decrypt(service_key_table_[encid_]);

      if (!single_message.json().empty()) {
        (*jsonroot)["tmc"]["message"] = single_message.json();
        decodeLocation(g_location_databases[ltn_], ltn_, jsonroot);
      }

      // Part of multi-group message
    } else {
      const auto continuity_index = getBits<3>(x, 0);

      if (continuity_index != message_.getContinuityIndex())
        message_ = Message(is_encrypted_);

      message_.pushMulti(x, y, z);
      if (message_.isComplete()) {
        if (is_encrypted_ && service_key_table_.find(encid_) != service_key_table_.end())
          message_.decrypt(service_key_table_[encid_]);

        if (!message_.json().empty()) {
          (*jsonroot)["tmc"]["message"] = message_.json();
          decodeLocation(g_location_databases[ltn_], ltn_, jsonroot);
        }
        message_ = Message(is_encrypted_);
      }
    }
  }
}

bool Message::isComplete() const {
  return is_complete_;
}

std::uint16_t Message::getContinuityIndex() const {
  return continuity_index_;
}

void Message::pushSingle(std::uint16_t x, std::uint16_t y, std::uint16_t z) {
  duration_          = getBits<3>(x, 0);
  diversion_advised_ = getBool(y, 15);
  direction_         = getBool(y, 14) ? Direction::Negative : Direction::Positive;
  extent_            = getBits<3>(y, 11);
  events_.push_back(getBits<11>(y, 0));
  if (is_encrypted_)
    encrypted_location_ = z;
  else
    location_ = z;
  directionality_ = getEvent(events_[0]).directionality;
  urgency_        = getEvent(events_[0]).urgency;
  duration_type_  = getEvent(events_[0]).duration_type;

  is_complete_ = true;
}

void Message::pushMulti(std::uint16_t x, std::uint16_t y, std::uint16_t z) {
  const auto new_continuity_index = getBits<3>(x, 0);
  if (continuity_index_ != new_continuity_index && continuity_index_ != 0) {
    //*stream_ << jsonVal("debug", "ERR: wrong continuity index!");
  }
  continuity_index_         = new_continuity_index;
  const bool is_first_group = getBool(y, 15);
  std::uint32_t current_group{};
  bool is_last_group{};

  if (is_first_group) {
    current_group = 0;
  } else if (getBool(y, 14)) {  // SG
    const auto group_sequence_indicator = getBits<2>(y, 12);
    current_group                       = 1;
    is_last_group                       = (group_sequence_indicator == 0);
  } else {
    const auto group_sequence_indicator = getBits<2>(y, 12);
    current_group                       = 4U - group_sequence_indicator;
    is_last_group                       = (group_sequence_indicator == 0);
  }

  parts_.at(current_group) = MessagePart(kMessagePartIsReceived, {y, z});

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
  direction_ = getBool(parts_[0].data[0], 14) ? Direction::Negative : Direction::Positive;
  extent_    = getBits<3>(parts_[0].data[0], 11);
  events_.push_back(getBits<11>(parts_[0].data[0], 0));
  if (is_encrypted_)
    encrypted_location_ = parts_[0].data[1];
  else
    location_ = parts_[0].data[1];
  directionality_ = getEvent(events_[0]).directionality;
  urgency_        = getEvent(events_[0]).urgency;
  duration_type_  = getEvent(events_[0]).duration_type;

  // Subsequent parts
  if (parts_[1].is_received) {
    for (const auto field : getFreeformFields(parts_)) {
      switch (field.label) {
        case FieldLabel::Duration: duration_ = field.data; break;

        case FieldLabel::ControlCode:
          if (field.data > 7)
            break;
          switch (static_cast<ControlCode>(field.data)) {
            case ControlCode::IncreaseUrgency:
              switch (urgency_) {
                case EventUrgency::None: urgency_ = EventUrgency::U; break;
                case EventUrgency::U:    urgency_ = EventUrgency::X; break;
                case EventUrgency::X:    urgency_ = EventUrgency::None; break;
              }
              break;

            case ControlCode::ReduceUrgency:
              switch (urgency_) {
                case EventUrgency::None: urgency_ = EventUrgency::X; break;
                case EventUrgency::U:    urgency_ = EventUrgency::None; break;
                case EventUrgency::X:    urgency_ = EventUrgency::U; break;
              }
              break;

            case ControlCode::ChangeDirectionality:
              directionality_ =
                  (directionality_ == EventDirectionality::Single ? EventDirectionality::Both
                                                                  : EventDirectionality::Single);
              break;

            case ControlCode::ChangeDurationType:
              duration_type_ =
                  (duration_type_ == DurationType::Dynamic ? DurationType::LongerLasting
                                                           : DurationType::Dynamic);
              break;

            case ControlCode::SetDiversion:      diversion_advised_ = true; break;

            case ControlCode::IncreaseExtentBy8: extent_ += 8; break;

            case ControlCode::IncreaseExtentBy16:
              extent_ += 16;
              break;

              // default :
              // *stream_ << jsonVal("debug", "TODO: TMC control code " +
              //    std::to_string(field_data));
          }
          break;

        case FieldLabel::AffectedLength:
          length_affected_     = field.data;
          has_length_affected_ = true;
          break;

        case FieldLabel::SpeedLimit:
          speed_limit_     = field.data * 5;
          has_speed_limit_ = true;
          break;

        case FieldLabel::Quantifier5bit:
          if (events_.size() > 0 && quantifiers_.find(events_.size() - 1U) == quantifiers_.end() &&
              getEvent(events_.back()).allows_quantifier &&
              getQuantifierSize(getEvent(events_.back()).quantifier_type) == 5) {
            quantifiers_.insert({events_.size() - 1U, field.data});
          } else {
            // *stream_ << jsonVal("debug", "invalid quantifier");
          }
          break;

        case FieldLabel::Quantifier8bit:
          if (events_.size() > 0 && quantifiers_.find(events_.size() - 1U) == quantifiers_.end() &&
              getEvent(events_.back()).allows_quantifier &&
              getQuantifierSize(getEvent(events_.back()).quantifier_type) == 8) {
            quantifiers_.insert({events_.size() - 1U, field.data});
          } else {
            // *stream_ << jsonVal("debug", "invalid quantifier");
          }
          break;

        case FieldLabel::Supplementary: supplementary_.push_back(field.data); break;

        case FieldLabel::StartTime:
          time_starts_     = field.data;
          has_time_starts_ = true;
          break;

        case FieldLabel::StopTime:
          time_until_     = field.data;
          has_time_until_ = true;
          break;

        case FieldLabel::AdditionalEvent:   events_.push_back(field.data); break;

        case FieldLabel::DetailedDiversion: diversion_.push_back(field.data); break;

        case FieldLabel::Destination:
        case FieldLabel::CrossLinkage:
        case FieldLabel::Separator:         break;
      }
    }
  }
}

void Message::clear() {
  for (MessagePart& part : parts_) part.is_received = false;

  continuity_index_ = 0;
}

nlohmann::ordered_json Message::json() const {
  nlohmann::ordered_json element;

  if (!is_complete_ || events_.empty())
    return element;

  for (const auto code : events_) element["event_codes"].push_back(code);
  for (const auto code : supplementary_) element["supplementary_codes"].push_back(code);

  std::vector<std::string> sentences;
  for (std::size_t i = 0; i < events_.size(); i++) {
    std::string description;
    if (isValidEventCode(events_[i])) {
      const auto event = getEvent(events_[i]);
      if (quantifiers_.count(i) == 1) {
        description = getDescriptionWithQuantifier(event, quantifiers_.at(i));
      } else {
        description = event.description;
      }
      sentences.push_back(ucfirst(std::move(description)));
    }
  }

  if (isValidEventCode(events_[0]))
    element["update_class"] = getEvent(events_[0]).update_class;

  for (const auto code : supplementary_)
    if (isValidSupplementaryCode(code))
      sentences.push_back(ucfirst(g_supplementary_data.find(code)->second));

  if (!sentences.empty())
    element["description"] = join(sentences, ". ") + ".";

  for (const std::uint16_t code : diversion_) element["diversion_route"].push_back(code);

  if (has_speed_limit_)
    element["speed_limit"] = std::to_string(speed_limit_) + " km/h";

  if (was_encrypted_)
    element["encrypted_location"] = encrypted_location_;

  if (!is_encrypted_)
    element["location"] = location_;

  element["direction"] = directionality_ == EventDirectionality::Single ? "single" : "both";

  element["extent"] = (direction_ == Direction::Negative ? "-" : "+") + std::to_string(extent_);

  if (has_time_starts_)
    element["starts"] = getTimeString(time_starts_);
  if (has_time_until_)
    element["until"] = getTimeString(time_until_);

  element["urgency"] = getUrgencyString(urgency_);

  return element;
}

void Message::decrypt(const ServiceKey& key) {
  if (!is_encrypted_)
    return;

  location_ = rotl16(encrypted_location_ ^ static_cast<std::uint16_t>(key.xorval << key.xorstart),
                     key.nrot);
  is_encrypted_ = false;
}

}  // namespace tmc
}  // namespace redsea
