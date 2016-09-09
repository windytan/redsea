#include "block_sync.h"

namespace redsea {

namespace {

const unsigned kBitmask16 = 0x000FFFF;
const unsigned kBitmask26 = 0x3FFFFFF;
const unsigned kBitmask28 = 0xFFFFFFF;

const unsigned kMaxErrorLength = 3;

uint32_t calcSyndrome(uint32_t vec) {

  uint32_t synd_reg = 0x000;
  uint32_t bit,l;

  for (int k=25; k>=0; k--) {
    bit       = (vec & (1 << k));
    l         = (synd_reg & 0x200);      // Store lefmost bit of register
    synd_reg  = (synd_reg << 1) & 0x3FF; // Rotate register
    synd_reg ^= (bit ? 0x31B : 0x00);    // Premultiply input by x^325 mod g(x)
    synd_reg ^= (l   ? 0x1B9 : 0x00);    // Division mod 2 by g(x)
  }

  return synd_reg;
}

uint32_t calcCheckBits(uint32_t data_word) {
  static const std::vector<uint32_t> genmat({
      0x077, 0x2e7, 0x3af, 0x30b, 0x359, 0x370, 0x1b8, 0x0dc,
      0x06e, 0x037, 0x2c7, 0x3bf, 0x303, 0x35d, 0x372, 0x1b9
  });
  uint32_t result = 0;

  for (int k=0; k<16; k++) {
    if ((data_word >> k) & 0x01) {
      result ^= genmat[15-k];
    }
  }
  return result;
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

      uint32_t m = calcCheckBits(0x01);
      uint32_t sy = calcSyndrome(((1<<10) + m) ^ errvec);
      result[sy] = errvec >> 10;
    }
  }
  return result;
}

}

BlockStream::BlockStream(int input_type) : bitcount_(0), prevbitcount_(0),
  left_to_read_(0), wideblock_(0), prevsync_(0), block_counter_(0),
  expected_offset_(A), pi_(0), has_sync_for_(5), is_in_sync_(false),
  offset_word_({0x0FC, 0x198, 0x168, 0x350, 0x1B4}),
  block_for_offset_({0, 1, 2, 2, 3}), group_data_(4), has_block_(5),
  block_has_errors_(50), subcarrier_(), ascii_bits_(), has_new_group_(false),
  error_lookup_(), data_length_(0), input_type_(input_type), is_eof_(false) {

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
      has_sync_for_[o] = (calcSyndrome(block ^ offset_word_[o]) == 0x000);
      has_sync_for_any |= has_sync_for_[o];
    }

    // Acquire sync

    if (!is_in_sync_) {
      if (has_sync_for_any) {
        for (eOffset o : {A, B, C, CI, D}) {
          if (has_sync_for_[o]) {
            int dist = bitcount_ - prevbitcount_;

            if (dist % 26 == 0 && dist <= 156 &&
                (block_for_offset_[prevsync_] + dist/26) % 4 ==
                block_for_offset_[o]) {
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

          uint16_t synd_reg =
            calcSyndrome(block ^ offset_word_[expected_offset_]);

          if (pi_ != 0 && expected_offset_ == A) {
            //printf(":offset 0: expecting PI%04x, got %04x, xor %04x, "
            //  "syndrome %03x\n", pi_, block>>10, pi_ ^ (block>>10), synd_reg);
          }

          if (error_lookup_.find(synd_reg) != error_lookup_.end()) {
            uint32_t corrected_block = (block ^ offset_word_[expected_offset_])
              ^ (error_lookup_[synd_reg] << 10);

            if (calcSyndrome(corrected_block) == 0x000) {
              message = (block >> 10) ^ error_lookup_[synd_reg];
              has_sync_for_[expected_offset_] = true;
            }

            /*printf(":offset %d: corrected %04x->%04x using vector %04x for "
              "syndrome %03x->%03x\n", expected_offset_, block >> 10, message,
              error_lookup_[synd_reg],
              synd_reg,calcSyndrome(corrected_block));
*/
          }

        }

        // Still no sync pulse
        if ( !has_sync_for_[expected_offset_]) {
          uncorrectable();
        }
      }

      // Error-free block received

      if (has_sync_for_[expected_offset_]) {

        group_data_[block_for_offset_[expected_offset_]] = message;
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
