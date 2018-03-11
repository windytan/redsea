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

enum eDirection {
  kPositiveDirection = 0, kNegativeDirection = 1
};
enum eEventNature {
  kInfoEvent, kForecastEvent, kSilentEvent,
};
enum eEventDirectionality {
  kSingleDirection, kBothDirections
};
enum eEventUrgency {
  kUrgencyNone, kUrgencyU, kUrgencyX
};
enum eDurationType {
  kDurationDynamic, kDurationLongerLasting
};
enum eQuantifierType {
  kQuantifierSmallNumber, kQuantifierNumber, kQuantifierLessThanMetres,
  kQuantifierPercent, kQuantifierUptoKmh, kQuantifierUptoTime,
  kQuantifierDegreesCelsius, kQuantifierTime, kQuantifierTonnes,
  kQuantifierMetres, kQuantifierUptoMillimetres, kQuantifierMHz, kQuantifierkHz
};

enum eFieldLabel {
  kLabelDuration = 0,
  kLabelControlCode = 1,
  kLabelAffectedLength = 2,
  kLabelSpeedLimit = 3,
  kLabelQuantifier5bit = 4,
  kLabelQuantifier8bit = 5,
  kLabelSupplementary = 6,
  kLabelStartTime = 7,
  kLabelStopTime = 8,
  kLabelAdditionalEvent = 9,
  kLabelDetailedDiversion = 10,
  kLabelDestination= 11,
  kLabelCrossLinkage = 13,
  kLabelSeparator = 14
};

enum eControlCode {
  kControlIncreaseUrgency = 0,
  kControlReduceUrgency = 1,
  kControlChangeDirectionality = 2,
  kControlChangeDurationType = 3,
  kControlSetDiversion = 5,
  kControlIncreaseExtent8 = 6,
  kControlIncreaseExtent16 = 7
};

struct FreeformField {
  uint16_t label;
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

struct Event {
 public:
  Event() : description(""), description_with_quantifier(""), nature(0),
            quantifier_type(0), duration_type(0), directionality(0), urgency(0),
            update_class(0), allows_quantifier(false), show_duration(true) {}
  std::string description;
  std::string description_with_quantifier;
  uint16_t nature;
  uint16_t quantifier_type;
  uint16_t duration_type;
  uint16_t directionality;
  uint16_t urgency;
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
  uint16_t duration_type_;
  bool divertadv_;
  uint16_t direction_;
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
  uint16_t directionality_;
  uint16_t urgency_;
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
