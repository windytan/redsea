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
#include "src/tmc/tmc.hh"

#include <array>
#include <cassert>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>
#include <vector>

#include "src/options.hh"
#include "src/tables.hh"
#include "src/tmc/eventdb.hh"
#include "src/tmc/locationdb.hh"
#include "src/tmc/message.hh"
#include "src/util/csv.hh"
#include "src/util/tree.hh"
#include "src/util/util.hh"

namespace redsea::tmc {
namespace {

std::map<std::uint16_t, LocationDatabase> g_location_databases;

std::vector<std::string> getScopeStrings(std::uint16_t mgs) {
  const bool mgs_i{getBool(mgs, 3)};
  const bool mgs_n{getBool(mgs, 2)};
  const bool mgs_r{getBool(mgs, 1)};
  const bool mgs_u{getBool(mgs, 0)};

  std::vector<std::string> scope;
  if (mgs_i)
    scope.emplace_back("inter-road");
  if (mgs_n)
    scope.emplace_back("national");
  if (mgs_r)
    scope.emplace_back("regional");
  if (mgs_u)
    scope.emplace_back("urban");

  return scope;
}

std::map<std::uint16_t, ServiceKey> loadServiceKeyTable() {
  std::map<std::uint16_t, ServiceKey> result;

  for (const CSVRow& row : readCSV("service_key_table.csv", ',')) {
    if (row.lengths.size() < 4)
      continue;

    std::uint16_t encid{};
    ServiceKey key;

    try {
      encid        = get_uint16(row, 0);
      key.xorval   = get_uint16(row, 1);
      key.xorstart = get_uint16(row, 2);
      key.nrot     = get_uint16(row, 3);
    } catch (...) {
      continue;
    }

    result.insert({encid, key});
  }

  return result;
}

void decodeLocation(const LocationDatabase& db, Message& message, std::uint16_t ltn,
                    ObjectTree& out) {
  if (db.ltn != ltn || db.ltn == 0 || !out["tmc"]["message"].contains("location"))
    return;

  const auto lcd         = message.getLocation();
  const int extent       = message.getExtent();
  const bool is_positive = (extent >= 0);

  if (db.points.find(lcd) != db.points.end()) {
    std::vector<Point> points;
    int points_left        = std::abs(extent) + 1;
    std::uint16_t this_lcd = lcd;
    while (points_left > 0 && db.points.find(this_lcd) != db.points.end()) {
      points.push_back(db.points.at(this_lcd));
      this_lcd = (is_positive ? db.points.at(this_lcd).pos_off : db.points.at(this_lcd).neg_off);
      points_left--;
    }

    for (std::size_t i = 0; i < points.size(); i++) {
      out["tmc"]["message"]["coordinates"][i]["lat"] = static_cast<double>(points[i].lat);
      out["tmc"]["message"]["coordinates"][i]["lon"] = static_cast<double>(points[i].lon);
    }

    if (points.size() > 1 && points.at(0).name1.length() > 0 &&
        points.at(points.size() - 1).name1.length() > 0) {
      out["tmc"]["message"]["span_from"] = points.at(0).name1;
      out["tmc"]["message"]["span_to"]   = points.at(points.size() - 1).name1;
    }
    const std::uint16_t roa_lcd = db.points.at(lcd).roa_lcd;
    if (db.roads.find(roa_lcd) != db.roads.end()) {
      const Road road = db.roads.at(roa_lcd);
      if (!road.road_number.empty())
        out["tmc"]["message"]["road_number"] = road.road_number;
      if (road.name.length() > 0)
        out["tmc"]["message"]["road_name"] = road.name;
      else if (!db.points.at(lcd).road_name.empty())
        out["tmc"]["message"]["road_name"] = db.points.at(lcd).road_name;
    }
  }
}

}  // namespace

TMCService::TMCService(const Options& options)
    : message_(is_encrypted_), service_key_table_(loadServiceKeyTable()), ps_(8) {
  if (!options.loctable_dirs.empty() && g_location_databases.empty()) {
    for (const std::string& loctable_dir : options.loctable_dirs) {
      const auto ltn            = readLTN(loctable_dir);
      g_location_databases[ltn] = loadLocationDatabase(loctable_dir);
      if (options.feed_thru)
        static_cast<void>(
            std::fprintf(stderr, "%s\n", g_location_databases[ltn].toString().c_str()));
      else
        static_cast<void>(std::printf("%s\n", g_location_databases[ltn].toString().c_str()));
    }
  }
}

void TMCService::receiveSystemGroup(std::uint16_t message, ObjectTree& out) {
  const auto variant = getBits<2>(message, 14);

  if (variant == 0) {
    if (isEventDataEmpty())
      loadEventData();

    is_initialized_ = true;
    const auto ltn  = getBits<6>(message, 6);

    is_encrypted_                             = (ltn == 0);
    out["tmc"]["system_info"]["is_encrypted"] = is_encrypted_;

    if (!is_encrypted_) {
      ltn_                                        = ltn;
      out["tmc"]["system_info"]["location_table"] = ltn_;
    }

    const bool afi = getBool(message, 5);
    const auto mgs = getBits<4>(message, 0);

    out["tmc"]["system_info"]["is_on_alt_freqs"] = afi;

    for (const std::string& s : getScopeStrings(mgs))
      out["tmc"]["system_info"]["scope"].push_back(s);
  } else if (variant == 1) {
    sid_                                    = getBits<6>(message, 6);
    out["tmc"]["system_info"]["service_id"] = sid_;

    const auto g                        = getBits<2>(message, 12);
    const std::array<int, 4> gap_values = {3, 5, 8, 11};
    out["tmc"]["system_info"]["gap"]    = gap_values[g];

    ltcc_ = getBits<4>(message, 0);
    if (ltcc_ > 0)
      out["tmc"]["system_info"]["ltcc"] = ltcc_;
  } else if (variant == 2) {
    const auto ltecc = getUint8(message, 0);
    if (ltecc > 0) {
      out["tmc"]["system_info"]["ltecc"] = ltecc;
      if (ltcc_ > 0) {
        out["tmc"]["system_info"]["country"] = getCountryString(ltcc_, ltecc);
      }
    }
  }
}

void TMCService::receiveUserGroup(std::uint16_t x, std::uint16_t y, std::uint16_t z,
                                  ObjectTree& out) {
  if (!is_initialized_)
    return;

  const bool t = getBool(x, 4);

  // Encryption administration group
  if (getBits<5>(x, 0) == 0x00) {
    sid_       = getBits<6>(y, 5);
    encid_     = getBits<5>(y, 0);
    ltn_       = getBits<6>(z, 10);
    has_encid_ = true;

    out["tmc"]["system_info"]["service_id"]     = sid_;
    out["tmc"]["system_info"]["encryption_id"]  = encid_;
    out["tmc"]["system_info"]["location_table"] = ltn_;

    // Tuning information
  } else if (t) {
    const auto variant = getBits<4>(x, 0);

    switch (variant) {
      case 4:
      case 5: {
        const std::size_t pos = 4U * (variant - 4U);

        ps_.set(pos + 0, getUint8(y, 8));
        ps_.set(pos + 1, getUint8(y, 0));
        ps_.set(pos + 2, getUint8(z, 8));
        ps_.set(pos + 3, getUint8(z, 0));

        if (ps_.isComplete())
          out["tmc"]["service_provider"] = ps_.getLastCompleteString();
        break;
      }

      case 6: {
        const std::uint16_t on_pi = z;
        if (other_network_freqs_.find(on_pi) == other_network_freqs_.end())
          other_network_freqs_.insert({on_pi, AltFreqList()});

        other_network_freqs_.at(on_pi).insert(getUint8(y, 8));
        other_network_freqs_.at(on_pi).insert(getUint8(y, 0));

        /* Here, the alternative frequencies are printed out right away -
           DKULTUR, for example, does not transmit information about the total
           length of the list */
        out["tmc"]["other_network"]["pi"] = getPrefixedHexString<4>(on_pi);
        for (const int frequency : other_network_freqs_.at(on_pi).getRawList())
          out["tmc"]["other_network"]["frequencies_khz"].push_back(frequency);
        other_network_freqs_.clear();
        break;
      }

      case 8: {
        if (y == 0 || z == 0 || y == z) {
          out["tmc"]["other_network"]["pi"] = getPrefixedHexString<4>(y);
        } else {
          out["tmc"]["other_network"]["pi_codes"].push_back(getPrefixedHexString<4>(y));
          out["tmc"]["other_network"]["pi_codes"].push_back(getPrefixedHexString<4>(z));
        }
        break;
      }

      case 9: {
        const auto on_pi  = z;
        const auto on_sid = getBits<6>(y, 0);
        const auto on_mgs = getBits<4>(y, 6);
        const auto on_ltn = getBits<6>(y, 10);

        out["tmc"]["other_network"]["pi"]             = getPrefixedHexString<4>(on_pi);
        out["tmc"]["other_network"]["service_id"]     = on_sid;
        out["tmc"]["other_network"]["location_table"] = on_ltn;

        for (const std::string& s : getScopeStrings(on_mgs))
          out["tmc"]["other_network"]["scope"].push_back(s);
        break;
      }

      default: {
        out["debug"].push_back("TODO: TMC tuning info variant " + std::to_string(variant));
        break;
      }
    }

    // User message
  } else {
    if (is_encrypted_ && !has_encid_)
      return;

    const bool f = getBool(x, 3);

    // Single-group message
    if (f) {
      Message single_message(is_encrypted_);
      single_message.pushSingle(x, y, z);

      if (is_encrypted_ && service_key_table_.find(encid_) != service_key_table_.end())
        single_message.decrypt(service_key_table_[encid_]);

      if (!single_message.tree().empty()) {
        out["tmc"]["message"] = single_message.tree();
        decodeLocation(g_location_databases[ltn_], single_message, ltn_, out);
      }

      // Part of multi-group message
    } else {
      const auto continuity_index = getBits<3>(x, 0);

      if (continuity_index != message_.getContinuityIndex())
        message_ = Message(is_encrypted_);

      message_.pushMulti(x, y, z);
      if (message_.isComplete()) {
        if (is_encrypted_ && service_key_table_.find(encid_) != service_key_table_.end())
          message_.decrypt(service_key_table_[encid_]);

        if (!message_.tree().empty()) {
          out["tmc"]["message"] = message_.tree();
          decodeLocation(g_location_databases[ltn_], message_, ltn_, out);
        }
        message_ = Message(is_encrypted_);
      }
    }
  }
}

}  // namespace redsea::tmc
