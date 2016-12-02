#include "src/block_sync.h"

#include <vector>

#include "src/util.h"

namespace redsea {

namespace {

const unsigned kBitmask16 = 0x000FFFF;
const unsigned kBitmask26 = 0x3FFFFFF;
const unsigned kBitmask28 = 0xFFFFFFF;

// "...the error-correction system should be enabled, but should be restricted
// by attempting to correct bursts of errors spanning one or two bits."
// Kopitz & Marks 1999: "RDS: The Radio Data System", p. 224
const unsigned kMaxErrorLength = 2;

const std::vector<uint16_t> offset_words =
    {0x0FC, 0x198, 0x168, 0x350, 0x1B4};
const std::map<uint16_t, eOffset> offset_syndromes =
    {{0x3D8, OFFSET_A},  {0x3D4, OFFSET_B}, {0x25C, OFFSET_C},
     {0x3CC, OFFSET_CI}, {0x258, OFFSET_D}};
const std::vector<eBlockNumber> block_number_for_offset =
    {BLOCK1, BLOCK2, BLOCK3, BLOCK3, BLOCK4};

// Section B.1.1: '-- calculated by the modulo-two addition of all the rows of
// the -- matrix for which the corresponding coefficient in the -- vector is 1.'
uint32_t matrixMultiply(uint32_t vec, const std::vector<uint32_t>& matrix) {
  uint32_t result = 0;

  for (size_t k=0; k < matrix.size(); k++)
    if ((vec >> k) & 0x01)
      result ^= matrix[matrix.size() - 1 - k];

  return result;
}

// Section B.2.1: 'The calculation of the syndromes -- can easily be done by
// multiplying each word with the parity matrix H.'
uint32_t calcSyndrome(uint32_t vec) {
  static const std::vector<uint32_t> parity_check_matrix({
      0x200, 0x100, 0x080, 0x040, 0x020, 0x010, 0x008, 0x004,
      0x002, 0x001, 0x2dc, 0x16e, 0x0b7, 0x287, 0x39f, 0x313,
      0x355, 0x376, 0x1bb, 0x201, 0x3dc,
      0x1ee, 0x0f7, 0x2a7, 0x38f, 0x31b
  });

  return matrixMultiply(vec, parity_check_matrix);
}

eOffset nextOffsetFor(eOffset o) {
  static const std::map<eOffset, eOffset> next_offset({
      {OFFSET_A, OFFSET_B}, {OFFSET_B, OFFSET_C},
      {OFFSET_C, OFFSET_D}, {OFFSET_CI, OFFSET_D},
      {OFFSET_D, OFFSET_A}
  });
  return next_offset.at(o);
}

// Precompute mapping of syndromes to error vectors
std::map<uint16_t, uint32_t> makeErrorLookupTable() {
  std::map<uint16_t, uint32_t> result;

  for (uint32_t e=1; e < (1 << kMaxErrorLength); e++) {
    for (unsigned shift=0; shift < 26; shift++) {
      uint32_t errvec = ((e << shift) & kBitmask26);

      uint32_t sy = calcSyndrome(errvec);
      result[sy] = errvec;
    }
  }
  return result;
}

}  // namespace

BlockStream::BlockStream(Options options) : bitcount_(0),
  prevbitcount_(0), left_to_read_(0), wideblock_(0), prevsync_(0),
  block_counter_(0), expected_offset_(OFFSET_A),
  received_offset_(OFFSET_INVALID), pi_(0), is_in_sync_(false),
  block_has_errors_(50),
#ifdef HAVE_LIQUID
  subcarrier_(options.feed_thru),
#endif
  ascii_bits_(options.feed_thru),
  error_lookup_(makeErrorLookupTable()),
  input_type_(options.input_type), is_eof_(false) {
}

int BlockStream::getNextBit() {
  int result = 0;
#ifdef HAVE_LIQUID
  if (input_type_ == INPUT_MPX) {
    result = subcarrier_.getNextBit();
    is_eof_ = subcarrier_.isEOF();
  }
#endif
  if (input_type_ == INPUT_ASCIIBITS) {
    result = ascii_bits_.getNextBit();
    is_eof_ = ascii_bits_.isEOF();
  }

  return result;
}

// Section B.2.2
uint32_t BlockStream::correctBurstErrors(uint32_t block) const {
  uint16_t synd_reg =
    calcSyndrome(block ^ offset_words[expected_offset_]);

  uint32_t corrected_block = block;

  if (error_lookup_.find(synd_reg) != error_lookup_.end()) {
    corrected_block = (block ^ offset_words[expected_offset_])
      ^ (error_lookup_.at(synd_reg));
  }

  return corrected_block;
}

// A block can't be decoded
void BlockStream::uncorrectable() {
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

bool BlockStream::acquireSync() {
  if (is_in_sync_)
    return true;

  // Try to find the repeating offset sequence
  if (received_offset_ != OFFSET_INVALID) {
    int dist = bitcount_ - prevbitcount_;

    if (dist % 26 == 0 && dist <= 156 &&
        (block_number_for_offset[prevsync_] + dist/26) % 4 ==
        block_number_for_offset[received_offset_]) {
      is_in_sync_ = true;
      expected_offset_ = received_offset_;
    } else {
      prevbitcount_ = bitcount_;
      prevsync_ = received_offset_;
    }
  }

  return is_in_sync_;
}

Group BlockStream::getNextGroup() {
  Group group;

  while (!isEOF()) {
    // Compensate for clock slip corrections
    bitcount_ += 26 - left_to_read_;

    // Read from radio
    for (int i=0; i < (is_in_sync_ ? left_to_read_ : 1); i++, bitcount_++) {
      wideblock_ = (wideblock_ << 1) + getNextBit();
    }

    left_to_read_ = 26;
    wideblock_ &= kBitmask28;

    uint32_t block = (wideblock_ >> 1) & kBitmask26;

    uint16_t syndrome = calcSyndrome(block);
    received_offset_ = (offset_syndromes.count(syndrome) > 0 ?
        offset_syndromes.at(syndrome) : OFFSET_INVALID);

    if (!acquireSync())
      continue;

    block_counter_++;
    uint16_t message = block >> 10;
    bool was_valid_word = true;

    if (expected_offset_ == OFFSET_C && received_offset_ == OFFSET_CI)
      expected_offset_ = OFFSET_CI;

    block_has_errors_[block_counter_ % block_has_errors_.size()] = false;

    if (received_offset_ != expected_offset_) {
      block_has_errors_[block_counter_ % block_has_errors_.size()] = true;

      was_valid_word = false;

      // Detect & correct clock slips (Section C.1.2)
      if (expected_offset_ == OFFSET_A && pi_ != 0 &&
          ((wideblock_ >> 12) & kBitmask16) == pi_) {
        message = pi_;
        wideblock_ >>= 1;
        received_offset_ = OFFSET_A;
      } else if (expected_offset_ == OFFSET_A && pi_ != 0 &&
          ((wideblock_ >> 10) & kBitmask16) == pi_) {
        message = pi_;
        wideblock_ = (wideblock_ << 1) + getNextBit();
        received_offset_ = OFFSET_A;
        left_to_read_ = 25;

      } else {
        block = correctBurstErrors(block);
        if (calcSyndrome(block) == 0x000) {
          message = block >> 10;
          received_offset_ = expected_offset_;
        }
      }

      // Still no valid syndrome
      if (received_offset_ != expected_offset_)
        uncorrectable();
    }

    // Error-free block received

    if (received_offset_ == expected_offset_) {
      if (expected_offset_ == OFFSET_CI)
        group.setCI(message);
      else
        group.set(block_number_for_offset[expected_offset_], message);

      if (expected_offset_ == OFFSET_A || expected_offset_ == OFFSET_CI) {
        if (was_valid_word)
          pi_ = message;
      }
    }

    expected_offset_ = nextOffsetFor(expected_offset_);

    if (expected_offset_ == OFFSET_A) {
      break;
    }
  }

  return group;
}

bool BlockStream::isEOF() const {
  return is_eof_;
}

#ifdef DEBUG
float BlockStream::getT() const {
  return subcarrier_.getT();
}
#endif

}  // namespace redsea
