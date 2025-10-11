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
#include <iosfwd>

#include "src/constants.hh"
#include "src/io/bitbuffer.hh"
#include "src/io/output.hh"
#include "src/options.hh"
#include "src/util/util.hh"

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
Channel::Channel(const Options& options, int which_channel)
    : options_(options), which_channel_(which_channel), station_(options, which_channel) {
  for (auto& block_stream : block_streams_) {
    block_stream.init(options);
  }
}

// Used for testing (PI is already known)
Channel::Channel(const Options& options, int which_channel, std::uint16_t pi)
    : options_(options), which_channel_(which_channel), station_(options, which_channel, pi) {
  for (auto& block_stream : block_streams_) {
    block_stream.init(options);
  }

  cached_pi_.update(pi);
  cached_pi_.update(pi);
}

// \param bit 0 or 1
// \param which_data_stream Which data stream was it received on, 0..3
void Channel::processBit(bool bit, std::size_t which_data_stream, std::ostream& output_ostream) {
  block_streams_[which_data_stream].pushBit(bit);

  if (block_streams_[which_data_stream].hasGroupReady())
    processAndPrintGroup(block_streams_[which_data_stream].popGroup(), which_data_stream,
                         output_ostream);
}

// \brief Process a bitbuffer, print out any complete groups etc.
// \param buffer A bit buffer corresponding to 1 chunk of MPX
void Channel::processBits(const BitBuffer& buffer, std::ostream& output_ostream) {
  for (int which_data_stream{}; which_data_stream < buffer.n_streams; which_data_stream++) {
    for (std::size_t i_bit = 0; i_bit < buffer.bits[which_data_stream].size(); i_bit++) {
      block_streams_[which_data_stream].pushBit(buffer.bits[which_data_stream].at(i_bit).value);

      if (options_.time_from_start) {
        delayed_time_offset_[which_data_stream].push(
            buffer.chunk_time_from_start +
            buffer.bits[which_data_stream].at(i_bit).time_from_chunk_start);
      }

      if (block_streams_[which_data_stream].hasGroupReady()) {
        Group group = block_streams_[which_data_stream].popGroup();

        if (options_.timestamp) {
          // Calculate this group's rx time (in system clock time) based on the buffer timestamp and
          // bit offset
          auto group_time =
              buffer.time_received -
              std::chrono::milliseconds(static_cast<int>(
                  static_cast<double>(buffer.bits[which_data_stream].size() - 1 - i_bit) /
                  kBitsPerSecond * 1e3));

          // When the source is faster than real-time, backwards timestamp calculation
          // produces meaningless results. We want to make sure that the time stays monotonic.
          if (group_time < last_group_rx_time_) {
            group_time = last_group_rx_time_;
          }

          group.setRxTime(group_time);

          last_group_rx_time_ = group_time;
        }

        if (options_.time_from_start) {
          // Remember the time_from_start from 104 bits ago = first bit of this group
          group.setTimeFromStart(delayed_time_offset_[which_data_stream].get());
        }

        processAndPrintGroup(group, which_data_stream, output_ostream);
      }
    }
  }
}

/// \brief Process and print this group as if it was just received.
/// \param which_data_stream Which data stream was it received on, 0..3
/// \param output_ostream Where to print the output
void Channel::processAndPrintGroup(Group group, std::size_t which_data_stream,
                                   std::ostream& output_ostream) {
  // If the rx timestamp wasn't set from the MPX buffer we'll set it now (hex/bits input?)
  if (options_.timestamp && !group.getRxTime().has_value) {
    const auto now = std::chrono::system_clock::now();
    group.setRxTime(now);

    // When the source is faster than real-time, backwards timestamp calculation
    // produces meaningless results. We want to make sure that the time stays monotonic.
    if (now < last_group_rx_time_) {
      group.setRxTime(last_group_rx_time_);
    }
    last_group_rx_time_ = now;
  }

  if (options_.bler) {
    bler_average_.push(static_cast<float>(group.getNumErrors()) / 4.f);
    group.setAverageBLER(100.f * bler_average_.getAverage());
  }

  if (which_data_stream != 0) {
    group.setVersionC();
  }
  group.setDataStream(which_data_stream);

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
    printAsHex(group, options_, output_ostream);
  } else {
    station_.updateAndPrint(group, output_ostream);
  }
}

// Process any remaining data
void Channel::flush(std::ostream& output_ostream) {
  for (std::size_t which_stream = 0; which_stream < block_streams_.size(); ++which_stream) {
    const Group remaining_group = block_streams_[which_stream].flushCurrentGroup();
    if (!remaining_group.isEmpty())
      processAndPrintGroup(remaining_group, which_stream, output_ostream);
  }
}

/// \note Not to be used for measurements - may lose precision
float Channel::getSecondsSinceCarrierLost() const {
  return static_cast<float>(block_streams_[0].getNumBitsSinceSyncLost()) / kBitsPerSecond;
}

void Channel::resetPI() {
  cached_pi_.reset();
}

}  // namespace redsea
