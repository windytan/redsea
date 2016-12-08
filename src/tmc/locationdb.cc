#include "src/tmc/locationdb.h"

#include "config.h"
#ifdef ENABLE_TMC

#include <climits>
#include <deque>
#include <fstream>
#include <sstream>
#include <string>
#include <utility>

#include "iconvpp/iconv.hpp"
#include <json/json.h>

#include "src/tmc/event_list.h"
#include "src/util.h"

namespace redsea {

namespace tmc {

namespace {

std::string utf8(std::string input, iconvpp::converter* conv) {
  std::string converted;
  conv->convert(input, converted);
  return converted;
}

}  // namespace

LocationDatabase loadLocationDatabase(std::string directory) {
  LocationDatabase locdb(0);
  std::map<std::string, int> columns;
  std::string encoding("UTF-8");

  for (std::vector<std::string> fields :
       readCSV(directory + "/README.DAT", ';')) {
    try {
      encoding = fields.at(4);
    } catch (const std::exception& e) {
      continue;
    }
  }

  // Misspelled encodings in the wild
  if (encoding.compare("ISO 8859-1") == 0)
    encoding = "ISO-8859-1";

  iconvpp::converter conv("UTF-8", encoding);

  bool is_title_row = true;

  for (std::vector<std::string> fields :
       readCSV(directory + "/NAMES.DAT", ';')) {

    if (is_title_row) {
      for (size_t i=0; i<fields.size(); i++)
        columns[fields[i]] = i;
      is_title_row = false;
    } else {

      try {
        int nid = std::stoi(fields.at(columns.at("NID")));
        locdb.names[nid] = utf8(fields.at(columns.at("NAME")), &conv);
      } catch (const std::exception& e) {
        continue;
      }
    }
  }

  columns.clear();
  is_title_row = true;

  for (std::vector<std::string> fields :
       readCSV(directory + "/ROADS.DAT", ';')) {

    if (is_title_row) {
      for (size_t i=0; i<fields.size(); i++)
        columns[fields[i]] = i;
      is_title_row = false;
    } else {

      try {
        Road road;
        road.lcd = std::stoi(fields.at(columns.at("LCD")));
        road.road_number = fields.at(columns.at("ROADNUMBER"));
        int rnid = 0;
        if (!fields.at(columns.at("RNID")).empty())
          rnid = std::stoi(fields.at(columns.at("RNID")));
        if (locdb.names.count(rnid) > 0)
          road.name = locdb.names[rnid];
        locdb.roads[road.lcd] = road;
      } catch (const std::exception& e) {
        continue;
      }
    }
  }

  columns.clear();
  is_title_row = true;

  for (std::vector<std::string> fields :
       readCSV(directory + "/SEGMENTS.DAT", ';')) {

    if (is_title_row) {
      for (size_t i=0; i<fields.size(); i++)
        columns[fields[i]] = i;
      is_title_row = false;
    } else {

      try {
        Segment seg;
        seg.lcd = std::stoi(fields.at(2));
        seg.roa_lcd = std::stoi(fields.at(columns.at("ROA_LCD")));
        locdb.segments[seg.lcd] = seg;
      } catch (const std::exception& e) {
        continue;
      }
    }
  }

  columns.clear();
  is_title_row = true;

  for (std::vector<std::string> fields :
       readCSV(directory + "/POINTS.DAT", ';')) {

    if (is_title_row) {
      for (size_t i=0; i<fields.size(); i++)
        columns[fields[i]] = i;
      is_title_row = false;
    } else {

      try {
        locdb.ltn = std::stoi(fields.at(columns.at("TABCD")));
        Point point;
        point.lcd = std::stoi(fields.at(columns.at("LCD")));
        int n1id = 0;
        if (!fields.at(columns.at("N1ID")).empty())
          std::stoi(fields.at(columns.at("N1ID")));
        if (locdb.names.count(n1id) > 0)
          point.name1 = locdb.names[n1id];
        if (!fields.at(columns.at("XCOORD")).empty())
          point.lon = std::stoi(fields.at(columns.at("XCOORD"))) * 1e-5f;
        if (!fields.at(columns.at("YCOORD")).empty())
          point.lat = std::stoi(fields.at(columns.at("YCOORD"))) * 1e-5f;
        if (!fields.at(columns.at("ROA_LCD")).empty())
          point.roa_lcd = std::stoi(fields.at(columns.at("ROA_LCD")));
        if (!fields.at(columns.at("SEG_LCD")).empty())
          point.seg_lcd = std::stoi(fields.at(columns.at("SEG_LCD")));

        int rnid = 0;
        if (!fields.at(columns.at("RNID")).empty())
          std::stoi(fields.at(columns.at("RNID")));
        if (locdb.names.count(rnid) > 0)
          point.road_name = locdb.names[rnid];

        if (point.roa_lcd == 0 && locdb.segments.count(point.seg_lcd) > 0) {
          point.roa_lcd = locdb.segments.at(point.seg_lcd).roa_lcd;
          point.road_name = locdb.roads.at(point.roa_lcd).name;
          point.road_name = locdb.names[rnid];
        }

        locdb.points[point.lcd] = point;
      } catch (const std::exception& e) {
        continue;
      }
    }
  }

  columns.clear();
  is_title_row = true;

  for (std::vector<std::string> fields :
       readCSV(directory + "/POFFSETS.DAT", ';')) {

    if (is_title_row) {
      for (size_t i=0; i<fields.size(); i++)
        columns[fields[i]] = i;
      is_title_row = false;
    } else {

      try {
        int lcd = std::stoi(fields.at(columns.at("LCD")));
        int neg = std::stoi(fields.at(columns.at("NEG_OFF_LCD")));
        int pos = std::stoi(fields.at(columns.at("POS_OFF_LCD")));
        if (locdb.points.count(lcd) > 0) {
          locdb.points[lcd].neg_off = neg;
          locdb.points[lcd].pos_off = pos;
        }
      } catch (const std::exception& e) {
        continue;
      }
    }
  }

  printf("{\"location_table_info\":{\"ltn\":%d,\"num_points\":%ld,"
         "\"num_roads\":%ld,\"num_names\":%ld}}\n", locdb.ltn,
         locdb.points.size(), locdb.roads.size(), locdb.names.size());

  return locdb;
}

}  // namespace tmc
}  // namespace redsea

#endif  // ENABLE_TMC
