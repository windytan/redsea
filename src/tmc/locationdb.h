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
#ifndef TMC_LOCATIONDB_H_
#define TMC_LOCATIONDB_H_

#include <iostream>
#include <map>
#include <string>

#include "src/rdsstring.h"

namespace redsea {
namespace tmc {

struct Point {
  uint16_t lcd          { 0 };
  std::string name1     { "" };
  std::string road_name { "" };
  uint16_t roa_lcd      { 0 };
  uint16_t seg_lcd      { 0 };
  uint16_t neg_off      { 0 };
  uint16_t pos_off      { 0 };
  float lon             { 0.f };
  float lat             { 0.f };
};

struct Segment {
  uint16_t lcd;
  uint16_t roa_lcd;
};

struct Road {
  uint16_t lcd            { 0 };
  std::string road_number { "" };
  std::string name        { "" };
  std::string name1       { "" };
};

struct AdminArea {
  uint16_t lcd;
  std::string name;
};

struct LocationDatabase {
  uint16_t ltn { 0 };
  std::map<uint16_t, Point> points;
  std::map<uint16_t, Road> roads;
  std::map<int, std::string> names;
  std::map<uint16_t, Segment> segments;
  std::map<uint16_t, AdminArea> admin_areas;
};
std::ostream& operator<<(std::ostream& strm, const LocationDatabase& locdb);

LocationDatabase loadLocationDatabase(const std::string& directory);

// Read and return the location table number of a location database
uint16_t readLTN(const std::string& directory);

}  // namespace tmc
}  // namespace redsea

#endif  // TMC_LOCATIONDB_H_
