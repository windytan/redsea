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
#include "src/tmc/locationdb.hh"

#include <climits>
#include <cstdint>
#include <exception>
#include <ostream>
#include <regex>
#include <string>

#include "ext/iconvpp/iconv.hpp"

#include "src/tmc/csv.hh"

namespace redsea {

namespace tmc {

namespace {

// \throws Conversion errors from iconv
std::string to_utf8(const std::string& input, const iconvpp::converter& converter) {
  std::string converted;
  converter.convert(input, converted);
  return converted;
}

}  // namespace

std::uint16_t readLTN(const std::string& directory) {
  std::uint16_t ltn = 0;

  const CSVTable table = readCSVWithTitles(directory + "/LOCATIONDATASETS.DAT", ';');
  for (const CSVRow& row : table.rows) {
    try {
      ltn = get_uint16(table, row, "TABCD");
    } catch (const std::exception&) {
      continue;
    }
  }

  return ltn;
}

LocationDatabase loadLocationDatabase(const std::string& directory) {
  LocationDatabase locdb;
  std::string encoding("UTF-8");

  for (const auto& fields : readCSV(directory + "/README.DAT", ';')) {
    try {
      encoding = fields.at(4);
    } catch (const std::exception&) {
      continue;
    }
  }

  // Misspelled encodings in the wild (TODO a better way to do this)
  if (std::regex_match(encoding, std::regex("ISO.8859-1\\b.*")))
    encoding = "ISO-8859-1";
  if (std::regex_match(encoding, std::regex("ISO.8859-15\\b.*")))
    encoding = "ISO-8859-15";

  const iconvpp::converter converter("UTF-8", encoding);

  CSVTable table = readCSVWithTitles(directory + "/NAMES.DAT", ';');
  for (const CSVRow& row : table.rows) {
    try {
      const int nid    = get_int(table, row, "NID");
      locdb.names[nid] = to_utf8(get_string(table, row, "NAME"), converter);
    } catch (const std::exception&) {
      continue;
    }
  }

  table = readCSVWithTitles(directory + "/ROADS.DAT", ';');
  for (const CSVRow& row : table.rows) {
    try {
      Road road;
      road.lcd         = get_uint16(table, row, "LCD");
      road.road_number = get_string(table, row, "ROADNUMBER");
      int rnid         = 0;
      if (row_contains(table, row, "RNID"))
        rnid = get_int(table, row, "RNID");
      if (locdb.names.find(rnid) != locdb.names.end())
        road.name = locdb.names[rnid];
      locdb.roads[road.lcd] = road;
    } catch (const std::exception&) {
      continue;
    }
  }

  table = readCSVWithTitles(directory + "/SEGMENTS.DAT", ';');
  for (const CSVRow& row : table.rows) {
    try {
      Segment seg{};
      seg.lcd                 = get_uint16(table, row, "LCD");
      seg.roa_lcd             = get_uint16(table, row, "ROA_LCD");
      locdb.segments[seg.lcd] = seg;
    } catch (const std::exception&) {
      continue;
    }
  }

  table = readCSVWithTitles(directory + "/POINTS.DAT", ';');
  for (const CSVRow& row : table.rows) {
    try {
      locdb.ltn = get_uint16(table, row, "TABCD");
      Point point;
      point.lcd = get_uint16(table, row, "LCD");
      int n1id  = 0;
      if (row_contains(table, row, "N1ID"))
        n1id = get_int(table, row, "N1ID");
      if (locdb.names.find(n1id) != locdb.names.end())
        point.name1 = locdb.names[n1id];
      if (row_contains(table, row, "XCOORD"))
        point.lon = static_cast<float>(get_int(table, row, "XCOORD")) * 1e-5f;
      if (row_contains(table, row, "YCOORD"))
        point.lat = static_cast<float>(get_int(table, row, "YCOORD")) * 1e-5f;
      if (row_contains(table, row, "ROA_LCD"))
        point.roa_lcd = get_uint16(table, row, "ROA_LCD");
      if (row_contains(table, row, "SEG_LCD"))
        point.seg_lcd = get_uint16(table, row, "SEG_LCD");

      int rnid = 0;
      if (row_contains(table, row, "RNID"))
        rnid = get_int(table, row, "RNID");
      if (locdb.names.find(rnid) != locdb.names.end())
        point.road_name = locdb.names[rnid];

      if (point.roa_lcd == 0 && locdb.segments.find(point.seg_lcd) != locdb.segments.end()) {
        point.roa_lcd   = locdb.segments.at(point.seg_lcd).roa_lcd;
        point.road_name = locdb.roads.at(point.roa_lcd).name;
        point.road_name = locdb.names[rnid];
      }

      locdb.points[point.lcd] = point;
    } catch (const std::exception&) {
      continue;
    }
  }

  table = readCSVWithTitles(directory + "/POFFSETS.DAT", ';');
  for (const CSVRow& row : table.rows) {
    try {
      const auto lcd = get_uint16(table, row, "LCD");
      const auto neg = get_uint16(table, row, "NEG_OFF_LCD");
      const auto pos = get_uint16(table, row, "POS_OFF_LCD");
      if (locdb.points.find(lcd) != locdb.points.end()) {
        locdb.points[lcd].neg_off = neg;
        locdb.points[lcd].pos_off = pos;
      }
    } catch (const std::exception&) {
      continue;
    }
  }

  table = readCSVWithTitles(directory + "/ADMINISTRATIVEAREA.DAT", ';');
  for (const CSVRow& row : table.rows) {
    try {
      AdminArea area;
      area.lcd                    = get_uint16(table, row, "LCD");
      area.name                   = get_string(table, row, "NID");
      locdb.admin_areas[area.lcd] = area;
    } catch (const std::exception&) {
      continue;
    }
  }

  return locdb;
}

std::ostream& operator<<(std::ostream& strm, const LocationDatabase& locdb) {
  return strm << "{\"location_table_info\":{" << "\"ltn\":" << locdb.ltn << ","
              << "\"num_points\":" << locdb.points.size() << ","
              << "\"num_roads\":" << locdb.roads.size() << ","
              << "\"num_names\":" << locdb.names.size() << "}}" << std::endl;
}

}  // namespace tmc
}  // namespace redsea
