#include "src/tmc/message.hh"

#include <array>
#include <cassert>
#include <cctype>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <deque>
#include <string>
#include <utility>
#include <vector>

#include "src/tmc/eventdb.hh"
#include "src/tree.hh"
#include "src/util.hh"

namespace redsea::tmc {

namespace {

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

std::string getTimeString(std::uint16_t field_data) {
  if (field_data <= 95) {
    return getHoursMinutesString(field_data / 4, 15 * (field_data % 4));

  } else if (field_data <= 200) {
    const int days = (field_data - 96) / 24;
    const int hour = (field_data - 96) % 24;
    if (days == 0)
      return "at " + getHoursMinutesString(hour, 0);
    else if (days == 1)
      return "after 1 day at " + getHoursMinutesString(hour, 0);
    else
      return "after " + std::to_string(days) + " days at " + getHoursMinutesString(hour, 0);

  } else if (field_data <= 231) {
    return "day " + std::to_string(field_data - 200) + " of the month";

  } else {
    const std::uint16_t month = (field_data - 232) / 2;
    const bool end_or_mid     = (field_data - 232) % 2;
    const std::array<std::string, 12> month_names{"January",   "February", "March",    "April",
                                                  "May",       "June",     "July",     "August",
                                                  "September", "October",  "November", "December"};
    if (month < 12)
      return (end_or_mid ? "end of " : "mid-") + month_names[month];
  }

  return "";
}

std::string replace(const std::string& str, const std::string& from, const std::string& to) {
  std::string result          = str;
  const std::size_t start_pos = result.find(from);
  if (start_pos != std::string::npos)
    result.replace(start_pos, from.length(), to);
  return result;
}

std::string getDescriptionWithQuantifier(const Event& event, std::uint16_t q_value) {
  std::string text("(Q)");

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

  return replace(event.description_with_quantifier, "(Q)", text);
}

std::string ucfirst(std::string in) {
  if (in.size() > 0)
    in[0] = static_cast<char>(std::toupper(in[0]));
  return in;
}

}  // namespace

MessagePart::MessagePart(bool _is_received, const std::array<std::uint16_t, 2>& data_in)
    : is_received(_is_received), data(data_in) {}

void Quantifiers::insert(std::size_t index, std::uint16_t value) {
  data.emplace_back(index, value);
}

bool Quantifiers::has(std::size_t index) const {
  for (const auto& pair : data) {
    if (pair.first == index)
      return true;
  }
  return false;
}

std::uint16_t Quantifiers::get(std::size_t index) const {
  for (const auto& pair : data) {
    if (pair.first == index)
      return pair.second;
  }
  return 0;
}

Message::Message(bool is_loc_encrypted)
    : is_encrypted_(is_loc_encrypted), was_encrypted_(is_loc_encrypted) {}

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

            case ControlCode::SetDiversion:       diversion_advised_ = true; break;

            case ControlCode::IncreaseExtentBy8:  extent_ += 8; break;

            case ControlCode::IncreaseExtentBy16: extent_ += 16; break;
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
          if (events_.size() > 0 && !quantifiers_.has(events_.size() - 1U) &&
              getEvent(events_.back()).allows_quantifier &&
              getQuantifierSize(getEvent(events_.back()).quantifier_type) == 5) {
            quantifiers_.insert(events_.size() - 1U, field.data);
          }
          break;

        case FieldLabel::Quantifier8bit:
          if (events_.size() > 0 && !quantifiers_.has(events_.size() - 1U) &&
              getEvent(events_.back()).allows_quantifier &&
              getQuantifierSize(getEvent(events_.back()).quantifier_type) == 8) {
            quantifiers_.insert(events_.size() - 1U, field.data);
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

// \return A hierarchical representation of the message, reflecting the JSON end result
ObjectTree Message::tree() const {
  ObjectTree element;

  if (!is_complete_ || events_.empty())
    return element;

  // False maybe-uninitialized warning with gcc 13 - compiler bug?
  // https://gcc.gnu.org/bugzilla/show_bug.cgi?id=114592
#if defined(__clang__)
#pragma clang diagnostic ignored "-Wunknown-warning-option"
#endif
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
  for (const auto code : events_) element["event_codes"].push_back(code);
  for (const auto code : supplementary_) element["supplementary_codes"].push_back(code);
#pragma GCC diagnostic pop

  std::vector<std::string> sentences;
  for (std::size_t i = 0; i < events_.size(); i++) {
    std::string description;
    if (isValidEventCode(events_[i])) {
      const auto event = getEvent(events_[i]);
      if (quantifiers_.has(i)) {
        description = getDescriptionWithQuantifier(event, quantifiers_.get(i));
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
      sentences.push_back(ucfirst(getSupplementaryDescription(code)));

  if (!sentences.empty())
    element["description"] = join(sentences, ". ") + ".";

  if (has_speed_limit_)
    element["speed_limit"] = std::to_string(speed_limit_) + " km/h";

  // False maybe-uninitialized warning with gcc 13 - compiler bug?
  // https://gcc.gnu.org/bugzilla/show_bug.cgi?id=114592
#if defined(__clang__)
#pragma clang diagnostic ignored "-Wunknown-warning-option"
#endif
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
  for (std::uint16_t code : diversion_) element["diversion_route"].push_back(code);
#pragma GCC diagnostic pop

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

bool Message::hasLocation() const {
  return location_ != 0;
}

std::uint16_t Message::getLocation() const {
  return location_;
}

int Message::getExtent() const {
  return (direction_ == Direction::Negative ? -1 : 1) * static_cast<int>(extent_);
}

}  // namespace redsea::tmc
