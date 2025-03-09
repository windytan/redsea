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
#ifndef BLOCK_SYNC_H_
#define BLOCK_SYNC_H_

#include <cstdint>

#include "src/common.h"
#include "src/groups.h"
#include "src/options.h"

namespace redsea {

struct SyncPulse {
  Offset offset{Offset::invalid};
  std::uint32_t bit_position{};

  bool couldFollow(const SyncPulse& other) const;
};

class SyncPulseBuffer {
 public:
  void push(Offset offset, std::uint32_t bitcount);
  bool isSequenceFound() const;

 private:
  std::array<SyncPulse, 4> pulses_;
};

struct ErrorCorrectionResult {
  bool succeeded{false};
  uint32_t corrected_bits{0};
};

class BlockStream {
 public:
  BlockStream() = default;
  void init(const Options& options);
  void pushBit(bool bit);
  Group popGroup();
  bool hasGroupReady() const;
  Group flushCurrentGroup() const;
  uint32_t getNumBitsSinceSyncLost() const;

 private:
  void acquireSync(Block block);
  void findBlockInInputRegister();
  void handleNewlyReceivedGroup();

  uint32_t bitcount_{0};
  uint32_t num_bits_until_next_block_{1};
  uint32_t input_register_{0};
  Offset expected_offset_{Offset::A};
  bool is_in_sync_{false};
  RunningSum<int, 50> block_error_sum50_;
  Options options_{};
  RunningAverage<float, kNumBlerAverageGroups> bler_average_;
  Group current_group_;
  Group ready_group_;
  bool has_group_ready_{false};
  uint32_t num_bits_since_sync_lost_{0};
  SyncPulseBuffer sync_buffer_{};
};

}  // namespace redsea
#endif  // BLOCK_SYNC_H_
