#include "src/block_sync.h"

#include <utility>
#include <vector>

#include "src/util.h"

namespace redsea {

const unsigned kBitmask16 = 0x000FFFF;
const unsigned kBitmask26 = 0x3FFFFFF;
const unsigned kBitmask28 = 0xFFFFFFF;

const std::vector<uint16_t> offset_words =
    {0x0FC, 0x198, 0x168, 0x350, 0x1B4};
const std::vector<eBlockNumber> block_number_for_offset =
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
uint32_t CalcSyndrome(uint32_t vec) {
  static const std::vector<uint32_t> parity_check_matrix({
      0x200, 0x100, 0x080, 0x040, 0x020, 0x010, 0x008, 0x004,
      0x002, 0x001, 0x2dc, 0x16e, 0x0b7, 0x287, 0x39f, 0x313,
      0x355, 0x376, 0x1bb, 0x201, 0x3dc,
      0x1ee, 0x0f7, 0x2a7, 0x38f, 0x31b
  });

  return MatrixMultiply(vec, parity_check_matrix);
}

eOffset nextOffsetFor(eOffset this_offset) {
  static const std::map<eOffset, eOffset> next_offset({
      {OFFSET_A, OFFSET_B}, {OFFSET_B, OFFSET_C},
      {OFFSET_C, OFFSET_D}, {OFFSET_C_PRIME, OFFSET_D},
      {OFFSET_D, OFFSET_A}, {OFFSET_INVALID, OFFSET_A}
  });
  return next_offset.at(this_offset);
}

// Precompute mapping of syndromes to error vectors

std::map<std::pair<uint16_t, eOffset>, uint32_t> MakeErrorLookupTable() {
  std::map<std::pair<uint16_t, eOffset>, uint32_t> result;

  for (eOffset o : {OFFSET_A, OFFSET_B, OFFSET_C, OFFSET_CI, OFFSET_D}) {
    // "...the error-correction system should be enabled, but should be
    // restricted by attempting to correct bursts of errors spanning one or two
    // bits."
    // Kopitz & Marks 1999: "RDS: The Radio Data System", p. 224
    for (uint32_t error_bits : {0x1, 0x3}) {
      for (int shift=0; shift < 26; shift++) {
        uint32_t error_vector = ((error_bits << shift) & kBitmask26);

        uint32_t syndrome = CalcSyndrome(error_vector ^ offset_words[offset]);
        result.insert({{syndrome, offset}, error_vector});
      }
    }
  }
  return result;
}

eOffset OffsetForSyndrome(uint16_t syndrome) {
  static const std::map<uint16_t, eOffset> offset_syndromes =
    {{0x3D8, OFFSET_A},  {0x3D4, OFFSET_B}, {0x25C, OFFSET_C},
     {0x3CC, OFFSET_C_PRIME}, {0x258, OFFSET_D}};

  if (offset_syndromes.count(syndrome) > 0)
    return offset_syndromes.at(syndrome);
  else
    return OFFSET_INVALID;
}

std::map<std::pair<uint16_t, eOffset>, uint32_t> kErrorLookup =
    MakeErrorLookupTable();

// Section B.2.2
uint32_t CorrectBurstErrors(uint32_t block, eOffset offset) {
  uint16_t syndrome = CalcSyndrome(block);
  uint32_t corrected_block = block;

  if (kErrorLookup.count({syndrome, offset}) > 0) {
    uint32_t err = kErrorLookup.at({syndrome, offset});
    corrected_block ^= err;
  }

  return corrected_block;
}

BlockStream::BlockStream(Options options) : bitcount_(0),
  prevbitcount_(0), left_to_read_(0), padded_block_(0), prevsync_(0),
  block_counter_(0), expected_offset_(OFFSET_A),
  received_offset_(OFFSET_INVALID), pi_(0), is_in_sync_(false),
  block_has_errors_(50),
#ifdef HAVE_LIQUID
  subcarrier_(options.feed_thru),
#endif
  ascii_bits_(options.feed_thru),
  input_type_(options.input_type), is_eof_(false) {
}

int BlockStream::NextBit() {
  int result = 0;
#ifdef HAVE_LIQUID
  if (input_type_ == INPUT_MPX) {
    result = subcarrier_.NextBit();
    is_eof_ = subcarrier_.eof();
  }
#endif
  if (input_type_ == INPUT_ASCIIBITS) {
    result = ascii_bits_.NextBit();
    is_eof_ = ascii_bits_.eof();
  }

  return result;
}

// A block can't be decoded
void BlockStream::Uncorrectable() {
  block_has_errors_[block_counter_ % block_has_errors_.size()] = true;

  unsigned num_erroneous_blocks = 0;
  for (bool e : block_has_errors_) {
    if (e)
      num_erroneous_blocks++;
  }

  // Sync is lost when >45 out of last 50 blocks are erroneous (Section C.1.2)
  if (is_in_sync_ && num_erroneous_blocks > 45) {
    is_in_sync_ = false;
    for (size_t i=0; i < block_has_errors_.size(); i++)
      block_has_errors_[i] = false;
    pi_ = 0x0000;
  }
}

bool BlockStream::AcquireSync() {
  if (is_in_sync_)
    return true;

  // Try to find the repeating offset sequence
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
    for (int i=0; i < (is_in_sync_ ? left_to_read_ : 1); i++, bitcount_++) {
      padded_block_ = (padded_block_ << 1) + NextBit();
    }

    left_to_read_ = 26;
    padded_block_ &= kBitmask28;

    uint32_t block = (padded_block_ >> 1) & kBitmask26;
    uint16_t message = block >> 10;

    received_offset_ = OffsetForSyndrome(CalcSyndrome(block));

    if (!AcquireSync())
      continue;

    block_counter_++;

    if (expected_offset_ == OFFSET_C && received_offset_ == OFFSET_C_PRIME)
      expected_offset_ = OFFSET_C_PRIME;

    block_has_errors_[block_counter_ % block_has_errors_.size()] = false;

    if (received_offset_ != expected_offset_) {
      block_has_errors_[block_counter_ % block_has_errors_.size()] = true;

      // Detect & correct clock slips (Section C.1.2)
      if (expected_offset_ == OFFSET_A && pi_ != 0 &&
          ((padded_block_ >> 12) & kBitmask16) == pi_) {
        message = pi_;
        padded_block_ >>= 1;
        received_offset_ = OFFSET_A;
      } else if (expected_offset_ == OFFSET_A && pi_ != 0 &&
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

      /*} else {
        block = correctBurstErrors(block);
        if (calcSyndrome(block) == 0x000) {
          message = block >> 10;
          received_offset_ = expected_offset_;
        }*/
      //}

      // Still no valid syndrome
      if (received_offset_ != expected_offset_)
        Uncorrectable();
    }

    // Error-free block received

    if (received_offset_ == expected_offset_) {
      if (expected_offset_ == OFFSET_C_PRIME)
        group.set_c_prime(message);
      else
        group.set(g_block_number_for_offset[expected_offset_], message);

      if (group.has_pi())
        pi_ = group.pi();
    }

    expected_offset_ = nextOffsetFor(expected_offset_);

    if (expected_offset_ == OFFSET_A) {
      break;
    }
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
