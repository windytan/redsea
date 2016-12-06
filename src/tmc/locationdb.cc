#include "src/tmc/locationdb.h"

#include "config.h"
#ifdef ENABLE_TMC

#include <climits>
#include <deque>
#include <fstream>
#include <sstream>
#include <string>
#include <utility>

#include <json/json.h>

#include "src/tmc/event_list.h"
#include "src/util.h"

namespace redsea {

namespace tmc {

LocationDatabase loadLocationDatabase(std::string directory) {
  LocationDatabase locdb;
  std::map<std::string, int> columns;

  bool is_title_row = true;
  for (std::vector<std::string> fields :
       readCSV(directory + "/NAMES.DAT", ';', 5)) {

    if (is_title_row) {
      for (size_t i=0; i<fields.size(); i++)
        columns[fields[i]] = i;
      is_title_row = false;
    }

    try {
      int nid = std::stoi(fields[columns.at("CID")]);
      locdb.names[nid] = fields[columns.at("NID")];
    } catch (const std::exception& e) {
      continue;
    }
  }

  columns.clear();
  is_title_row = true;

  for (std::vector<std::string> fields :
       readCSV(directory + "/POINTS.DAT", ';', 26)) {

    if (is_title_row) {
      for (size_t i=0; i<fields.size(); i++)
        columns[fields[i]] = i;
      is_title_row = false;
    }

    try {
      int lcd = std::stoi(fields[columns.at("LCD")]);
      int rnid = std::stoi(fields[columns.at("RNID")]);
      int n1id = std::stoi(fields[columns.at("N1ID")]);
      Point point;
      if (locdb.names.count(n1id) > 0)
        point.name1 = locdb.names[n1id];
      if (locdb.names.count(rnid) > 0)
        point.road_name = locdb.names[rnid];
      point.lon = std::stoi(fields[columns.at("XCOORD")]) * 1e-5f;
      point.lat = std::stoi(fields[columns.at("YCOORD")]) * 1e-5f;
      locdb.points[lcd] = point;
    } catch (const std::exception& e) {
      continue;
    }
  }
  return locdb;
}

}  // namespace tmc
}  // namespace redsea

#endif  // ENABLE_TMC
