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
    which_channel_(which_channel), pi_(0x0000), prev_new_pi_(0x0000),
    new_pi_(0x0000),
    block_stream_(options), station_(pi_, options, which_channel),
#ifdef HAVE_LIQUID
    subcarrier_(options),
#endif
    bler_average_(kNumBlerAverageGroups) {
}

Channel::Channel(const Channel& other) :
    options_(other.options_), which_channel_(other.which_channel_),
    pi_(other.pi_), prev_new_pi_(other.prev_new_pi_), new_pi_(other.new_pi_),
    block_stream_(options_), station_(pi_, options_, which_channel_),
#ifdef HAVE_LIQUID
    subcarrier_(options_),
#endif
    bler_average_(kNumBlerAverageGroups) {
}

void Channel::ProcessChunk(const std::vector<float>& chunk) {
#ifdef HAVE_LIQUID
  subcarrier_.ProcessChunk(chunk);
  for (bool bit : subcarrier_.PopBits())
    ProcessBit(bit);
#endif
}

void Channel::ProcessBit(bool bit) {
  block_stream_.PushBit(bit);

  for (Group group : block_stream_.PopGroups())
    ProcessGroup(group);
}

void Channel::ProcessGroup(Group group) {
  if (options_.timestamp)
    group.set_time(std::chrono::system_clock::now());

  if (group.has_pi()) {
    // Repeated PI confirms change
    prev_new_pi_ = new_pi_;
    new_pi_ = group.pi();

    if (new_pi_ == prev_new_pi_ || options_.input_type == INPUT_HEX) {
      if (new_pi_ != pi_)
        station_ = Station(new_pi_, options_, which_channel_);
      pi_ = new_pi_;
    } else {
      return;
    }
  }

  if (options_.bler) {
    bler_average_.push(100.0f * group.num_errors() / 4);
    group.set_bler(bler_average_.average());
  }

  if (options_.output_type == redsea::OUTPUT_HEX) {
    redsea::PrintHexGroup(group, options_.feed_thru ?
        &std::cerr : &std::cout,
        options_.time_format);
  } else {
    station_.UpdateAndPrint(group, options_.feed_thru ?
        &std::cerr : &std::cout);
  }
}

}  // namespace redsea
