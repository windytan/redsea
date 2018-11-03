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

#include <map>
#include <vector>

#include "config.h"
#include "src/groups.h"
#include "src/options.h"

namespace redsea {

class ErrorCorrectionResult {
 public:
  bool     succeeded      { false };
  uint32_t corrected_bits { 0 };
};

class BlockStream {
 public:
  explicit BlockStream(const Options& options);
  void PushBit(bool bit);
  std::vector<Group> PopGroups();
  bool eof() const;

 private:
  void AcquireSync(Block block);
  void UncorrectableErrorEncountered();
  void FindBlockInInputRegister();
  void NewGroupReceived();

  unsigned bitcount_                  { 0 };
  unsigned previous_syncing_bitcount_ { 0 };
  int      num_bits_until_next_block_ { 1 };
  uint32_t input_register_            { 0 };
  Offset   previous_syncing_offset_   { Offset::A };
  Offset   expected_offset_           { Offset::A };
  uint16_t pi_                        { 0x0000 };
  bool     is_in_sync_                { false };
  RunningSum<50> block_error_sum50_;
  const    Options options_;
  bool     is_eof_                    { false };
  RunningAverage<kNumBlerAverageGroups> bler_average_;
  Group current_group_;
  std::vector<Group> groups_;
};

} // namespace redsea
#endif // BLOCK_SYNC_H_
