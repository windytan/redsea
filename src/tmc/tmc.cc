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

#include <algorithm>
#include <array>
#include <cassert>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include "src/freq.hh"
#include "src/io/tree.hh"
#include "src/options.hh"
#include "src/tables.hh"
#include "src/text/stringutil.hh"
#include "src/tmc/csv.hh"
#include "src/tmc/eventdb.hh"
#include "src/tmc/locationdb.hh"
#include "src/util.hh"

namespace redsea::tmc {

namespace {

// Feature request #63: Multiple location databases
std::vector<std::pair<std::uint16_t, LocationDatabase>> g_location_databases;

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

std::vector<std::pair<std::uint16_t, ServiceKey>> loadServiceKeyTable() {
  std::vector<std::pair<std::uint16_t, ServiceKey>> result;

  for (const CSVRow& row : readCSV("service_key_table.csv", ',')) {
    if (row.lengths.size() < 4)
      continue;

    std::uint16_t encid{};
    ServiceKey key;

    try {
      encid        = static_cast<std::uint16_t>(std::stoi(row.at(0)));
      key.xorval   = static_cast<std::uint8_t>(std::stoi(row.at(1)));
      key.xorstart = static_cast<std::uint8_t>(std::stoi(row.at(2)));
      key.nrot     = static_cast<std::uint8_t>(std::stoi(row.at(3)));
    } catch (const std::exception&) {
      continue;
    }

    result.emplace_back(encid, key);
  }

  return result;
}

void decodeLocation(const Message& message, const LocationDatabase& db, std::uint16_t ltn,
                    ObjectTree& treeroot) {
  if (db.ltn != ltn || db.ltn == 0 || !message.hasLocation())
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

    for (const auto& point : points) {
      ObjectTree coord;
      coord["lat"] = static_cast<double>(point.lat);
      coord["lon"] = static_cast<double>(point.lon);
      treeroot["tmc"]["message"]["coordinates"].push_back(coord);
    }

    if (points.size() > 1 && points.at(0).name1.length() > 0 &&
        points.at(points.size() - 1).name1.length() > 0) {
      treeroot["tmc"]["message"]["span_from"] = points.at(0).name1;
      treeroot["tmc"]["message"]["span_to"]   = points.at(points.size() - 1).name1;
    }
    const std::uint16_t roa_lcd = db.points.at(lcd).roa_lcd;
    //      std::uint16_t seg_lcd = db.points.at(lcd).seg_lcd;
    //      treeroot["tmc"]["message"]["seg_lcd"] = seg_lcd;
    //      treeroot["tmc"]["message"]["roa_lcd"] = roa_lcd;
    if (db.roads.find(roa_lcd) != db.roads.end()) {
      const Road road = db.roads.at(roa_lcd);
      if (!road.road_number.empty())
        treeroot["tmc"]["message"]["road_number"] = road.road_number;
      if (road.name.length() > 0)
        treeroot["tmc"]["message"]["road_name"] = road.name;
      else if (!db.points.at(lcd).road_name.empty())
        treeroot["tmc"]["message"]["road_name"] = db.points.at(lcd).road_name;
    }
  }
}

// If the proper database is available
void decodeLocationIfPossible(const Message& message, std::uint16_t ltn, ObjectTree& treeroot) {
  if (ltn == 0 || !message.hasLocation())
    return;

  const auto locdb = std::find_if(g_location_databases.begin(), g_location_databases.end(),
                                  [ltn](const auto& pair) { return pair.first == ltn; });
  if (locdb != g_location_databases.end()) {
    decodeLocation(message, locdb->second, ltn, treeroot);
  }
}

}  // namespace

TMCService::TMCService(const Options& options)
    : message_(is_encrypted_), service_key_table_(loadServiceKeyTable()), ps_(8) {
  g_location_databases.reserve(kMaxNumLocationDatabases);

  if (!options.loctable_dirs.empty() && g_location_databases.empty()) {
    for (const std::string& loctable_dir : options.loctable_dirs) {
      const auto ltn = readLTN(loctable_dir);
      g_location_databases.emplace_back(ltn, loadLocationDatabase(loctable_dir));
      if (options.feed_thru)
        std::cerr << g_location_databases.back().second;
      else
        std::cout << g_location_databases.back().second;
    }

    // If user provided a location database, preload the event data as well
    loadEventData();
  }
}

void TMCService::receiveSystemGroup(std::uint16_t message, ObjectTree& treeroot) {
  const auto variant = getBits(message, 14, 2);

  if (variant == 0) {
    if (isEventDataEmpty())
      loadEventData();

    is_initialized_ = true;
    const auto ltn  = getBits(message, 6, 6);

    is_encrypted_                                  = (ltn == 0);
    treeroot["tmc"]["system_info"]["is_encrypted"] = is_encrypted_;

    if (!is_encrypted_) {
      ltn_                                             = ltn;
      treeroot["tmc"]["system_info"]["location_table"] = ltn_;
    }

    const bool afi = getBool(message, 5);
    const auto mgs = getBits(message, 0, 4);

    treeroot["tmc"]["system_info"]["is_on_alt_freqs"] = afi;

    for (const std::string& s : getScopeStrings(mgs))
      treeroot["tmc"]["system_info"]["scope"].push_back(s);
  } else if (variant == 1) {
    sid_                                         = getBits(message, 6, 6);
    treeroot["tmc"]["system_info"]["service_id"] = sid_;

    const auto g                          = getBits(message, 12, 2);
    const std::array<int, 4> gap_values   = {3, 5, 8, 11};
    treeroot["tmc"]["system_info"]["gap"] = gap_values[g];

    ltcc_ = getBits(message, 0, 4);
    if (ltcc_ > 0)
      treeroot["tmc"]["system_info"]["ltcc"] = ltcc_;
  } else if (variant == 2) {
    const auto ltecc = getUint8(message, 0);
    if (ltecc > 0) {
      treeroot["tmc"]["system_info"]["ltecc"] = ltecc;
      if (ltcc_ > 0) {
        treeroot["tmc"]["system_info"]["country"] = getCountryString(ltcc_, ltecc);
      }
    }
  }
}

