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
#ifndef TMC_TMC_H_
#define TMC_TMC_H_

#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include <nlohmann/json_fwd.hpp>

#include "src/text/rdsstring.hh"
#include "src/util.hh"

namespace redsea {

struct Options;

namespace tmc {

enum class Direction : std::uint8_t { Positive = 0, Negative = 1 };

enum class EventNature : std::uint8_t { Event = 0, Forecast = 1, Silent = 2 };

enum class EventDirectionality : std::uint8_t { Single = 0, Both = 1 };

enum class EventUrgency : std::uint8_t { None = 0, U = 1, X = 2 };

enum class DurationType : std::uint8_t { Dynamic = 0, LongerLasting = 1 };

enum class QuantifierType : std::uint8_t {
  SmallNumber     = 0,
  Number          = 1,
  LessThanMetres  = 2,
  Percent         = 3,
  UptoKmh         = 4,
  UptoTime        = 5,
  DegreesCelsius  = 6,
  Time            = 7,
  Tonnes          = 8,
  Metres          = 9,
  UptoMillimetres = 10,
  MHz             = 11,
  kHz             = 12
};

enum class FieldLabel : std::uint8_t {
  Duration          = 0,
  ControlCode       = 1,
  AffectedLength    = 2,
  SpeedLimit        = 3,
  Quantifier5bit    = 4,
  Quantifier8bit    = 5,
  Supplementary     = 6,
  StartTime         = 7,
  StopTime          = 8,
  AdditionalEvent   = 9,
  DetailedDiversion = 10,
  Destination       = 11,
  // RFU              12
  CrossLinkage      = 13,
  Separator         = 14
};

enum class ControlCode : std::uint8_t {
  IncreaseUrgency      = 0,
  ReduceUrgency        = 1,
  ChangeDirectionality = 2,
  ChangeDurationType   = 3,
  // Spoken/Unspoken     4
  SetDiversion         = 5,
  IncreaseExtentBy8    = 6,
  IncreaseExtentBy16   = 7
};

struct FreeformField {
  FieldLabel label;
  std::uint16_t data;
};

struct ServiceKey {
  std::uint8_t xorval{0};
  std::uint8_t xorstart{0};
  std::uint8_t nrot{0};
};

struct Event {
  std::string description;
  std::string description_with_quantifier;
  EventNature nature{EventNature::Event};
  QuantifierType quantifier_type{QuantifierType::SmallNumber};
  DurationType duration_type{DurationType::Dynamic};
  EventDirectionality directionality{EventDirectionality::Single};
  EventUrgency urgency{EventUrgency::None};
  std::uint16_t update_class{0};
  bool allows_quantifier{false};
  bool show_duration{true};
};

Event getEvent(std::uint16_t code);

const bool kMessagePartIsReceived = true;

struct MessagePart {
  MessagePart() = default;
  MessagePart(bool _is_received, const std::array<std::uint16_t, 2>& data_in)
      : is_received(_is_received), data(data_in) {}

  bool is_received{false};
  std::array<std::uint16_t, 2> data{};
};

class Message {
 public:
  explicit Message(bool is_loc_encrypted)
      : is_encrypted_(is_loc_encrypted), was_encrypted_(is_loc_encrypted) {}
  void pushMulti(std::uint16_t x, std::uint16_t y, std::uint16_t z);
  void pushSingle(std::uint16_t x, std::uint16_t y, std::uint16_t z);
  nlohmann::ordered_json json() const;
  void decrypt(const ServiceKey& key);
  bool isComplete() const;
  void clear();
  std::uint16_t getContinuityIndex() const;

 private:
  void decodeMulti();

  bool is_encrypted_;
  bool was_encrypted_;
  std::uint16_t duration_{0};
  DurationType duration_type_{DurationType::Dynamic};
  bool diversion_advised_{false};
  Direction direction_{Direction::Positive};
  std::uint16_t extent_{0};
  std::vector<std::uint16_t> events_;
  std::vector<std::uint16_t> supplementary_;
  std::map<size_t, std::uint16_t> quantifiers_;
  std::vector<std::uint16_t> diversion_;
  std::uint16_t location_{0};
  std::uint16_t encrypted_location_{0};
  bool is_complete_{false};
  bool has_length_affected_{false};
  std::uint16_t length_affected_{0};
  bool has_time_until_{false};
  std::uint16_t time_until_{0};
  bool has_time_starts_{false};
  std::uint16_t time_starts_{0};
  bool has_speed_limit_{false};
  std::uint16_t speed_limit_{0};
  EventDirectionality directionality_{EventDirectionality::Single};
  EventUrgency urgency_{EventUrgency::None};
  std::uint16_t continuity_index_{0};
  std::array<MessagePart, 5> parts_{};
};

class TMCService {
 public:
  explicit TMCService(const Options& options);
  void receiveSystemGroup(std::uint16_t message, nlohmann::ordered_json*);
  void receiveUserGroup(std::uint16_t x, std::uint16_t y, std::uint16_t z, nlohmann::ordered_json*);

 private:
  bool is_initialized_{false};
  bool is_encrypted_{false};
  bool has_encid_{false};
  std::uint16_t ltn_{0};
  std::uint16_t sid_{0};
  std::uint16_t encid_{0};
  std::uint16_t ltcc_{0};
  Message message_;
  std::map<std::uint16_t, ServiceKey> service_key_table_;
  RDSString ps_;
  std::map<std::uint16_t, AltFreqList> other_network_freqs_;
};

}  // namespace tmc
}  // namespace redsea

#endif  // TMC_H_
