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

#include "config.h"
#ifdef ENABLE_TMC

#include <iostream>
#include <map>
#include <string>
#include <vector>

#include <json/json.h>

#include "src/common.h"
#include "src/options.h"
#include "src/rdsstring.h"
#include "src/tmc/locationdb.h"
#include "src/util.h"

namespace redsea {
namespace tmc {

enum class Direction {
  Positive = 0,
  Negative = 1
};

enum class EventNature {
  Event    = 0,
  Forecast = 1,
  Silent   = 2
};

enum class EventDirectionality {
  Single = 0,
  Both   = 1
};

enum class EventUrgency {
  None = 0,
  U    = 1,
  X    = 2
};

enum class DurationType {
  Dynamic       = 0,
  LongerLasting = 1
};

enum class QuantifierType {
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

enum class FieldLabel {
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

enum class ControlCode {
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
  uint16_t data;
};

struct ServiceKey {
  ServiceKey() : xorval(0), xorstart(0), nrot(0) {}
  ServiceKey(uint8_t _xorval, uint8_t _xorstart, uint8_t _nrot) :
    xorval(_xorval), xorstart(_xorstart), nrot(_nrot) {}
  uint8_t xorval;
  uint8_t xorstart;
  uint8_t nrot;
};

class Event {
 public:
  Event() : description(""), description_with_quantifier(""),
            nature(EventNature::Event),
            quantifier_type(QuantifierType::SmallNumber),
            duration_type(DurationType::Dynamic),
            directionality(EventDirectionality::Single),
            urgency(EventUrgency::None),
            update_class(0), allows_quantifier(false), show_duration(true) {}
  std::string description;
  std::string description_with_quantifier;
  EventNature nature;
  QuantifierType quantifier_type;
  DurationType duration_type;
  EventDirectionality directionality;
  EventUrgency urgency;
  uint16_t update_class;
  bool allows_quantifier;
  bool show_duration;
};

Event getEvent(uint16_t code);

const bool kMessagePartIsReceived = true;

struct MessagePart {
  MessagePart() : is_received(false), data() {}
  MessagePart(bool _is_received, const std::vector<uint16_t>& _data) :
    is_received(_is_received), data(_data) {}
  bool is_received;
  std::vector<uint16_t> data;
};

class Message {
 public:
  explicit Message(bool is_loc_encrypted);
  void PushMulti(uint16_t x, uint16_t y, uint16_t z);
  void PushSingle(uint16_t x, uint16_t y, uint16_t z);
  Json::Value json() const;
  void Decrypt(const ServiceKey& key);
  bool complete() const;
  void clear();
  uint16_t continuity_index() const;

 private:
  void DecodeMulti();
  bool is_encrypted_;
  bool was_encrypted_;
  uint16_t duration_;
  DurationType duration_type_;
  bool divertadv_;
  Direction direction_;
  uint16_t extent_;
  std::vector<uint16_t> events_;
  std::vector<uint16_t> supplementary_;
  std::map<uint16_t, uint16_t> quantifiers_;
  std::vector<uint16_t> diversion_;
  uint16_t location_;
  uint16_t encrypted_location_;
  bool is_complete_;
  bool has_length_affected_;
  uint16_t length_affected_;
  bool has_time_until_;
  uint16_t time_until_;
  bool has_time_starts_;
  uint16_t time_starts_;
  bool has_speed_limit_;
  uint16_t speed_limit_;
  EventDirectionality directionality_;
  EventUrgency urgency_;
  uint16_t continuity_index_;
  std::vector<MessagePart> parts_;
};

class TMC {
 public:
  explicit TMC(const Options& options);
  void SystemGroup(uint16_t message, Json::Value*);
  void UserGroup(uint16_t x, uint16_t y, uint16_t z, Json::Value*);

 private:
  bool is_initialized_;
  bool is_encrypted_;
  bool has_encid_;
  bool is_enhanced_mode_;
  uint16_t ltn_;
  uint16_t sid_;
  uint16_t encid_;
  Message message_;
  std::map<uint16_t, ServiceKey> service_key_table_;
  RDSString ps_;
  std::map<uint16_t, AltFreqList> other_network_freqs_;
};

}  // namespace tmc
}  // namespace redsea

#endif  // ENABLE_TMC
#endif  // TMC_H_
