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
#include "src/block_sync.h"

#include <map>
#include <utility>
#include <vector>

namespace redsea {

constexpr unsigned kBlockLength  = 26;
constexpr unsigned kBlockBitmask = (1 << kBlockLength) - 1;
constexpr unsigned kCheckwordLength = 10;

// Each offset word is associated with one block number
constexpr eBlockNumber BlockNumberForOffset(Offset offset) {
  switch (offset) {
    case Offset::A       : return BLOCK1; break;
    case Offset::B       : return BLOCK2; break;
    case Offset::C       : return BLOCK3; break;
    case Offset::Cprime  : return BLOCK3; break;
    case Offset::D       : return BLOCK4; break;

    case Offset::invalid : return BLOCK1; break;
  }
}

// Return the next offset word in sequence
constexpr Offset NextOffsetFor(Offset this_offset) {
  switch (this_offset) {
    case Offset::A       : return Offset::B; break;
    case Offset::B       : return Offset::C; break;
    case Offset::C       : return Offset::D; break;
    case Offset::Cprime  : return Offset::D; break;
    case Offset::D       : return Offset::A; break;

    case Offset::invalid : return Offset::A; break;
  }
}

// IEC 62106:2015 section B.3.1 Table B.1
constexpr Offset OffsetForSyndrome(uint16_t syndrome) {
  switch (syndrome) {
    case 0b1111011000 : return Offset::A;       break;
    case 0b1111010100 : return Offset::B;       break;
    case 0b1001011100 : return Offset::C;       break;
    case 0b1111001100 : return Offset::Cprime;  break;
    case 0b1001011000 : return Offset::D;       break;

    default           : return Offset::invalid; break;
  }
}

// EN 50067:1998, section B.1.1: '-- calculated by the modulo-two addition of
// all the rows of the -- matrix for which the corresponding coefficient in the
// -- vector is 1.'
uint32_t MatrixMultiply(uint32_t vec, const std::vector<uint32_t>& matrix) {
  uint32_t result = 0;

  for (size_t k = 0; k < matrix.size(); k++)
    if ((vec >> k) & 0b1)
      result ^= matrix[matrix.size() - 1 - k];

  return result;
}

// EN 50067:1998, section B.2.1: 'The calculation of the syndromes for the
// different offset words can easily be done by multiplying each word with the
// parity matrix H.'
uint32_t CalculateSyndrome(uint32_t vec) {
  static const std::vector<uint32_t> parity_check_matrix({
    0b1000000000,
    0b0100000000,
    0b0010000000,
    0b0001000000,
    0b0000100000,
    0b0000010000,
    0b0000001000,
    0b0000000100,
    0b0000000010,
    0b0000000001,
    0b1011011100,
    0b0101101110,
    0b0010110111,
    0b1010000111,
    0b1110011111,
    0b1100010011,
    0b1101010101,
    0b1101110110,
    0b0110111011,
    0b1000000001,
    0b1111011100,
    0b0111101110,
    0b0011110111,
    0b1010100111,
    0b1110001111,
    0b1100011011
  });

  return MatrixMultiply(vec, parity_check_matrix);
}

// Precompute mapping of syndromes to error vectors
// IEC 62106:2015 section B.3.1
std::map<std::pair<uint16_t, Offset>, uint32_t> MakeErrorLookupTable() {
  std::map<std::pair<uint16_t, Offset>, uint32_t> lookup_table;

  // Table B.1
  const std::map<Offset, uint16_t> offset_words({
      { Offset::A,      0b0011111100 },
      { Offset::B,      0b0110011000 },
      { Offset::C,      0b0101101000 },
      { Offset::Cprime, 0b1101010000 },
      { Offset::D,      0b0110110100 }
  });

  for (auto offset : offset_words) {
    // Kopitz & Marks 1999: "RDS: The Radio Data System", p. 224:
    // "...the error-correction system should be enabled, but should be
    // restricted by attempting to correct bursts of errors spanning one or two
    // bits."
    for (uint32_t error_bits : {0b1, 0b11}) {
      for (unsigned shift = 0; shift < kBlockLength; shift++) {
        uint32_t error_vector = ((error_bits << shift) & kBlockBitmask);

        uint32_t syndrome =
            CalculateSyndrome(error_vector ^ offset.second);
        lookup_table.insert({{syndrome, offset.first}, error_vector});
      }
    }
  }
  return lookup_table;
}

// EN 50067:1998, section B.2.2
ErrorCorrectionResult CorrectBurstErrors(Block block, Offset expected_offset) {
  static const auto error_lookup_table = MakeErrorLookupTable();

  ErrorCorrectionResult result;

  uint16_t syndrome = CalculateSyndrome(block.raw);
  result.corrected_bits = block.raw;

  auto search = error_lookup_table.find({syndrome, expected_offset});
  if (search != error_lookup_table.end()) {
    uint32_t err = search->second;
    result.corrected_bits ^= err;
    result.succeeded = true;
  }

  return result;
}

void SyncPulseBuffer::Push(Offset offset, int bitcount) {
  for (size_t i = 0; i < pulses.size() - 1; i++) {
    pulses[i] = pulses[i + 1];
  }
  pulses.back() = {offset, bitcount};
};

bool SyncPulseBuffer::SequenceFound() const {
  for (size_t prev_i = 0; prev_i < pulses.size() - 1; prev_i++) {
    int sync_distance = pulses.back().bitcount - pulses[prev_i].bitcount;

    bool found = (sync_distance % kBlockLength == 0 &&
                  sync_distance / kBlockLength <= 6 &&
                  pulses[prev_i].offset != Offset::invalid &&
      (BlockNumberForOffset(pulses[prev_i].offset) + sync_distance / kBlockLength) % 4 ==
       BlockNumberForOffset(pulses.back().offset));

    if (found)
      return true;
  }
  return false;
}

BlockStream::BlockStream(const Options& options) :
  options_(options) {
}

void BlockStream::UncorrectableErrorEncountered() {
  // EN 50067:1998, section C.1.2:
  // Sync is lost when >45 out of last 50 blocks are erroneous
  if (is_in_sync_ && block_error_sum50_.sum() > 45) {
    is_in_sync_ = false;
    block_error_sum50_.clear();
  }
}

void BlockStream::AcquireSync(Block block) {
  static SyncPulseBuffer sync_buffer;

  if (is_in_sync_)
    return;

  // Try to find a repeating offset sequence
  if (block.offset != Offset::invalid) {
    sync_buffer.Push(block.offset, bitcount_);

    if (sync_buffer.SequenceFound()) {
      is_in_sync_ = true;
      expected_offset_ = block.offset;
      current_group_ = Group();
    } else {
      previous_syncing_bitcount_= bitcount_;
      previous_syncing_offset_  = block.offset;
    }
  }
}

void BlockStream::PushBit(bool bit) {
  input_register_ = (input_register_ << 1) + bit;
  num_bits_until_next_block_--;
  bitcount_++;

  if (num_bits_until_next_block_ == 0) {
    FindBlockInInputRegister();

    num_bits_until_next_block_ = is_in_sync_ ? kBlockLength : 1;
  }
}

void BlockStream::FindBlockInInputRegister() {
  Block block;
  block.raw    = input_register_ & kBlockBitmask;
  block.offset = OffsetForSyndrome(CalculateSyndrome(block.raw));

  AcquireSync(block);

  if (is_in_sync_) {
    if (expected_offset_ == Offset::C && block.offset == Offset::Cprime)
      expected_offset_ = Offset::Cprime;

    block.had_errors = (block.offset != expected_offset_);
    block_error_sum50_.push(block.had_errors);

    block.data = block.raw >> kCheckwordLength;

    if (block.had_errors) {
      auto correction = CorrectBurstErrors(block, expected_offset_);
      if (correction.succeeded) {
        block.data   = correction.corrected_bits >> kCheckwordLength;
        block.offset = expected_offset_;
      } else {
        UncorrectableErrorEncountered();
      }
    }

    // Error-free block received or errors successfully corrected
    if (block.offset == expected_offset_) {
      block.is_received = true;
      current_group_.set_block(BlockNumberForOffset(expected_offset_), block);
    }

    expected_offset_ = NextOffsetFor(expected_offset_);

    if (expected_offset_ == Offset::A)
      NewGroupReceived();
  }
}

void BlockStream::NewGroupReceived() {
  groups_.push_back(current_group_);
  current_group_ = Group();
}

// TODO: this will probably never be > 1
std::vector<Group> BlockStream::PopGroups() {
  std::vector<Group> result = groups_;
  groups_.clear();
  return result;
}

Group BlockStream::FlushCurrentGroup() const {
  return current_group_;
}

}  // namespace redsea
