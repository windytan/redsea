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
#include "src/rdsstring.h"
#include "src/tmc/locationdb.h"

namespace redsea {
namespace tmc {

enum eDirection {
  DIR_POSITIVE, DIR_NEGATIVE
};
enum eEventNature {
  EVENT_INFO, EVENT_FORECAST
};
enum eEventDirectionality {
  DIR_SINGLE, DIR_BOTH
};
enum eEventUrgency {
  URGENCY_NONE, URGENCY_U, URGENCY_X
};
enum eDurationType {
  DURATION_DYNAMIC, DURATION_LASTING
};
enum eQuantifierType {
  Q_SMALL_NUMBER, Q_NUMBER, Q_LESS_THAN_METRES, Q_PERCENT, Q_UPTO_KMH,
  Q_UPTO_TIME, Q_DEG_CELSIUS, Q_TIME, Q_TONNES, Q_METRES, Q_UPTO_MILLIMETRES,
  Q_MHZ, Q_KHZ
};

struct ServiceKey {
  ServiceKey() {}
  ServiceKey(uint8_t _xorval, uint8_t _xorstart, uint8_t _nrot) :
    xorval(_xorval), xorstart(_xorstart), nrot(_nrot) {}
  uint8_t xorval;
  uint8_t xorstart;
  uint8_t nrot;
};

class Event {
 public:
  Event();
  Event(std::string, std::string, uint16_t, uint16_t, uint16_t, uint16_t,
      uint16_t, uint16_t, bool);
  std::string description;
  std::string description_with_quantifier;
  uint16_t nature;
  uint16_t quantifier_type;
  uint16_t duration_type;
  uint16_t directionality;
  uint16_t urgency;
  uint16_t update_class;
  bool allows_quantifier;
};

Event getEvent(uint16_t code);

struct MessagePart {
  MessagePart() : is_received(false), data() {}
  MessagePart(bool _is_received, std::vector<uint16_t> _data) :
    is_received(_is_received), data(_data) {}
  bool is_received;
  std::vector<uint16_t> data;
};

class Message {
 public:
  explicit Message(bool is_loc_encrypted);
  void pushMulti(uint16_t x, uint16_t y, uint16_t z);
  void pushSingle(uint16_t x, uint16_t y, uint16_t z);
  Json::Value json() const;
  void decrypt(ServiceKey);
  bool isComplete() const;
  void clear();
  uint16_t getContinuityIndex() const;

 private:
  void decodeMulti();
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
  TMC(Options options);
  void systemGroup(uint16_t message, Json::Value*);
  void userGroup(uint16_t x, uint16_t y, uint16_t z, Json::Value*);

 private:
  bool is_initialized_;
  bool is_encrypted_;
  bool has_encid_;
  uint16_t ltn_;
  uint16_t sid_;
  uint16_t encid_;
  Message message_;
  std::map<uint16_t, ServiceKey> service_key_table_;
  RDSString ps_;
};

}  // namespace tmc
}  // namespace redsea

#endif  // ENABLE_TMC
#endif  // TMC_H_
