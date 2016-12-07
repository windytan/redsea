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
  std::string name1;
  std::string road_name;
  uint16_t roa_lcd;
  uint16_t neg_off;
  uint16_t pos_off;
  float lon;
  float lat;
};

struct Road {

};

struct LocationDatabase {
  std::map<int, Point> points;
  std::map<int, Road> roads;
  std::map<int, std::string> names;
};

LocationDatabase loadLocationDatabase(std::string directory);

}  // namespace tmc
}  // namespace redsea

#endif  // ENABLE_TMC
#endif  // TMC_LOCATIONDB_H_
