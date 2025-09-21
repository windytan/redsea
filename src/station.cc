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
#include "src/station.hh"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <ctime>
#include <ostream>
#include <string>

#include "src/decode/decode.hh"
#include "src/decode/decode_oda.hh"
#include "src/group.hh"
#include "src/io/output.hh"
#include "src/io/tree.hh"
#include "src/maybe.hh"
#include "src/options.hh"
#include "src/tables.hh"
#include "src/text/stringutil.hh"
#include "src/util.hh"

namespace redsea {

/*
 * A Station represents a single broadcast carrier identified by its RDS PI
 * code.
 *
 * @param which_channel Which PCM channel the station is on
 */
Station::Station(const Options& options, int which_channel)
    : options_(options), which_channel_(which_channel), tmc_(options) {}

Station::Station(const Options& options, int which_channel, std::uint16_t pi)
    : Station(options, which_channel) {
  // A delegating constructor can't have other mem-initializers
  // NOLINTBEGIN(cppcoreguidelines-prefer-member-initializer)
  pi_     = pi;
  has_pi_ = true;
  // NOLINTEND(cppcoreguidelines-prefer-member-initializer)
}

// @param stream The stream to print to (not to be confused with RDS2 data streams)
void Station::updateAndPrint(const Group& group, std::ostream& stream) {
  if (!has_pi_) {
    return;
  }

  ObjectTree tree;

  if (options_.streams) {
    tree["stream"] = group.getDataStream();
  }

  if (group.getType().version != GroupType::Version::C) {
    // Allow 1 group with missed PI. For subsequent misses, don't process at all.
    if (group.hasPI()) {
      last_group_had_pi_ = true;
    } else if (last_group_had_pi_) {
      last_group_had_pi_ = false;
    } else {
      return;
    }

    if (group.isEmpty()) {
      return;
    }

    tree["pi"] = getPrefixedHexString(getPI(), 4);
    if (options_.rbds) {
      const std::string callsign{getCallsignFromPI(getPI())};
      if (!callsign.empty()) {
        if ((getPI() & 0xF000U) == 0x1000U) {
          tree["callsign_uncertain"] = callsign;
        } else {
          tree["callsign"] = callsign;
        }
      }
    }
  }  // if not C

  if (options_.timestamp) {
    tree["rx_time"] = getTimePointString(group.getRxTime(), options_.time_format);
  }

  if (group.hasBLER()) {
    tree["bler"] = std::lround(group.getBLER());
  }

  if (options_.num_channels > 1) {
    tree["channel"] = which_channel_;
  }

  if (options_.show_raw) {
    tree["raw_data"] = formatHex(group);
  }

  decodeBasics(group, tree, options_.rbds);

  // ODA support in groups
  // ---------------------
  //
  // -  can't be used for ODA
  // o  can be used for ODA
  // O  ODA only
  //
  //             111111
  //   0123456789012345
  // A -----ooooo-OOo--
  // B ---OOooOOOOOOO--

  if (group.hasType()) {
    const GroupType& type = group.getType();

    if (type.version == GroupType::Version::C) {
      decodeC(group, tree, oda_app_for_pipe_, rft_file_);
      // These groups can't be used for ODA
    } else if (type.number == 0) {
      decodeType0(group, tree, alt_freq_list_, ps_, options_.show_partial);
    } else if (type.number == 1) {
      decodeType1(group, tree, slc_, pi_);
    } else if (type.number == 2) {
      decodeType2(group, tree, radiotext_, options_.show_partial);
    } else if (type.number == 3 && type.version == GroupType::Version::A) {
      decodeType3A(group, tree, oda_app_for_group_, radiotext_, ert_, tmc_);
    } else if (type.number == 4 && type.version == GroupType::Version::A) {
      decodeType4A(group, tree);
    } else if (type.number == 10 && type.version == GroupType::Version::A) {
      decodeType10A(group, tree, ptyname_);
    } else if (type.number == 14) {
      decodeType14(group, tree, eon_ps_names_, eon_alt_freqs_, options_.rbds);
    } else if (type.number == 15 && type.version == GroupType::Version::B) {
      decodeType15B(group, tree);

      // Other groups can be reassigned for ODA by a 3A group
    } else if (oda_app_for_group_.contains(type)) {
      decodeODAGroup(group, tree, oda_app_for_group_, radiotext_, ert_, tmc_);

      // Below: Groups that could optionally be used for ODA but have
      // another primary function
    } else if (type.number == 5) {
      decodeType5(group, tree, full_tdc_);
    } else if (type.number == 6) {
      decodeType6(group, tree);
    } else if (type.number == 7 && type.version == GroupType::Version::A) {
      decodeType7A(group, tree);
    } else if (type.number == 8 && type.version == GroupType::Version::A) {
      if (group.has(BLOCK2) && group.has(BLOCK3) && group.has(BLOCK4))
        tmc_.receiveUserGroup(getBits(group.get(BLOCK2), 0, 5), group.get(BLOCK3),
                              group.get(BLOCK4), tree);
    } else if (type.number == 9 && type.version == GroupType::Version::A) {
      decodeType9A(group, tree);

    } else if (type.number == 15 && type.version == GroupType::Version::A) {
      decodeType15A(group, tree, long_ps_, options_.show_partial);

      // ODA-only groups
      // 3B, 4B, 7B, 8B, 9B, 10B, 11A, 11B, 12A, 12B, 13B
    } else {
      decodeODAGroup(group, tree, oda_app_for_group_, radiotext_, ert_, tmc_);
    }
  }

  if (options_.time_from_start && group.getTimeFromStart().valid) {
    tree["time_from_start"] = group.getTimeFromStart().data;
  }

  if (tree.empty())
    return;

  printAsJson(tree, stream);
}

std::uint16_t Station::getPI() const {
  return pi_;
}

}  // namespace redsea
