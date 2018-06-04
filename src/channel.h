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
#include "src/subcarrier.h"

namespace redsea {

class Channel {
 public:
  Channel(const Options& options, int which_channel);
  Channel(const Channel& other);
  void ProcessChunk(MPXBuffer<>& chunk);
  void ProcessBit(bool bit);
  void ProcessGroup(Group group);

 private:
  Options options_;
  int which_channel_;
  uint16_t pi_;
  uint16_t prev_new_pi_;
  uint16_t new_pi_;
  BlockStream block_stream_;
  Station station_;
#ifdef HAVE_LIQUID
  Subcarrier subcarrier_;
#endif
  RunningAverage bler_average_;
};

}  // namespace redsea

#endif  // CHANNEL_H_
