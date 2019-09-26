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
    block_stream_(options_), station_(cached_pi_.get(), options_, which_channel_) {
}

void Channel::processBit(bool bit) {
  block_stream_.pushBit(bit);

  for (Group group : block_stream_.popGroups())
    processGroup(group);
}

void Channel::processBits(std::vector<bool> bits) {
  for (bool bit : bits)
    processBit(bit);
}

void Channel::processGroup(Group group) {
  if (options_.timestamp)
    group.setTime(std::chrono::system_clock::now());

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

  if (options_.output_type == redsea::OutputType::Hex) {
    group.printHex(options_.feed_thru ?
        &std::cerr : &std::cout,
        options_.time_format);
  } else {
    station_.updateAndPrint(group, options_.feed_thru ?
        &std::cerr : &std::cout);
  }
}

// Process any remaining data
void Channel::flush() {
  Group last_group = block_stream_.flushCurrentGroup();
  if (!last_group.isEmpty())
    processGroup(last_group);
}

}  // namespace redsea
