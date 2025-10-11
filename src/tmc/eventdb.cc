#include "eventdb.hh"

#include <cstdint>
#include <exception>
#include <map>
#include <string>
#include <utility>

#include "src/util/csv.hh"

namespace redsea::tmc {

namespace {

std::map<std::uint16_t, Event> g_event_db;
std::map<std::uint16_t, std::string> g_supplementary_descriptions;

}  // namespace

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

bool isValidEventCode(std::uint16_t code) {
  return g_event_db.find(code) != g_event_db.end();
}

// Return a predefined TMC event by its code.
Event getEvent(std::uint16_t code) {
  if (g_event_db.find(code) != g_event_db.end())
    return g_event_db.find(code)->second;
  else
    return {};
}

bool isValidSupplementaryCode(std::uint16_t code) {
  return g_supplementary_descriptions.find(code) != g_supplementary_descriptions.end();
}

std::string getSupplementaryDescription(std::uint16_t code) {
  if (g_supplementary_descriptions.find(code) != g_supplementary_descriptions.end())
    return g_supplementary_descriptions.find(code)->second;
  else
    return "";
}

std::string_view getUrgencyString(EventUrgency u) {
  switch (u) {
    case EventUrgency::None: return "none";
    case EventUrgency::U:    return "U";
    case EventUrgency::X:    return "X";
  }
  return "none";
}

bool isEventDataEmpty() {
  return g_event_db.empty();
}

void loadEventData() {
  const CSVTable table{readCSVContainerWithTitles(tmc_raw_data_events, ';')};

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

      if (get_string(table, row, "T").find('D') != std::string::npos)
        event.duration_type = DurationType::Dynamic;
      else if (get_string(table, row, "T").find('L') != std::string::npos)
        event.duration_type = DurationType::LongerLasting;

      if (get_string(table, row, "T").find('(') != std::string::npos)
        event.show_duration = false;

      if (row_contains(table, row, "D") && get_int(table, row, "D") == 2)
        event.directionality = EventDirectionality::Both;

      event.update_class = get_uint16(table, row, "C");

      g_event_db[code] = std::move(event);
    } catch (const std::exception&) {
      continue;
    }
  }

  for (const CSVRow& row : readCSVContainer(tmc_raw_data_suppl, ';')) {
    if (row.lengths.size() < 2)
      continue;

    const auto code = get_uint16(row, 0);
    const auto desc = row.at(1);

    g_supplementary_descriptions.insert({code, std::string{desc}});
  }
}

}  // namespace redsea::tmc
