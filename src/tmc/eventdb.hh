#ifndef TMC_EVENTDB_H_
#define TMC_EVENTDB_H_

#include <array>
#include <cstdint>
#include <string>

namespace redsea::tmc {

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

enum class Direction : std::uint8_t { Positive = 0, Negative = 1 };

enum class EventNature : std::uint8_t { Event = 0, Forecast = 1, Silent = 2 };

enum class EventDirectionality : std::uint8_t { Single = 0, Both = 1 };

enum class EventUrgency : std::uint8_t { None = 0, U = 1, X = 2 };

enum class DurationType : std::uint8_t { Dynamic = 0, LongerLasting = 1 };

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

std::uint16_t getQuantifierSize(QuantifierType qtype);
bool isValidEventCode(std::uint16_t code);
Event getEvent(std::uint16_t code);
bool isValidSupplementaryCode(std::uint16_t code);
std::string getSupplementaryDescription(std::uint16_t code);
std::string getUrgencyString(EventUrgency u);

extern const std::array<std::string, 1553> tmc_raw_data_events;
extern const std::array<std::string, 233> tmc_raw_data_suppl;

bool isEventDataEmpty();
void loadEventData();

}  // namespace redsea::tmc

#endif  // TMC_EVENTDB_H_
