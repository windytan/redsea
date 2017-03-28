#ifndef TMC_LOCATIONDB_H_
#define TMC_LOCATIONDB_H_

#include "config.h"
#ifdef ENABLE_TMC

#include <iostream>
#include <map>
#include <string>
#include <vector>

#include <json/json.h>

#include "src/rdsstring.h"

namespace redsea {
namespace tmc {

struct Point {
  Point() : lcd(0), name1(""), road_name(""), roa_lcd(0), neg_off(0),
            pos_off(0), lon(0), lat(0) {}
  uint16_t lcd;
  std::string name1;
  std::string road_name;
  uint16_t roa_lcd;
  uint16_t seg_lcd;
  uint16_t neg_off;
  uint16_t pos_off;
  float lon;
  float lat;
};

struct Segment {
  uint16_t lcd;
  uint16_t roa_lcd;
};

struct Road {
  Road() : lcd(), road_number(""), name(""), name1("") {}
  uint16_t lcd;
  std::string road_number;
  std::string name;
  std::string name1;
};

struct AdminArea {
  uint16_t lcd;
  std::string name;
};

struct LocationDatabase {
  LocationDatabase() : ltn(0) {}
  uint16_t ltn;
  std::map<uint16_t, Point> points;
  std::map<uint16_t, Road> roads;
  std::map<int, std::string> names;
  std::map<uint16_t, Segment> segments;
  std::map<uint16_t, AdminArea> admin_areas;
};

LocationDatabase LoadLocationDatabase(std::string directory);
std::ostream& operator<<(std::ostream& strm, const LocationDatabase& locdb);

}  // namespace tmc
}  // namespace redsea

#endif  // ENABLE_TMC
#endif  // TMC_LOCATIONDB_H_
