#include "block_sync.h"

namespace redsea {

namespace {

const unsigned kBitmask16 = 0x000FFFF;
const unsigned kBitmask26 = 0x3FFFFFF;
const unsigned kBitmask28 = 0xFFFFFFF;

const unsigned kMaxErrorLength = 3;

const std::vector<uint16_t> offset_word = {0x0FC, 0x198, 0x168, 0x350, 0x1B4};
const std::vector<uint16_t> block_for_offset = {0, 1, 2, 2, 3};

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

// Section B.1.1: 'The check bits of the code vector are thus readily
// calculated by the modulo-two addition of all the rows of the generator
// matrix for which the corresponding coefficient in the message vector is 1.'
uint32_t calcCheckBits(uint32_t data_word) {

  static const std::vector<uint32_t> generator_matrix({
      0x077, 0x2e7, 0x3af, 0x30b, 0x359, 0x370, 0x1b8, 0x0dc,
      0x06e, 0x037, 0x2c7, 0x3bf, 0x303, 0x35d, 0x372, 0x1b9
  });

  return matrixMultiply(data_word, generator_matrix);
}

eOffset nextOffsetFor(eOffset o) {
  static const std::map<eOffset,eOffset> next_offset({
      {A,B}, {B,C}, {C,D}, {CI,D}, {D,A}
  });
  return next_offset.at(o);
}

std::map<uint16_t,uint16_t> makeErrorLookupTable() {

  std::map<uint16_t,uint16_t> result;

  for (uint32_t e=1; e < (1<<kMaxErrorLength); e++) {
    for (unsigned shift=0; shift < 16; shift++) {
      uint32_t errvec = ((e << shift) & kBitmask16) << 10;

      uint32_t m = calcCheckBits(0x00);
      uint32_t sy = calcSyndrome(((1<<10) + m) ^ errvec);
      result[sy] = errvec >> 10;
    }
  }
  return result;
}

} // namespace

BlockStream::BlockStream(int input_type) : bitcount_(0), prevbitcount_(0),
  left_to_read_(0), wideblock_(0), prevsync_(0), block_counter_(0),
  expected_offset_(A), pi_(0), has_sync_for_(5), is_in_sync_(false),
  group_data_(4), has_block_(5), block_has_errors_(50), subcarrier_(),
  ascii_bits_(), has_new_group_(false), error_lookup_(), data_length_(0),
  input_type_(input_type), is_eof_(false) {

  error_lookup_ = makeErrorLookupTable();

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

uint32_t BlockStream::correctBurstErrors(uint32_t block) const {

  uint16_t synd_reg =
    calcSyndrome(block ^ offset_word[expected_offset_]);

  uint32_t corrected_block = block;

  if (error_lookup_.find(synd_reg) != error_lookup_.end()) {
    corrected_block = (block ^ offset_word[expected_offset_])
      ^ (error_lookup_.at(synd_reg) << 10);
  }

  return corrected_block;

}

void BlockStream::uncorrectable() {
  //printf(":offset %d: not received\n",expected_offset_);
  data_length_ = 0;

  if (has_block_[A]) {
    has_new_group_ = true;
    data_length_ = 1;

    if (has_block_[B]) {
      data_length_ = 2;

      if (has_block_[C] || has_block_[CI]) {
        data_length_ = 3;
      }
    }
  }

  block_has_errors_[block_counter_ % block_has_errors_.size()] = true;

  unsigned erroneous_blocks = 0;
  for (bool e : block_has_errors_) {
    if (e)
      erroneous_blocks ++;
  }

  // Sync is lost when >45 out of last 50 blocks are erroneous (Section C.1.2)
  if (is_in_sync_ && erroneous_blocks > 45) {
    is_in_sync_ = false;
    for (unsigned i=0; i<block_has_errors_.size(); i++)
      block_has_errors_[i] = false;
    pi_ = 0x0000;
    //printf(":too many errors, sync lost\n");
  }

  for (eOffset o : {A, B, C, CI, D})
    has_block_[o] = false;

}

std::vector<uint16_t> BlockStream::getNextGroup() {

  has_new_group_ = false;
  data_length_ = 0;

  while (!(has_new_group_ || isEOF())) {

    // Compensate for clock slip corrections
    bitcount_ += 26 - left_to_read_;

    // Read from radio
    for (int i=0; i < (is_in_sync_ ? (int)left_to_read_ : 1); i++,bitcount_++) {
      wideblock_ = (wideblock_ << 1) + getNextBit();
    }

    left_to_read_ = 26;
    wideblock_ &= kBitmask28;

    uint32_t block = (wideblock_ >> 1) & kBitmask26;

    // Find the offsets for which the calcSyndrome is zero
    bool has_sync_for_any = false;
    for (eOffset o : {A, B, C, CI, D}) {
      has_sync_for_[o] = (calcSyndrome(block ^ offset_word[o]) == 0x000);
      has_sync_for_any |= has_sync_for_[o];
    }

    // Acquire sync

    if (!is_in_sync_) {
      if (has_sync_for_any) {
        for (eOffset o : {A, B, C, CI, D}) {
          if (has_sync_for_[o]) {
            int dist = bitcount_ - prevbitcount_;

            if (dist % 26 == 0 && dist <= 156 &&
                (block_for_offset[prevsync_] + dist/26) % 4 ==
                block_for_offset[o]) {
              is_in_sync_ = true;
              expected_offset_ = o;
              //printf(":sync!\n");
            } else {
              prevbitcount_ = bitcount_;
              prevsync_ = o;
            }
          }
        }
      }
    }

    // Synchronous decoding

    if (is_in_sync_) {

      block_counter_ ++;
      uint16_t message = block >> 10;

      if (expected_offset_ == C && !has_sync_for_[C] && has_sync_for_[CI]) {
        expected_offset_ = CI;
      }

      if ( !has_sync_for_[expected_offset_]) {

        // If message is a correct PI, error was probably in check bits
        if (expected_offset_ == A && message == pi_ && pi_ != 0) {
          has_sync_for_[A] = true;
          //printf(":offset 0: ignoring error in check bits\n");
        } else if (expected_offset_ == C && message == pi_ && pi_ != 0) {
          has_sync_for_[CI] = true;
          //printf(":offset 0: ignoring error in check bits\n");

        // Detect & correct clock slips (Section C.1.2)
        } else if (expected_offset_ == A && pi_ != 0 &&
            ((wideblock_ >> 12) & kBitmask16) == pi_) {
          message = pi_;
          wideblock_ >>= 1;
          has_sync_for_[A] = true;
          //printf(":offset 0: clock slip corrected\n");
        } else if (expected_offset_ == A && pi_ != 0 &&
            ((wideblock_ >> 10) & kBitmask16) == pi_) {
          message = pi_;
          wideblock_ = (wideblock_ << 1) + getNextBit();
          has_sync_for_[A] = true;
          left_to_read_ = 25;
          //printf(":offset 0: clock slip corrected\n");

        // Detect & correct burst errors (Section B.2.2)
        } else {

          block = correctBurstErrors(block);
          if (calcSyndrome(block) == 0x000) {
            message = block >> 10;
            has_sync_for_[expected_offset_] = true;
          }

        }

        // Still no sync pulse
        if ( !has_sync_for_[expected_offset_]) {
          uncorrectable();
        }
      }

      // Error-free block received

      if (has_sync_for_[expected_offset_]) {

        group_data_[block_for_offset[expected_offset_]] = message;
        has_block_[expected_offset_] = true;

        if (expected_offset_ == A) {
          pi_ = message;
        }

        // Complete group received
        if (has_block_[A] && has_block_[B] && (has_block_[C] ||
            has_block_[CI]) && has_block_[D]) {
          has_new_group_ = true;
          data_length_ = 4;
        }
      }

      expected_offset_ = nextOffsetFor(expected_offset_);

      if (expected_offset_ == A) {
        for (eOffset o : {A, B, C, CI, D})
          has_block_[o] = false;
      }

    }

  }

  auto result = group_data_;
  result.resize(data_length_);

  return result;

}

bool BlockStream::isEOF() const {
  return is_eof_;
}

} // namespace redsea
