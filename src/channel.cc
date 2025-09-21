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

CachedPI::Result CachedPI::update(const std::uint16_t pi) {
  Result status(Result::SpuriousChange);

  // Three repeats of the same PI --> confirmed change
  if (has_previous_ && pi_prev1_ == pi_prev2_ && pi == pi_prev1_) {
    status        = (pi == pi_confirmed_ ? Result::NoChange : Result::ChangeConfirmed);
    pi_confirmed_ = pi;
  }

  // So noisy that two PIs in a row get corrupted --> drop
  if (has_previous_ && (pi != pi_confirmed_ && pi_prev1_ != pi_confirmed_ && pi != pi_prev1_)) {
    reset();
  } else {
    has_previous_ = true;
  }

  pi_prev2_ = pi_prev1_;
  pi_prev1_ = pi;

  return status;
}

std::uint16_t CachedPI::get() const {
  return pi_confirmed_;
}

void CachedPI::reset() {
  pi_confirmed_ = pi_prev1_ = pi_prev2_ = 0;
  has_previous_                         = false;
}

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
void Channel::processBit(bool bit, int which_stream) {
  block_stream_[which_stream].pushBit(bit);

  if (block_stream_[which_stream].hasGroupReady())
    processGroup(block_stream_[which_stream].popGroup(), which_stream);
}

// \param buffer A bit buffer corresponding to 1 chunk
void Channel::processBits(const BitBuffer& buffer) {
  for (int which_stream{}; which_stream < buffer.n_streams; which_stream++) {
    for (int i_bit = 0; i_bit < static_cast<int>(buffer.bits[which_stream].size()); i_bit++) {
      block_stream_[which_stream].pushBit(buffer.bits[which_stream].at(i_bit).value);

      if (options_.time_from_start) {
        delayed_time_offset_[which_stream].push(
            buffer.chunk_time_from_start +
            buffer.bits[which_stream].at(i_bit).time_from_chunk_start);
      }

      if (block_stream_[which_stream].hasGroupReady()) {
        Group group = block_stream_[which_stream].popGroup();

        if (options_.timestamp) {
          // Calculate this group's rx time (in system clock time) based on the buffer timestamp and
          // bit offset
          auto group_time = buffer.time_received -
                            std::chrono::milliseconds(static_cast<int>(
                                static_cast<double>(buffer.bits[which_stream].size() - 1 - i_bit) /
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
          group.setTimeFromStart(delayed_time_offset_[which_stream].get());
        }

        processGroup(group, which_stream);
      }
    }
  }
}

// Handle this group as if it was just received.
// \param which_stream Which data stream was it received on, 0..3
void Channel::processGroup(Group group, int which_stream) {
  // If the rx timestamp wasn't set from the MPX buffer
  if (options_.timestamp && !group.hasRxTime()) {
    auto now = std::chrono::system_clock::now();
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
  for (std::size_t which_stream = 0; which_stream < block_stream_.size(); ++which_stream) {
    const Group remaining_group = block_stream_[which_stream].flushCurrentGroup();
    if (!remaining_group.isEmpty())
      processGroup(remaining_group, static_cast<int>(which_stream));
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
