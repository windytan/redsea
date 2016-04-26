#ifndef TMC_EVENTS_H_
#define TMC_EVENTS_H_

#include <map>
#include <string>

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
  Q_SMALL_NUMBER, Q_NUMBER, Q_LESS_THAN_METRES, Q_PERCENT, Q_UPTO_KMH, Q_UPTO_TIME, Q_DEG_CELSIUS, Q_TIME, Q_TONNES, Q_METRES, Q_UPTO_MILLIMETRES, Q_MHZ, Q_KHZ
};


class Event {
  public:
    Event();
    Event(std::string, std::string, uint16_t, uint16_t, uint16_t, uint16_t, uint16_t, uint16_t);
    std::string description;
    std::string description_with_quantifier;
    uint16_t nature;
    uint16_t quantifier_type;
    uint16_t duration_type;
    uint16_t directionality;
    uint16_t urgency;
    uint16_t update_class;

};

Event getEvent(uint16_t code);



} // namespace redsea
} // namespace tmc
#endif // TMC_EVENTS_H_
