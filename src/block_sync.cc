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

#include <array>
#include <cassert>
#include <map>
#include <utility>

namespace redsea {

// If the block error rate (0-100) exceeds this value over a longer period, assume that it's
// because we lost synchronization. A lower value will make redsea give up in noisy conditions.
constexpr int kMaxTolerableBLER = 85;

constexpr int kMaxErrorsToleratedOver50Blocks{kMaxTolerableBLER / 2};

constexpr int kBlockLength       = 26;
constexpr unsigned kBlockBitmask = (1 << kBlockLength) - 1;
constexpr int kCheckwordLength   = 10;

// Each offset word is associated with one block number
eBlockNumber getBlockNumberForOffset(Offset offset) {
  switch (offset) {
    case Offset::A:       return BLOCK1;
    case Offset::B:       return BLOCK2;
    case Offset::C:       return BLOCK3;
    case Offset::Cprime:  return BLOCK3;
    case Offset::D:       return BLOCK4;

    case Offset::invalid: return BLOCK1;
  }
  return BLOCK1;
}

// Return the next offset word in sequence
Offset getNextOffsetFor(Offset this_offset) {
  switch (this_offset) {
    case Offset::A:       return Offset::B;
    case Offset::B:       return Offset::C;
    case Offset::C:       return Offset::D;
    case Offset::Cprime:  return Offset::D;
    case Offset::D:       return Offset::A;

    case Offset::invalid: return Offset::A;
  }
  return Offset::A;
}

// IEC 62106:2015 section B.3.1 Table B.1
Offset getOffsetForSyndrome(uint32_t syndrome) {
  switch (syndrome) {
    case 0b1111011000: return Offset::A;
    case 0b1111010100: return Offset::B;
    case 0b1001011100: return Offset::C;
    case 0b1111001100: return Offset::Cprime;
    case 0b1001011000: return Offset::D;

    default:           return Offset::invalid;
  }
}

uint32_t calculateSyndrome(uint32_t vec) {
  // clang-format off
  constexpr std::array<uint32_t, 26> parity_check_matrix{
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
  };
  // clang-format on

  // EN 50067:1998, section B.1.1: Matrix multiplication is '-- calculated by
  // the modulo-two addition of all the rows of the -- matrix for which the
  // corresponding coefficient in the -- vector is 1.'

  uint32_t result{};

  for (size_t k = 0; k < parity_check_matrix.size(); k++)
    result ^= parity_check_matrix[parity_check_matrix.size() - 1 - k] * ((vec >> k) & 0b1);

  return result;
}

// Precompute mapping of syndromes to error vectors
// IEC 62106:2015 section B.3.1
std::map<std::pair<uint32_t, Offset>, uint32_t> makeErrorLookupTable() {
  std::map<std::pair<uint32_t, Offset>, uint32_t> lookup_table;

  // Table B.1
  // clang-format off
  constexpr std::array<std::pair<Offset, uint32_t>, 5> offset_words{{
      { Offset::A,      0b0011111100 },
      { Offset::B,      0b0110011000 },
      { Offset::C,      0b0101101000 },
      { Offset::Cprime, 0b1101010000 },
      { Offset::D,      0b0110110100 }
  }};
  // clang-format on

  for (const auto& offset : offset_words) {
    // Kopitz & Marks 1999: "RDS: The Radio Data System", p. 224:
    // "...the error-correction system should be enabled, but should be
    // restricted by attempting to correct bursts of errors spanning one or two
    // bits."
    for (const uint32_t error_bits : {0b1u, 0b11u}) {
      for (uint32_t shift = 0; shift < kBlockLength; shift++) {
        const uint32_t error_vector = ((error_bits << shift) & kBlockBitmask);

        const uint32_t syndrome = calculateSyndrome(error_vector ^ offset.second);
        lookup_table.insert({
            {syndrome, offset.first},
            error_vector
        });
      }
    }
  }
  return lookup_table;
}

// EN 50067:1998, section B.2.2
ErrorCorrectionResult correctBurstErrors(Block block, Offset expected_offset) {
  static const auto error_lookup_table = makeErrorLookupTable();

  ErrorCorrectionResult result;

  const uint32_t syndrome = calculateSyndrome(block.raw);
  result.corrected_bits   = block.raw;

  const auto search = error_lookup_table.find({syndrome, expected_offset});
  if (search != error_lookup_table.end()) {
    const uint32_t err = search->second;
    result.corrected_bits ^= err;
    result.succeeded = true;
  }

  return result;
}

// Could this pulse realistically follow other?
bool SyncPulse::couldFollow(const SyncPulse& other) const {
  // Overflows after 41 days of continuous data. This may cause us to discard 1 valid sync pulse
  // if at that exact moment we're out of sync.
  const uint32_t sync_distance = bit_position - other.bit_position;

  return sync_distance % kBlockLength == 0 && sync_distance / kBlockLength <= 6 &&
         offset != Offset::invalid && other.offset != Offset::invalid &&
         (getBlockNumberForOffset(other.offset) + sync_distance / kBlockLength) % 4 ==
             getBlockNumberForOffset(offset);
}

// Push a detected sync pulse to the buffer for determination of validity.
// @param offset The calculated cyclic offset
// @param bitcount Bit position where the detection happened
void SyncPulseBuffer::push(Offset offset, int bitcount) {
  assert(pulses_.size() >= 1);
  for (size_t i = 0; i < pulses_.size() - 1; i++) {
    pulses_[i] = pulses_[i + 1];
  }

  SyncPulse new_pulse;
  new_pulse.offset       = offset;
  new_pulse.bit_position = bitcount;

  pulses_.back() = new_pulse;
}

// Search for three sync pulses in the correct cyclic rhythm
bool SyncPulseBuffer::isSequenceFound() const {
  const auto& third = pulses_.back();

  assert(pulses_.size() >= 2);
  for (size_t i_first = 0; i_first < pulses_.size() - 2; i_first++) {
    for (size_t i_second = i_first + 1; i_second < pulses_.size() - 1; i_second++) {
      if (third.couldFollow(pulses_[i_second]) && pulses_[i_second].couldFollow(pulses_[i_first]))
        return true;
    }
  }
  return false;
}

BlockStream::BlockStream(const Options& options) : options_(options) {}

// Try to find a cyclic pattern in the offset words.
void BlockStream::acquireSync(Block block) {
  if (is_in_sync_)
    return;

  num_bits_since_sync_lost_++;

  if (block.offset != Offset::invalid) {
    sync_buffer_.push(block.offset, bitcount_);

    if (sync_buffer_.isSequenceFound()) {
      is_in_sync_               = true;
      expected_offset_          = block.offset;
      current_group_            = Group();
      num_bits_since_sync_lost_ = 0;
    }
  }
}

void BlockStream::pushBit(bool bit) {
  input_register_ = (input_register_ << 1) + bit;
  num_bits_until_next_block_--;
  bitcount_++;

  if (num_bits_until_next_block_ == 0) {
    findBlockInInputRegister();

    num_bits_until_next_block_ = is_in_sync_ ? kBlockLength : 1;
  }
}

// Search the input register for block data + offset. If found, add it to the group.
void BlockStream::findBlockInInputRegister() {
  Block block;
  block.raw    = input_register_ & kBlockBitmask;
  block.offset = getOffsetForSyndrome(calculateSyndrome(block.raw));

  acquireSync(block);

  if (is_in_sync_) {
    if (expected_offset_ == Offset::C && block.offset == Offset::Cprime)
      expected_offset_ = Offset::Cprime;

    block.had_errors = (block.offset != expected_offset_);
    block_error_sum50_.push(block.had_errors);
    // EN 50067:1998, section C.1.2:
    // Sync is dropped when too many of the previous syndromes failed
    if (block_error_sum50_.getSum() > kMaxErrorsToleratedOver50Blocks) {
      is_in_sync_ = false;
      block_error_sum50_.clear();
      return;
    }

    block.data = static_cast<uint16_t>(block.raw >> kCheckwordLength);

    if (block.had_errors && options_.use_fec) {
      const auto correction = correctBurstErrors(block, expected_offset_);
      if (correction.succeeded) {
        block.data   = static_cast<uint16_t>(correction.corrected_bits >> kCheckwordLength);
        block.offset = expected_offset_;
      }
    }

    // Error-free block received or errors successfully corrected
    if (block.offset == expected_offset_) {
      block.is_received = true;
      current_group_.setBlock(getBlockNumberForOffset(expected_offset_), block);
    }

    expected_offset_ = getNextOffsetFor(expected_offset_);

    if (expected_offset_ == Offset::A)
      handleNewlyReceivedGroup();
  }
}

// A whole group of four blocks was received.
void BlockStream::handleNewlyReceivedGroup() {
  ready_group_     = current_group_;
  has_group_ready_ = true;
  current_group_   = Group();
}

bool BlockStream::hasGroupReady() const {
  return has_group_ready_;
}

Group BlockStream::popGroup() {
  has_group_ready_ = false;
  return ready_group_;
}

Group BlockStream::flushCurrentGroup() const {
  return current_group_;
}

uint32_t BlockStream::getNumBitsSinceSyncLost() const {
  return num_bits_since_sync_lost_;
}

}  // namespace redsea
