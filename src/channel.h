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

#include "config.h"

#include "src/block_sync.h"
#include "src/common.h"
#include "src/options.h"

namespace redsea {

class CachedPI {
 public:
  enum class Result {
    ChangeConfirmed, NoChange, SpuriousChange
  };

  CachedPI(InputType input_type) :
    bypass_(input_type == InputType::Hex) {}
  Result Update(uint16_t pi) {
    pi_prev2_ = pi_prev1_;
    pi_prev1_ = pi;

    Result status (Result::SpuriousChange);

    if (pi_prev1_ == pi_prev2_ || bypass_) {
      status = (pi == pi_confirmed_ ? Result::NoChange :
                                      Result::ChangeConfirmed);
      pi_confirmed_ = pi;
    }
    return status;
  }
  uint16_t Get() const {
    return pi_confirmed_;
  }

 private:
  uint16_t pi_confirmed_ { 0 };
  uint16_t pi_prev1_     { 0 };
  uint16_t pi_prev2_     { 0 };
  bool     bypass_       { false };
};

class Channel {
 public:
  Channel(const Options& options, int which_channel);
  Channel(const Channel& other);
  void ProcessBit(bool bit);
  void ProcessBits(std::vector<bool> bits);
  void ProcessGroup(Group group);
  void Flush();

 private:
  Options options_;
  int which_channel_;
  CachedPI cached_pi_;
  BlockStream block_stream_;
  Station station_;
  RunningAverage<float, kNumBlerAverageGroups> bler_average_;
};

}  // namespace redsea

#endif  // CHANNEL_H_
