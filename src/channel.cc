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
#include "src/channel.hh"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iostream>

#include "src/constants.hh"
#include "src/io/bitbuffer.hh"
#include "src/io/output.hh"
#include "src/options.hh"
#include "src/util.hh"

namespace redsea {

/*
 * A Channel represents a single 'FM channel', or a multiplex signal on one
 * frequency. This also corresponds to channels in audio files. The station
 * on a channel may change, due to propagation changes etc.
 *
 * A Channel object can receive data either as chunks of MPX signal, single
 * bits, or groups. Usage of these inputs shouldn't be intermixed.
 *
 */
Channel::Channel(const Options& options, int which_channel, std::ostream& output_stream = std::cout)
    : options_(options),
      which_channel_(which_channel),
      output_stream_(output_stream),
      station_(options, which_channel) {
  for (auto& strm : block_stream_) {
    strm.init(options);
  }
}

// Used for testing (PI is already known)
Channel::Channel(const Options& options, std::ostream& output_stream, std::uint16_t pi)
    : options_(options), output_stream_(output_stream), station_(options, 0, pi) {
  for (auto& strm : block_stream_) {
    strm.init(options);
  }

  cached_pi_.update(pi);
  cached_pi_.update(pi);
}

// \param bit 0 or 1
// \param which_stream Which data stream was it received on, 0..3
void Channel::processBit(bool bit, std::size_t which_stream) {
  block_stream_[which_stream].pushBit(bit);

  if (block_stream_[which_stream].hasGroupReady())
    processGroup(block_stream_[which_stream].popGroup(), which_stream);
}

// \param which_stream Which data stream was it received on, 0..3
void Channel::processBits(const BitBuffer& buffer, std::size_t which_stream) {
  for (std::size_t i_bit = 0; i_bit < buffer.bits[which_stream].size(); i_bit++) {
    block_stream_[which_stream].pushBit(buffer.bits[which_stream].at(i_bit));

    if (block_stream_[which_stream].hasGroupReady()) {
      Group group = block_stream_[which_stream].popGroup();

      // Calculate this group's rx time based on the buffer timestamp and bit offset
      auto group_time = buffer.time_received -
                        std::chrono::milliseconds(static_cast<int>(
                            static_cast<double>(buffer.bits[which_stream].size() - 1 - i_bit) /
                            kBitsPerSecond * 1e3));

      // When the source is faster than real-time, backwards timestamp calculation
      // produces meaningless results. We want to make sure that the time stays monotonic.
      if (group_time < last_group_rx_time_) {
        group_time = last_group_rx_time_;
      }

      group.setTime(group_time);
      processGroup(group, which_stream);

      last_group_rx_time_ = group_time;
    }
  }
}

// Handle this group as if it was just received.
// \param which_stream Which data stream was it received on, 0..3
void Channel::processGroup(Group group, std::size_t which_stream) {
  if (options_.timestamp && !group.hasTime()) {
    auto now = std::chrono::system_clock::now();
    group.setTime(now);

    // When the source is faster than real-time, backwards timestamp calculation
    // produces meaningless results. We want to make sure that the time stays monotonic.
    if (now < last_group_rx_time_) {
      group.setTime(last_group_rx_time_);
    }
    last_group_rx_time_ = now;
  }

  if (options_.bler) {
    bler_average_.push(static_cast<float>(group.getNumErrors()) / 4.f);
    group.setAverageBLER(100.f * bler_average_.getAverage());
  }

  if (which_stream != 0) {
    group.setVersionC();
  }
  group.setDataStream(which_stream);

  // If the PI code changes, all previously received data for the station
  // is cleared. We don't want this to happen on spurious bit errors, so
  // a change of PI code is only confirmed after a repeat.
  if (group.hasPI()) {
    const auto pi_status = cached_pi_.update(group.getPI());
    switch (pi_status) {
      case CachedPI::Result::ChangeConfirmed:
        station_ = Station(options_, which_channel_, cached_pi_.get());
        break;

      case CachedPI::Result::SpuriousChange:
      case CachedPI::Result::NoChange:       break;
    }
  }

  if (options_.output_type == redsea::OutputType::Hex) {
    printAsHex(group, options_, output_stream_);
  } else {
    station_.updateAndPrint(group, output_stream_);
  }
}

// Process any remaining data
void Channel::flush() {
  for (std::size_t i = 1; i < block_stream_.size(); ++i) {
    const Group remaining_group = block_stream_[i].flushCurrentGroup();
    if (!remaining_group.isEmpty())
      processGroup(remaining_group, i);
  }
}

/// \note Not to be used for measurements - may lose precision
float Channel::getSecondsSinceCarrierLost() const {
  return static_cast<float>(block_stream_[0].getNumBitsSinceSyncLost()) / kBitsPerSecond;
}

void Channel::resetPI() {
  cached_pi_.reset();
}

}  // namespace redsea
