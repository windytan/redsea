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

#include <utility>
#include <vector>

namespace redsea {

const unsigned kBitmask16 = 0x000FFFF;
const unsigned kBitmask26 = 0x3FFFFFF;
const unsigned kBitmask28 = 0xFFFFFFF;

const eBlockNumber g_block_number_for_offset[5] =
    {BLOCK1, BLOCK2, BLOCK3, BLOCK3, BLOCK4};

// Section B.1.1: '-- calculated by the modulo-two addition of all the rows of
// the -- matrix for which the corresponding coefficient in the -- vector is 1.'
uint32_t MatrixMultiply(uint32_t vec, const std::vector<uint32_t>& matrix) {
  uint32_t result = 0;

  for (size_t k=0; k < matrix.size(); k++)
    if ((vec >> k) & 0x01)
      result ^= matrix[matrix.size() - 1 - k];

  return result;
}

// Section B.2.1: 'The calculation of the syndromes -- can easily be done by
// multiplying each word with the parity matrix H.'
uint32_t CalculateSyndrome(uint32_t vec) {
  static const std::vector<uint32_t> parity_check_matrix({
      0x200, 0x100, 0x080, 0x040, 0x020, 0x010, 0x008, 0x004,
      0x002, 0x001, 0x2dc, 0x16e, 0x0b7, 0x287, 0x39f, 0x313,
      0x355, 0x376, 0x1bb, 0x201, 0x3dc,
      0x1ee, 0x0f7, 0x2a7, 0x38f, 0x31b
  });

  return MatrixMultiply(vec, parity_check_matrix);
}

eOffset NextOffsetFor(eOffset this_offset) {
  static const std::map<eOffset, eOffset> next_offset({
      {OFFSET_A, OFFSET_B}, {OFFSET_B, OFFSET_C},
      {OFFSET_C, OFFSET_D}, {OFFSET_C_PRIME, OFFSET_D},
      {OFFSET_D, OFFSET_A}, {OFFSET_INVALID, OFFSET_A}
  });
  return next_offset.at(this_offset);
}

// Precompute mapping of syndromes to error vectors

std::map<std::pair<uint16_t, eOffset>, uint32_t> MakeErrorLookupTable() {
  std::map<std::pair<uint16_t, eOffset>, uint32_t> lookup_table;

  const uint16_t offset_words[] =
      {0x0FC, 0x198, 0x168, 0x350, 0x1B4};

  for (eOffset offset : {OFFSET_A, OFFSET_B, OFFSET_C,
                         OFFSET_C_PRIME, OFFSET_D}) {
    // "...the error-correction system should be enabled, but should be
    // restricted by attempting to correct bursts of errors spanning one or two
    // bits."
    // Kopitz & Marks 1999: "RDS: The Radio Data System", p. 224
    for (uint32_t error_bits : {0x1, 0x3}) {
      for (int shift=0; shift < 26; shift++) {
        uint32_t error_vector = ((error_bits << shift) & kBitmask26);

        uint32_t syndrome = CalculateSyndrome(error_vector ^ offset_words[offset]);
        lookup_table.insert({{syndrome, offset}, error_vector});
      }
    }
  }
  return lookup_table;
}

eOffset OffsetForSyndrome(uint16_t syndrome) {
  static const std::map<uint16_t, eOffset> offset_syndromes =
    {{0x3D8, OFFSET_A},
     {0x3D4, OFFSET_B},
     {0x25C, OFFSET_C},
     {0x3CC, OFFSET_C_PRIME},
     {0x258, OFFSET_D}};

  if (offset_syndromes.count(syndrome) > 0)
    return offset_syndromes.at(syndrome);
  else
    return OFFSET_INVALID;
}

const std::map<std::pair<uint16_t, eOffset>, uint32_t> kErrorLookup =
    MakeErrorLookupTable();

// Section B.2.2
uint32_t CorrectBurstErrors(uint32_t block, eOffset offset) {
  uint16_t syndrome = CalculateSyndrome(block);
  uint32_t corrected_block = block;

  if (kErrorLookup.count({syndrome, offset}) > 0) {
    uint32_t err = kErrorLookup.at({syndrome, offset});
    corrected_block ^= err;
  }

  return corrected_block;
}

RunningSum::RunningSum(int length) :
  history_(length), pointer_(0) {
}

void RunningSum::push(int number) {
  history_[pointer_] = number;
  pointer_ = (pointer_ + 1) % history_.size();
}

int RunningSum::sum() const {
  int result = 0;
  for (int number : history_)
    result += number;

  return result;
}

void RunningSum::clear() {
  for (size_t i = 0; i < history_.size(); i++)
    history_[i] = 0;
}

BlockStream::BlockStream(const Options& options) : bitcount_(0),
  prevbitcount_(0), left_to_read_(0), padded_block_(0), prevsync_(0),
  expected_offset_(OFFSET_A),
  received_offset_(OFFSET_INVALID), pi_(0x0000), is_in_sync_(false),
  block_error_sum_(50),
#ifdef HAVE_LIQUID
  subcarrier_(options),
#endif
  options_(options),
  ascii_bit_reader_(options),
  input_type_(options.input_type), is_eof_(false), bler_average_(12) {
}

int BlockStream::NextBit() {
  int bit = 0;
#ifdef HAVE_LIQUID
  if (input_type_ == INPUT_MPX_STDIN || input_type_ == INPUT_MPX_SNDFILE) {
    bit = subcarrier_.NextBit();
    is_eof_ = subcarrier_.eof();
  }
#endif
  if (input_type_ == INPUT_ASCIIBITS) {
    bit = ascii_bit_reader_.NextBit();
    is_eof_ = ascii_bit_reader_.eof();
  }

  return bit;
}

// A block can't be decoded
void BlockStream::Uncorrectable() {
  // Sync is lost when >45 out of last 50 blocks are erroneous (Section C.1.2)
  if (is_in_sync_ && block_error_sum_.sum() > 45) {
    is_in_sync_ = false;
    block_error_sum_.clear();
    pi_ = 0x0000;
  }
}

bool BlockStream::AcquireSync() {
  if (is_in_sync_)
    return true;

  // Try to find a repeating offset sequence
  if (received_offset_ != OFFSET_INVALID) {
    int dist = bitcount_ - prevbitcount_;

    if (dist % 26 == 0 && dist <= 156 &&
        (g_block_number_for_offset[prevsync_] + dist/26) % 4 ==
        g_block_number_for_offset[received_offset_]) {
      is_in_sync_ = true;
      expected_offset_ = received_offset_;
    } else {
      prevbitcount_ = bitcount_;
      prevsync_ = received_offset_;
    }
  }

  return is_in_sync_;
}

Group BlockStream::NextGroup() {
  Group group;

  while (!eof()) {
    // Compensate for clock slip corrections
    bitcount_ += 26 - left_to_read_;

    // Read from radio
    for (int i=0; i < (is_in_sync_ ? left_to_read_ : 1); i++, bitcount_++)
      padded_block_ = (padded_block_ << 1) + NextBit();

    left_to_read_ = 26;
    padded_block_ &= kBitmask28;

    uint32_t block = (padded_block_ >> 1) & kBitmask26;
    uint16_t message = block >> 10;

    received_offset_ = OffsetForSyndrome(CalculateSyndrome(block));

    if (!AcquireSync())
      continue;

    if (expected_offset_ == OFFSET_C && received_offset_ == OFFSET_C_PRIME)
      expected_offset_ = OFFSET_C_PRIME;

    bool block_had_errors = (received_offset_ != expected_offset_);
    block_error_sum_.push(block_had_errors);

    if (block_had_errors) {
      // Detect & correct clock slips (Section C.1.2)
      if (expected_offset_ == OFFSET_A && pi_ != 0x0000 &&
          ((padded_block_ >> 12) & kBitmask16) == pi_) {
        message = pi_;
        padded_block_ >>= 1;
        received_offset_ = OFFSET_A;
      } else if (expected_offset_ == OFFSET_A && pi_ != 0x0000 &&
          ((padded_block_ >> 10) & kBitmask16) == pi_) {
        message = pi_;
        padded_block_ = (padded_block_ << 1) + NextBit();
        received_offset_ = OFFSET_A;
        left_to_read_ = 25;
      } else {
        uint32_t corrected_block = CorrectBurstErrors(block, expected_offset_);
        if (corrected_block != block) {
          message = corrected_block >> 10;
          received_offset_ = expected_offset_;
        }
      }

      // Still no valid syndrome
      if (received_offset_ != expected_offset_)
        Uncorrectable();
    }

    // Error-free block received or errors successfully corrected

    if (received_offset_ == expected_offset_) {
      if (expected_offset_ == OFFSET_C_PRIME)
        group.set_c_prime(message, block_had_errors);
      else
        group.set(g_block_number_for_offset[expected_offset_], message,
                  block_had_errors);

      if (group.has_pi())
        pi_ = group.pi();
    }

    expected_offset_ = NextOffsetFor(expected_offset_);

    if (expected_offset_ == OFFSET_A)
      break;
  }

  return group;
}

bool BlockStream::eof() const {
  return is_eof_;
}

#ifdef DEBUG
float BlockStream::t() const {
  return subcarrier_.t();
}
#endif

}  // namespace redsea
