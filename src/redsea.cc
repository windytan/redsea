/*
 * redsea - RDS decoder
 * Copyright (c) Oona Räisänen OH2EIQ (windyoona@gmail.com)
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

#include "redsea.h"

#include <complex>

#include "filters.h"

#define FS      250000.0
#define FC_0    57000.0
#define IBUFLEN 4096
#define OBUFLEN 128
#define BITBUFLEN 1024

#define MASK_5BIT  0x000001F
#define MASK_10BIT 0x00003FF
#define MASK_16BIT 0x000FFFF
#define MASK_26BIT 0x3FFFFFF
#define MASK_28BIT 0xFFFFFFF

BitReceiver::BitReceiver() : tot_errs_(2), reading_frame_(0), counter_(0), fsc_(FC_0), bit_buffer_fill_count_(0), bit_buffer_write_ptr_(0), bit_buffer_read_ptr_(0), bit_buffer_(BITBUFLEN), subcarr_phi_(0), clock_offset_(0) {

}

void BitReceiver::bit(int b) {
  bit_buffer_[bit_buffer_write_ptr_] = b;
  bit_buffer_write_ptr_ = (bit_buffer_write_ptr_ + 1) % BITBUFLEN;
  bit_buffer_fill_count_ ++;
  /*if (nbit % 4 == 0) {
    printf("%x", nybble & 0xf);
    if ((nbit/4) % OBUFLEN == 0) {
      fflush(0);
    }
  }*/
}

void BitReceiver::deltaBit(int b) {
  bit(b ^ dbit_);
  dbit_ = b;
}

int sign(double a) {
  return (a >= 0 ? 1 : 0);
}

void BitReceiver::biphase(double acc) {

  if (sign(acc) != sign(prev_acc_)) {
    tot_errs_[counter_ % 2] ++;
  }

  if (counter_ % 2 == reading_frame_) {
    deltaBit(sign(acc + prev_acc_));
  }
  if (counter_ == 0) {
    if (tot_errs_[1 - reading_frame_] < tot_errs_[reading_frame_]) {
      reading_frame_ = 1 - reading_frame_;
    }
    tot_errs_[0] = 0;
    tot_errs_[1] = 0;
  }

  prev_acc_ = acc;
  counter_ = (counter_ + 1) % 800;
}


void BitReceiver::readMoreBits() {

  int16_t sample[IBUFLEN];
  int bytesread = fread(sample, sizeof(int16_t), IBUFLEN, stdin);
  if (bytesread < 1) exit(0);

  for (int i = 0; i < bytesread; i++) {

    /* Subcarrier downmix & phase recovery */

    subcarr_phi_ += 2 * M_PI * fsc_ * (1.0/FS);
    std::complex<double> subcarr_bb =
      filter_lp_2400_iq(sample[i] / 32768.0 * std::polar(1.0, subcarr_phi_));

    double pll_beta = 50;

    double d_phi_sc = 2.0*filter_lp_pll(real(subcarr_bb) * imag(subcarr_bb));
    subcarr_phi_ -= pll_beta * d_phi_sc;
    fsc_         -= .5 * pll_beta * d_phi_sc;

    /* 1187.5 Hz clock */

    double clock_phi = subcarr_phi_ / 48.0 + clock_offset_;
    double lo_clock  = (fmod(clock_phi, 2*M_PI) < M_PI ? 1 : -1);

    /* Clock phase recovery */

    if (sign(prev_bb_) != sign(real(subcarr_bb))) {
      double d_cphi = fmod(clock_phi, M_PI);
      if (d_cphi >= M_PI_2) d_cphi -= M_PI;
      clock_offset_ -= 0.005 * d_cphi;
    }

    /* Decimate band-limited signal */
    if (numsamples_ % 8 == 0) {

      /* biphase symbol integrate & dump */
      acc_ += real(subcarr_bb) * lo_clock;

      if (sign(lo_clock) != sign(prevclock_)) {
        biphase(acc_);
        acc_ = 0;
      }

      prevclock_ = lo_clock;
    }

    numsamples_ ++;

    prev_bb_ = real(subcarr_bb);

  }

}


int BitReceiver::getNextBit() {
  while (bit_buffer_fill_count_ < 1)
    readMoreBits();

  int result = bit_buffer_[bit_buffer_read_ptr_];
  bit_buffer_fill_count_ --;
  bit_buffer_read_ptr_ = (bit_buffer_read_ptr_ + 1) % BITBUFLEN;
  //printf("read %d, write %d, fill count %d\n",bit_buffer_read_ptr_, bit_buffer_write_ptr_, bit_buffer_fill_count_);
  return result;
}

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

BlockReceiver::BlockReceiver() : has_sync_for_(5), offset_word_({0x0FC, 0x198, 0x168, 0x350, 0x1B4}),
  block_for_offset_({0, 1, 2, 2, 3}), group_data_(4), has_block_(5), bitcount_(0), left_to_read_(0),
  bit_receiver_(), wideblock_(0), block_has_errors_(50)

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

void BlockReceiver::blockError() {
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

std::vector<uint16_t> BlockReceiver::getNextGroup() {

  has_whole_group_ = false;

  while (!has_whole_group_) {

    // Compensate for clock slip corrections
    bitcount_ += 26 - left_to_read_;

    // Read from radio
    for (int i=0; i < (is_in_sync_ ? left_to_read_ : 1); i++, bitcount_++) {
      wideblock_ = (wideblock_ << 1) + bit_receiver_.getNextBit();
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
          wideblock_ = (wideblock_ << 1) + bit_receiver_.getNextBit();
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
          printf("%04x %04x %04x %04x\n",group_data_[0], group_data_[1], group_data_[2], group_data_[3]);
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

int main() {
  BlockReceiver block_receiver;

  while (true) {
    block_receiver.getNextGroup();
  }
}
