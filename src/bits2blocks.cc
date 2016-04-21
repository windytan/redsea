#include "bits2blocks.h"

#define MASK_5BIT  0x000001F
#define MASK_10BIT 0x00003FF
#define MASK_16BIT 0x000FFFF
#define MASK_26BIT 0x3FFFFFF
#define MASK_28BIT 0xFFFFFFF

#define MAX_ERR_LEN 0

namespace redsea {

namespace {

uint32_t rol10(uint32_t word, int k) {
  uint32_t result = word;
  uint32_t l;
  for (int i=0; i<k; i++) {
    l       = (result & 0x200);
    result  = (result << 1) & 0x3FF;
    result ^= l;
  }
  return result;
}

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

uint32_t calcCheckBits(uint32_t dataWord) {
  uint32_t generator = 0x1B9;
  uint32_t result    = 0;

  for (int k=0; k<16; k++) {
    if ((dataWord >> k) & 0x01) {
      result ^= rol10(generator, k);
    }
  }
  return result;
}

}

BlockStream::BlockStream() : has_sync_for_(5), group_data_(4), has_block_(5),
  bitcount_(0), left_to_read_(0), bit_stream_(), wideblock_(0), block_has_errors_(50)

{

  offset_word_ = {0x0FC, 0x198, 0x168, 0x350, 0x1B4};

  block_for_offset_ = {0, 1, 2, 2, 3};

  for (uint32_t e=1; e < (1<<MAX_ERR_LEN); e++) {
    for (int shift=0; shift < 16; shift++) {
      uint32_t errvec = (e << shift) & MASK_16BIT;

      uint32_t m = calcCheckBits(0x01);
      uint32_t sy = calcSyndrome(((1<<10) + m) ^ errvec);
      error_lookup_[sy] = errvec;
    }
  }

}

void BlockStream::uncorrectable() {
  printf(":offset %d: not received\n",expected_offset_);
  int data_length = 0;

  // TODO: return partial group
  /*if (has_block_[A]) {
    data_length = 1;

    if (has_block_[B]) {
      data_length = 2;

      if (has_block_[C] || has_block_[CI]) {
        data_length = 3;
      }
    }
  }*/

  block_has_errors_[block_counter_ % block_has_errors_.size()] = true;

  int erroneous_blocks = 0;
  for (bool e : block_has_errors_) {
    if (e)
      erroneous_blocks ++;
  }

  // Sync is lost when >45 out of last 50 blocks are erroneous (Section C.1.2)
  if (is_in_sync_ && erroneous_blocks > 45) {
    is_in_sync_ = false;
    for (int i=0; i<(int)block_has_errors_.size(); i++)
      block_has_errors_[i] = false;
    pi_ = 0x0000;
    printf(":too many errors, sync lost\n");
  }

  for (int o=A; o<=D; o++)
    has_block_[o] = false;

}

std::vector<uint16_t> BlockStream::getNextGroup() {

  has_whole_group_ = false;

  while (!(has_whole_group_ || isEOF())) {

    // Compensate for clock slip corrections
    bitcount_ += 26 - left_to_read_;

    // Read from radio
    for (int i=0; i < (is_in_sync_ ? left_to_read_ : 1); i++, bitcount_++) {
      wideblock_ = (wideblock_ << 1) + bit_stream_.getNextBit();
    }

    left_to_read_ = 26;
    wideblock_ &= MASK_28BIT;

    uint32_t block = (wideblock_ >> 1) & MASK_26BIT;

    // Find the offsets for which the calcSyndrome is zero
    bool has_sync_for_any = false;
    for (int o=A; o<=D; o++) {
      has_sync_for_[o] = (calcSyndrome(block ^ offset_word_[o]) == 0x000);
      has_sync_for_any |= has_sync_for_[o];
    }

    // Acquire sync

    if (!is_in_sync_) {
      if (has_sync_for_any) {
        for (int o=A; o<=D; o++) {
          if (has_sync_for_[o]) {
            int dist = bitcount_ - prevbitcount_;

            if (dist % 26 == 0 && dist <= 156 &&
                (block_for_offset_[prevsync_] + dist/26) % 4 == block_for_offset_[o]) {
              is_in_sync_ = true;
              expected_offset_ = o;
              printf(":sync!\n");
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
          printf(":offset 0: ignoring error in check bits\n");
        } else if (expected_offset_ == C && message == pi_ && pi_ != 0) {
          has_sync_for_[CI] = true;
          printf(":offset 0: ignoring error in check bits\n");

        // Detect & correct clock slips (Section C.1.2)
        } else if (expected_offset_ == A && pi_ != 0 &&
            ((wideblock_ >> 12) & MASK_16BIT) == pi_) {
          message = pi_;
          wideblock_ >>= 1;
          has_sync_for_[A] = true;
          printf(":offset 0: clock slip corrected\n");
        } else if (expected_offset_ == A && pi_ != 0 &&
            ((wideblock_ >> 10) & MASK_16BIT) == pi_) {
          message = pi_;
          wideblock_ = (wideblock_ << 1) + bit_stream_.getNextBit();
          has_sync_for_[A] = true;
          left_to_read_ = 25;
          printf(":offset 0: clock slip corrected\n");

        // Detect & correct burst errors (Section B.2.2)
        } else {

          uint16_t synd_reg = calcSyndrome(block ^ offset_word_[expected_offset_]);

          if (pi_ != 0 && expected_offset_ == A) {
            printf(":offset 0: expecting PI%04x, got %04x, xor %04x, syndrome %03x\n",
                pi_, block>>10, pi_ ^ (block>>10), synd_reg);
          }

          if (error_lookup_.find(synd_reg) != error_lookup_.end()) {
            message = (block >> 10) ^ error_lookup_[synd_reg];
            has_sync_for_[expected_offset_] = true;

            printf(":offset %d: error corrected using vector %04x for syndrome %03x\n",
                expected_offset_, error_lookup_[synd_reg], synd_reg);

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
          has_whole_group_ = true;
        }
      }

      expected_offset_ = (expected_offset_ == C ? D : (expected_offset_ + 1) % 5);

      if (expected_offset_ == A) {
        for (int o=A; o<=D; o++)
          has_block_[o] = false;
      }

    }

  }

  return group_data_;

}

bool BlockStream::isEOF() const {
  return bit_stream_.isEOF();
}

} // namespace redsea
