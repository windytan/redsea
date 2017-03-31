#include "src/tmc/locationdb.h"

#include "config.h"
#ifdef ENABLE_TMC

#include <climits>
#include <deque>
#include <fstream>
#include <regex>
#include <sstream>
#include <string>
#include <utility>

#include "iconvpp/iconv.hpp"

#include "src/tmc/event_list.h"
#include "src/util.h"

namespace redsea {

namespace tmc {

namespace {

std::string to_utf8(std::string input, iconvpp::converter* conv) {
  std::string converted;
  conv->convert(input, converted);
  return converted;
}

}  // namespace

LocationDatabase LoadLocationDatabase(const std::string& directory) {
  LocationDatabase locdb;
  std::string encoding("UTF-8");

  for (std::vector<std::string> fields :
       ReadCSV(directory + "/README.DAT", ';')) {
    try {
      encoding = fields.at(4);
    } catch (const std::exception& e) {
      continue;
    }
  }

  // Misspelled encodings in the wild (TODO a better way to do this)
  if (std::regex_match(encoding, std::regex("ISO.8859-1\\b.*")))
    encoding = "ISO-8859-1";
  if (std::regex_match(encoding, std::regex("ISO.8859-15\\b.*")))
    encoding = "ISO-8859-15";

  iconvpp::converter conv("UTF-8", encoding);

  for (CSVRow row : ReadCSVWithTitles(directory + "/NAMES.DAT", ';')) {
    try {
      int nid = std::stoi(row.at("NID"));
      locdb.names[nid] = to_utf8(row.at("NAME"), &conv);
    } catch (const std::exception& e) {
      continue;
    }
  }

  for (CSVRow row : ReadCSVWithTitles(directory + "/ROADS.DAT", ';')) {
    try {
      Road road;
      road.lcd = std::stoi(row.at("LCD"));
      road.road_number = row.at("ROADNUMBER");
      int rnid = 0;
      if (!row.at("RNID").empty())
        rnid = std::stoi(row.at("RNID"));
      if (locdb.names.count(rnid) > 0)
        road.name = locdb.names[rnid];
      locdb.roads[road.lcd] = road;
    } catch (const std::exception& e) {
      continue;
    }
  }

  for (CSVRow row : ReadCSVWithTitles(directory + "/SEGMENTS.DAT", ';')) {
    try {
      Segment seg;
      seg.lcd = std::stoi(row.at("LCD"));
      seg.roa_lcd = std::stoi(row.at("ROA_LCD"));
      locdb.segments[seg.lcd] = seg;
    } catch (const std::exception& e) {
      continue;
    }
  }

  for (CSVRow row : ReadCSVWithTitles(directory + "/POINTS.DAT", ';')) {
    try {
      locdb.ltn = std::stoi(row.at("TABCD"));
      Point point;
      point.lcd = std::stoi(row.at("LCD"));
      int n1id = 0;
      if (!row.at("N1ID").empty())
        std::stoi(row.at("N1ID"));
      if (locdb.names.count(n1id) > 0)
        point.name1 = locdb.names[n1id];
      if (!row.at("XCOORD").empty())
        point.lon = std::stoi(row.at("XCOORD")) * 1e-5f;
      if (!row.at("YCOORD").empty())
        point.lat = std::stoi(row.at("YCOORD")) * 1e-5f;
      if (!row.at("ROA_LCD").empty())
        point.roa_lcd = std::stoi(row.at("ROA_LCD"));
      if (!row.at("SEG_LCD").empty())
        point.seg_lcd = std::stoi(row.at("SEG_LCD"));

      int rnid = 0;
      if (!row.at("RNID").empty())
        rnid = std::stoi(row.at("RNID"));
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

  for (CSVRow row : ReadCSVWithTitles(directory + "/POFFSETS.DAT", ';')) {
    try {
      int lcd = std::stoi(row.at("LCD"));
      int neg = std::stoi(row.at("NEG_OFF_LCD"));
      int pos = std::stoi(row.at("POS_OFF_LCD"));
      if (locdb.points.count(lcd) > 0) {
        locdb.points[lcd].neg_off = neg;
        locdb.points[lcd].pos_off = pos;
      }
    } catch (const std::exception& e) {
      continue;
    }
  }

  for (CSVRow row :
       ReadCSVWithTitles(directory + "/ADMINISTRATIVEAREA.DAT", ';')) {
    try {
      AdminArea area;
      area.lcd = std::stoi(row.at("LCD"));
      area.name = row.at("NID");
      locdb.admin_areas[area.lcd] = area;
    } catch (const std::exception& e) {
      continue;
    }
  }

  return locdb;
}

std::ostream& operator<<(std::ostream& strm, const LocationDatabase& locdb) {
  return strm << "{\"location_table_info\":{" <<
                 "\"ltn\":" << locdb.ltn << "," <<
                 "\"num_points\":" << locdb.points.size() << "," <<
                 "\"num_roads\":" << locdb.roads.size() << "," <<
                 "\"num_names\":" << locdb.names.size() <<
                 "}}" << std::endl;
}

}  // namespace tmc
}  // namespace redsea

#endif  // ENABLE_TMC
