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
#include "src/channel.h"

#include "src/common.h"

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
Channel::Channel(const Options& options, int which_channel) :
    options_(options),
    which_channel_(which_channel),
    cached_pi_(options.input_type),
    block_stream_(options), station_(0x0000, options, which_channel) {
}

Channel::Channel(const Channel& other) :
    options_(other.options_), which_channel_(other.which_channel_),
    cached_pi_(options_.input_type),
    block_stream_(options_), station_(cached_pi_.get(), options_, which_channel_),
    last_group_rx_time_(std::chrono::system_clock::now()) {
}

void Channel::processBit(bool bit) {
  block_stream_.pushBit(bit);

  if (block_stream_.hasGroupReady())
    processGroup(block_stream_.popGroup());
}

void Channel::processBits(BitBuffer buffer) {
  for (size_t i_bit = 0; i_bit < buffer.bits.size(); i_bit++) {
    block_stream_.pushBit(buffer.bits[i_bit]);

    if (block_stream_.hasGroupReady()) {
      Group group = block_stream_.popGroup();

      // Calculate this group's rx time based on the buffer timestamp and bit offset
      auto group_time = buffer.time_received -
        std::chrono::milliseconds(int((buffer.bits.size() - 1 - i_bit) / 1187.5 * 1e3));

      // When the source is faster than real-time, backwards timestamp calculation
      // produces meaningless results. We want to make sure that the time stays monotonic.
      if (group_time < last_group_rx_time_) {
        group_time = last_group_rx_time_;
      }

      group.setTime(group_time);
      processGroup(group);

      last_group_rx_time_ = group_time;
    }
  }
}

void Channel::processGroup(Group group) {
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
    bler_average_.push(group.getNumErrors() / 4.f);
    group.setAverageBLER(100.f * bler_average_.getAverage());
  }

  // If the PI code changes, all previously received data for the station
  // is cleared. We don't want this to happen on spurious bit errors, so
  // a change of PI code is only confirmed after a repeat.
  if (group.hasPI()) {
    auto pi_status = cached_pi_.update(group.getPI());
    switch (pi_status) {
      case CachedPI::Result::ChangeConfirmed:
        station_ = Station(cached_pi_.get(), options_, which_channel_);
        break;

      case CachedPI::Result::SpuriousChange:
        group.setBlock(BLOCK1, Block());
        break;

      case CachedPI::Result::NoChange:
        break;
    }
  }

  auto stream = options_.feed_thru ? &std::cerr : &std::cout;

  if (options_.output_type == redsea::OutputType::Hex) {
    if (!group.isEmpty()) {
      group.printHex(stream);
      if (options_.timestamp)
        *stream << ' ' << getTimePointString(group.getRxTime(), options_.time_format);
      *stream << '\n' << std::flush;
    }
  } else {
    station_.updateAndPrint(group, stream);
  }
}

// Process any remaining data
void Channel::flush() {
  Group last_group = block_stream_.flushCurrentGroup();
  if (!last_group.isEmpty())
    processGroup(last_group);
}

}  // namespace redsea
