#include "blockstream.h"

#define MASK_5BIT  0x000001F
#define MASK_10BIT 0x00003FF
#define MASK_16BIT 0x000FFFF
#define MASK_26BIT 0x3FFFFFF
#define MASK_28BIT 0xFFFFFFF

uint16_t syndrome(int vec) {

  uint16_t synd_reg = 0x000;
  int bit,l;

  for (int k=25; k>=0; k--) {
    bit       = (vec & (1 << k));
    l         = (synd_reg & 0x200);      // Store lefmost bit of register
    synd_reg  = (synd_reg << 1) & 0x3FF; // Rotate register
    synd_reg ^= (bit ? 0x31B : 0x00);    // Premultiply input by x^325 mod g(x)
    synd_reg ^= (l   ? 0x1B9 : 0x00);    // Division mod 2 by g(x)
  }

  return synd_reg;
}

BlockStream::BlockStream() : has_sync_for_(5), offset_word_({0x0FC, 0x198, 0x168, 0x350, 0x1B4}),
  block_for_offset_({0, 1, 2, 2, 3}), group_data_(4), has_block_(5), bitcount_(0), left_to_read_(0),
  bit_stream_(), wideblock_(0), block_has_errors_(50)

{

  error_vector_ = {
    {0x200, 0b1000000000000000},
    {0x300, 0b1100000000000000},
    {0x180, 0b0110000000000000},
    {0x0c0, 0b0011000000000000},
    {0x060, 0b0001100000000000},
    {0x030, 0b0000110000000000},
    {0x018, 0b0000011000000000},
    {0x00c, 0b0000001100000000},
    {0x006, 0b0000000110000000},
    {0x003, 0b0000000011000000},
    {0x2dd, 0b0000000001100000},
    {0x3b2, 0b0000000000110000},
    {0x1d9, 0b0000000000011000},
    {0x230, 0b0000000000001100},
    {0x118, 0b0000000000000110},
    {0x08c, 0b0000000000000011},
    {0x046, 0b0000000000000001}
  };
}

void BlockStream::blockError() {
  printf("offset %d not received\n",expected_offset_);
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
    for (int i=0; i<block_has_errors_.size(); i++)
      block_has_errors_[i] = false;
    pi_ = 0x0000;
    printf("too many errors, sync lost\n");
  }

  for (int o=A; o<=D; o++)
    has_block_[o] = false;

}

std::vector<uint16_t> BlockStream::getNextGroup() {

  has_whole_group_ = false;

  while (!has_whole_group_) {

    // Compensate for clock slip corrections
    bitcount_ += 26 - left_to_read_;

    // Read from radio
    for (int i=0; i < (is_in_sync_ ? left_to_read_ : 1); i++, bitcount_++) {
      wideblock_ = (wideblock_ << 1) + bit_stream_.getNextBit();
    }

    left_to_read_ = 26;
    wideblock_ &= MASK_28BIT;

    int block = (wideblock_ >> 1) & MASK_26BIT;

    // Find the offsets for which the syndrome is zero
    bool has_sync_for_any = false;
    for (int o=A; o<=D; o++) {
      has_sync_for_[o] = (syndrome(block ^ offset_word_[o]) == 0x000);
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
              printf("sync!\n");
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
          printf("ignoring error in check bits\n");
        } else if (expected_offset_ == C && message == pi_ && pi_ != 0) {
          has_sync_for_[CI] = true;
          printf("ignoring error in check bits\n");

        // Detect & correct clock slips (Section C.1.2)
        } else if (expected_offset_ == A && pi_ != 0 &&
            ((wideblock_ >> 12) & MASK_16BIT) == pi_) {
          message = pi_;
          wideblock_ >>= 1;
          has_sync_for_[A] = true;
          printf("clock slip corrected\n");
        } else if (expected_offset_ == A && pi_ != 0 &&
            ((wideblock_ >> 10) & MASK_16BIT) == pi_) {
          message = pi_;
          wideblock_ = (wideblock_ << 1) + bit_stream_.getNextBit();
          has_sync_for_[A] = true;
          left_to_read_ = 25;
          printf("clock slip corrected\n");

        // Detect & correct burst errors (Section B.2.2)
        } else {

          uint16_t synd_reg = syndrome(block ^ offset_word_[expected_offset_]);

          if (pi_ != 0 && expected_offset_ == A) {
            printf("expecting PI%04x, got %04x, xor %d, syndrome %03x\n",
                pi_, block>>10, pi_ ^ (block>>10), synd_reg);
          }

          if (error_vector_.find(synd_reg) != error_vector_.end()) {
            message = (block >> 10) ^ error_vector_[synd_reg];
            has_sync_for_[expected_offset_] = true;

            printf("error corrected block %d using vector %04x for syndrome %03x\n",
                expected_offset_, error_vector_[synd_reg], synd_reg);

          }

        }

        // Still no sync pulse
        if ( !has_sync_for_[expected_offset_]) {
          blockError();
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
        if (has_block_[A] && has_block_[B] && (has_block_[C] || has_block_[CI]) && has_block_[D]) {
          has_whole_group_ = true;
          //printf("%04x %04x %04x %04x\n",group_data_[0], group_data_[1], group_data_[2], group_data_[3]);
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

bool BlockStream::eof() const {
  return bit_stream_.eof();
}