void TMCService::receiveUserGroup(std::uint16_t x, std::uint16_t y, std::uint16_t z,
                                  ObjectTree& treeroot) {
  if (!is_initialized_)
    return;

  const bool t = getBool(x, 4);

  // Encryption administration group
  if (getBits(x, 0, 5) == 0x00) {
    sid_       = getBits(y, 5, 6);
    encid_     = getBits(y, 0, 5);
    ltn_       = getBits(z, 10, 6);
    has_encid_ = true;

    treeroot["tmc"]["system_info"]["service_id"]     = sid_;
    treeroot["tmc"]["system_info"]["encryption_id"]  = encid_;
    treeroot["tmc"]["system_info"]["location_table"] = ltn_;

    // Tuning information
  } else if (t) {
    const auto variant = getBits(x, 0, 4);

    switch (variant) {
      case 4:
      case 5: {
        const std::size_t pos = 4U * (variant - 4U);

        ps_.set(pos + 0, getUint8(y, 8));
        ps_.set(pos + 1, getUint8(y, 0));
        ps_.set(pos + 2, getUint8(z, 8));
        ps_.set(pos + 3, getUint8(z, 0));

        if (ps_.isComplete())
          treeroot["tmc"]["service_provider"] = ps_.getLastCompleteString();
        break;
      }

      case 6: {
        const std::uint16_t on_pi = z;
        if (std::find_if(other_network_freqs_.begin(), other_network_freqs_.end(),
                         [on_pi](const auto& pair) { return pair.first == on_pi; }) ==
            other_network_freqs_.end())
          other_network_freqs_.emplace_back(on_pi, AltFreqList());

        const auto on_freqs_it =
            std::find_if(other_network_freqs_.begin(), other_network_freqs_.end(),
                         [on_pi](const auto& pair) { return pair.first == on_pi; });

        on_freqs_it->second.insert(getUint8(y, 8));
        on_freqs_it->second.insert(getUint8(y, 0));

        /* Here, the alternative frequencies are printed out right away -
           DKULTUR, for example, does not transmit information about the total
           length of the list */
        treeroot["tmc"]["other_network"]["pi"] = getPrefixedHexString(on_pi, 4);
        for (const int frequency : on_freqs_it->second.getRawList())
          treeroot["tmc"]["other_network"]["frequencies_khz"].push_back(frequency);
        other_network_freqs_.clear();
        break;
      }

      case 8: {
        if (y == 0 || z == 0 || y == z) {
          treeroot["tmc"]["other_network"]["pi"] = getPrefixedHexString(y, 4);
        } else {
          treeroot["tmc"]["other_network"]["pi_codes"].push_back(getPrefixedHexString(y, 4));
          treeroot["tmc"]["other_network"]["pi_codes"].push_back(getPrefixedHexString(z, 4));
        }
        break;
      }

      case 9: {
        const auto on_pi  = z;
        const auto on_sid = getBits(y, 0, 6);
        const auto on_mgs = getBits(y, 6, 4);
        const auto on_ltn = getBits(y, 10, 6);

        treeroot["tmc"]["other_network"]["pi"]             = getPrefixedHexString(on_pi, 4);
        treeroot["tmc"]["other_network"]["service_id"]     = on_sid;
        treeroot["tmc"]["other_network"]["location_table"] = on_ltn;

        for (const std::string& s : getScopeStrings(on_mgs))
          treeroot["tmc"]["other_network"]["scope"].push_back(s);
        break;
      }

      default: {
        treeroot["debug"].push_back("TODO: TMC tuning info variant " + std::to_string(variant));
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

      if (is_encrypted_) {
        const auto key_it = std::find_if(service_key_table_.begin(), service_key_table_.end(),
                                         [this](const auto& pair) { return pair.first == encid_; });
        if (key_it != service_key_table_.end())
          single_message.decrypt(key_it->second);
      }

      if (!single_message.tree().empty()) {
        treeroot["tmc"]["message"] = single_message.tree();

        decodeLocationIfPossible(single_message, ltn_, treeroot);
      }

      // Part of multi-group message
    } else {
      const auto continuity_index = getBits(x, 0, 3);

      if (continuity_index != message_.getContinuityIndex())
        message_ = Message(is_encrypted_);

      message_.pushMulti(x, y, z);
      if (message_.isComplete()) {
        if (is_encrypted_) {
          const auto key_it =
              std::find_if(service_key_table_.begin(), service_key_table_.end(),
                           [this](const auto& pair) { return pair.first == encid_; });
          if (key_it != service_key_table_.end())
            message_.decrypt(key_it->second);
        }

        if (!message_.tree().empty()) {
          treeroot["tmc"]["message"] = message_.tree();

          decodeLocationIfPossible(message_, ltn_, treeroot);
        }
        message_ = Message(is_encrypted_);
      }
    }
  }
}

}  // namespace redsea::tmc
