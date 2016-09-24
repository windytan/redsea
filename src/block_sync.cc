#include "block_sync.h"

namespace redsea {

namespace {

const unsigned kBitmask16 = 0x000FFFF;
const unsigned kBitmask26 = 0x3FFFFFF;
const unsigned kBitmask28 = 0xFFFFFFF;

const unsigned kMaxErrorLength = 5;

const std::vector<uint16_t> offset_words =
  {0x0FC, 0x198, 0x168, 0x350, 0x1B4};
const std::map<uint16_t,eOffset> offset_syndromes =
  {{0x3D8,OFFSET_A},  {0x3D4,OFFSET_B}, {0x25C,OFFSET_C},
   {0x3CC,OFFSET_CI}, {0x258,OFFSET_D}};
const std::vector<uint16_t> block_number_for_offset =
  {0, 1, 2, 2, 3};

// Section B.1.1: '-- calculated by the modulo-two addition of all the rows of
// the -- matrix for which the corresponding coefficient in the -- vector is 1.'
uint32_t matrixMultiply(uint32_t vec, const std::vector<uint32_t>& matrix) {

  uint32_t result = 0;

  for (int k=0; k<(int)matrix.size(); k++)
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
  static const std::map<eOffset,eOffset> next_offset({
      {OFFSET_A,OFFSET_B}, {OFFSET_B,OFFSET_C},
      {OFFSET_C,OFFSET_D}, {OFFSET_CI,OFFSET_D},
      {OFFSET_D,OFFSET_A}
  });
  return next_offset.at(o);
}

// Precompute mapping of syndromes to error vectors
std::map<uint16_t,uint32_t> makeErrorLookupTable() {

  std::map<uint16_t,uint32_t> result;

  for (uint32_t e=1; e < (1<<kMaxErrorLength); e++) {
    for (unsigned shift=0; shift < 26; shift++) {
      uint32_t errvec = ((e << shift) & kBitmask26);

      uint32_t sy = calcSyndrome(errvec);
      result[sy] = errvec;
    }
  }
  return result;
}

} // namespace

BlockStream::BlockStream(eInputType input_type) : bitcount_(0),
  prevbitcount_(0), left_to_read_(0), wideblock_(0), prevsync_(0),
  block_counter_(0), expected_offset_(OFFSET_A),
  received_offset_(OFFSET_INVALID), pi_(0), is_in_sync_(false), group_data_(4),
  has_block_(5), block_has_errors_(50), subcarrier_(), ascii_bits_(),
  error_lookup_(makeErrorLookupTable()), num_blocks_received_(0),
  input_type_(input_type), is_eof_(false) {

}

int BlockStream::getNextBit() {
  int result = 0;
  if (input_type_ == INPUT_MPX) {
    result = subcarrier_.getNextBit();
    is_eof_ = subcarrier_.isEOF();

  } else if (input_type_ == INPUT_ASCIIBITS) {
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

// When a block can't be decoded, save the beginning of the group if possible
void BlockStream::uncorrectable() {
  num_blocks_received_ = 0;

  for (eOffset o : {OFFSET_A, OFFSET_B, OFFSET_C, OFFSET_CI}) {
    if (has_block_[o]) {
      num_blocks_received_ = block_number_for_offset[o] + 1;
    } else {
      break;
    }
  }

  block_has_errors_[block_counter_ % block_has_errors_.size()] = true;

  unsigned num_erroneous_blocks = 0;
  for (bool e : block_has_errors_) {
    if (e)
      num_erroneous_blocks ++;
  }

  // Sync is lost when >45 out of last 50 blocks are erroneous (Section C.1.2)
  if (is_in_sync_ && num_erroneous_blocks > 45) {
    is_in_sync_ = false;
    for (unsigned i=0; i<block_has_errors_.size(); i++)
      block_has_errors_[i] = false;
    pi_ = 0x0000;
  }

  for (eOffset o : {OFFSET_A, OFFSET_B, OFFSET_C, OFFSET_CI, OFFSET_D})
    has_block_[o] = false;

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
      //printf(":sync!\n");
    } else {
      prevbitcount_ = bitcount_;
      prevsync_ = received_offset_;
    }
  }

  return is_in_sync_;

}

Group BlockStream::getNextGroup() {

  num_blocks_received_ = 0;

  while (num_blocks_received_ == 0 && !isEOF()) {

    // Compensate for clock slip corrections
    bitcount_ += 26 - left_to_read_;

    // Read from radio
    for (int i=0; i < (is_in_sync_ ? (int)left_to_read_ : 1); i++,bitcount_++) {
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

    block_counter_ ++;
    uint16_t message = block >> 10;

    if (expected_offset_ == OFFSET_C && received_offset_ == OFFSET_CI)
      expected_offset_ = OFFSET_CI;

    if ( received_offset_ != expected_offset_) {

      // If message is a correct PI, error was probably in check bits
      if (expected_offset_ == OFFSET_A && message == pi_ && pi_ != 0) {
        received_offset_ = OFFSET_A;
        //printf(":offset 0: ignoring error in check bits\n");
      } else if (expected_offset_ == OFFSET_C && message == pi_ && pi_ != 0) {
        received_offset_ = OFFSET_CI;
        //printf(":offset 0: ignoring error in check bits\n");

      // Detect & correct clock slips (Section C.1.2)
      } else if (expected_offset_ == OFFSET_A && pi_ != 0 &&
          ((wideblock_ >> 12) & kBitmask16) == pi_) {
        message = pi_;
        wideblock_ >>= 1;
        received_offset_ = OFFSET_A;
        //printf(":offset 0: clock slip corrected\n");
      } else if (expected_offset_ == OFFSET_A && pi_ != 0 &&
          ((wideblock_ >> 10) & kBitmask16) == pi_) {
        message = pi_;
        wideblock_ = (wideblock_ << 1) + getNextBit();
        received_offset_ = OFFSET_A;
        left_to_read_ = 25;
        //printf(":offset 0: clock slip corrected\n");

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

      group_data_[block_number_for_offset[expected_offset_]] = message;
      has_block_[expected_offset_] = true;

      if (expected_offset_ == OFFSET_A) {
        pi_ = message;
      }

      // Complete group received
      if (has_block_[OFFSET_A] && has_block_[OFFSET_B] &&
         (has_block_[OFFSET_C] || has_block_[OFFSET_CI]) &&
          has_block_[OFFSET_D]) {
        num_blocks_received_ = 4;
      }
    }

    expected_offset_ = nextOffsetFor(expected_offset_);

    // End-of-group reset
    if (expected_offset_ == OFFSET_A) {
      for (eOffset o : {OFFSET_A, OFFSET_B, OFFSET_C, OFFSET_CI, OFFSET_D})
        has_block_[o] = false;
    }

  }

  std::vector<uint16_t> result = group_data_;
  result.resize(num_blocks_received_);

  return Group(result);

}

bool BlockStream::isEOF() const {
  return is_eof_;
}

#ifdef DEBUG
float BlockStream::getT() const {
  return subcarrier_.getT();
}
#endif

} // namespace redsea
