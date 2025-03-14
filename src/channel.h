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
#ifndef CHANNEL_H_
#define CHANNEL_H_

#include <array>
#include <iostream>

#include "src/block_sync.h"
#include "src/common.h"
#include "src/options.h"

namespace redsea {

// Normally, the PI code is not expected to change. This class keeps track of the current PI
// code and ignores spurious bit errors.
class CachedPI {
 public:
  enum class Result { ChangeConfirmed, NoChange, SpuriousChange };

  CachedPI() = default;

  // Input the most recently received PI code.
  Result update(const uint16_t pi) {
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
  uint16_t get() const {
    return pi_confirmed_;
  }
  void reset() {
    pi_confirmed_ = pi_prev1_ = pi_prev2_ = 0;
    has_previous_                         = false;
  }

 private:
  uint16_t pi_confirmed_{0};
  uint16_t pi_prev1_{0};
  uint16_t pi_prev2_{0};
  bool has_previous_{false};
};

class Channel {
 public:
  Channel(const Options& options, int which_channel, std::ostream& output_stream);
  Channel(const Options& options, std::ostream& output_stream, uint16_t pi);
  void processBit(bool bit, std::size_t which_stream);
  void processBits(const BitBuffer& buffer, std::size_t which_stream);
  void processGroup(Group group, std::size_t which_stream);
  void flush();
  float getSecondsSinceCarrierLost() const;
  void resetPI();

 private:
  Options options_{};
  int which_channel_{};
  std::ostream& output_stream_;
  CachedPI cached_pi_;
  std::array<BlockStream, 4> block_stream_;
  Station station_;
  RunningAverage<float, kNumBlerAverageGroups> bler_average_;
  std::chrono::time_point<std::chrono::system_clock> last_group_rx_time_;
};

}  // namespace redsea

#endif  // CHANNEL_H_
