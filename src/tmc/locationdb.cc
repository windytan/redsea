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
#include "src/tmc/locationdb.h"

#include "config.h"
#ifdef ENABLE_TMC

#include <climits>
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

std::string to_utf8(const std::string& input, iconvpp::converter& converter) {
  std::string converted;
  converter.convert(input, converted);
  return converted;
}

}  // namespace

uint16_t readLTN(const std::string& directory) {
  uint16_t ltn = 0;

  CSVTable table = readCSVWithTitles(directory + "/LOCATIONDATASETS.DAT", ';');
  for (CSVRow row : table.rows) {
    try {
      ltn = get_uint16(table, row, "TABCD");
    } catch (const std::exception& e) {
      continue;
    }
  }

  return ltn;
}

LocationDatabase loadLocationDatabase(const std::string& directory) {
  LocationDatabase locdb;
  std::string encoding("UTF-8");

  for (std::vector<std::string> fields :
       readCSV(directory + "/README.DAT", ';')) {
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

  iconvpp::converter converter("UTF-8", encoding);

  CSVTable table = readCSVWithTitles(directory + "/NAMES.DAT", ';');
  for (CSVRow row : table.rows) {
    try {
      int nid = get_int(table, row, "NID");
      locdb.names[nid] = to_utf8(get_string(table, row, "NAME"), converter);
    } catch (const std::exception& e) {
      continue;
    }
  }

  table = readCSVWithTitles(directory + "/ROADS.DAT", ';');
  for (CSVRow row : table.rows) {
    try {
      Road road;
      road.lcd = get_uint16(table, row, "LCD");
      road.road_number = get_string(table, row, "ROADNUMBER");
      int rnid = 0;
      if (row_contains(table, row, "RNID"))
        rnid = get_int(table, row, "RNID");
      if (locdb.names.count(rnid) > 0)
        road.name = locdb.names[rnid];
      locdb.roads[road.lcd] = road;
    } catch (const std::exception& e) {
      continue;
    }
  }

  table = readCSVWithTitles(directory + "/SEGMENTS.DAT", ';');
  for (CSVRow row : table.rows) {
    try {
      Segment seg;
      seg.lcd = get_uint16(table, row, "LCD");
      seg.roa_lcd = get_uint16(table, row, "ROA_LCD");
      locdb.segments[seg.lcd] = seg;
    } catch (const std::exception& e) {
      continue;
    }
  }

  table = readCSVWithTitles(directory + "/POINTS.DAT", ';');
  for (CSVRow row : table.rows) {
    try {
      locdb.ltn = get_uint16(table, row, "TABCD");
      Point point;
      point.lcd = get_uint16(table, row, "LCD");
      int n1id = 0;
      if (row_contains(table, row, "N1ID"))
        n1id = get_int(table, row, "N1ID");
      if (locdb.names.count(n1id) > 0)
        point.name1 = locdb.names[n1id];
      if (row_contains(table, row, "XCOORD"))
        point.lon = get_int(table, row, "XCOORD") * 1e-5f;
      if (row_contains(table, row, "YCOORD"))
        point.lat = get_int(table, row, "YCOORD") * 1e-5f;
      if (row_contains(table, row, "ROA_LCD"))
        point.roa_lcd = get_uint16(table, row, "ROA_LCD");
      if (row_contains(table, row, "SEG_LCD"))
        point.seg_lcd = get_uint16(table, row, "SEG_LCD");

      int rnid = 0;
      if (row_contains(table, row, "RNID"))
        rnid = get_int(table, row, "RNID");
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

  table = readCSVWithTitles(directory + "/POFFSETS.DAT", ';');
  for (CSVRow row : table.rows) {
    try {
      uint16_t lcd = get_uint16(table, row, "LCD");
      uint16_t neg = get_uint16(table, row, "NEG_OFF_LCD");
      uint16_t pos = get_uint16(table, row, "POS_OFF_LCD");
      if (locdb.points.count(lcd) > 0) {
        locdb.points[lcd].neg_off = neg;
        locdb.points[lcd].pos_off = pos;
      }
    } catch (const std::exception& e) {
      continue;
    }
  }

  table = readCSVWithTitles(directory + "/ADMINISTRATIVEAREA.DAT", ';');
  for (CSVRow row : table.rows) {
    try {
      AdminArea area;
      area.lcd = get_uint16(table, row, "LCD");
      area.name = get_string(table, row, "NID");
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
