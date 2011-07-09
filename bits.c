/* bits.c -- part of redsea RDS decoder (c) OH2-250
 *
 * decodes physical layer:  get_bit()
 *         data-link layer: main()
 *
 * stdin:  RDS signal PCM at baseband
 *         raw pcm, 16bit, little-endian, signed-integer, single-channel,
 *         6000 samples/sec
 *
 * stdout: RDS data
 *         A group is preceded by a one-byte block count header
 *         One block = 2 bytes
 *         special codes: 0x00 = sync lost
 *                        0xFF = sync acquired
 * 
 *
 * Page, section and figure numbers refer to IEC 62106, Edition 2
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>

#include "bits.h"

// Demodulate next PSK bit from PCM stream
short int get_bit() {

  int                   j, f=FALSE, g=FALSE, p;
  static double         w = 2 * M_PI * (1187.5 / 6000.0);  // PSK carrier frequency, rad/sample
  static unsigned short bit_phase = 0;     /* bit phase */
  static unsigned short bpadvance = 12971; //   2^16 / (6000 Hz / 1187.5 bps)
  static unsigned int   lo_time = 0;
  static unsigned int   intptr=0, bit=0, prevbit=0, diffbit=0;
  static short int      sample=0, prevsample=0, x=0, x0=0;
  static int            integral[5] = {0};
  static double         lo = 0;
  static double         lo_phase = 0;

  j = bit_phase;
  if (j>0x8000) j -= 0x10000;
  for (f=p=0; j < 0x10000; j += bpadvance) {

    if (fread(&sample, 2, 1, stdin)) {

      /* Local oscillator at 1187.5 Hz */
      lo = cos(w * lo_time + lo_phase);
      lo_time++;

      /* Product detector & integrator over 5 samples */
      integral[intptr++ % 5] = sample * lo;
      x = (integral[0] + integral[1] + integral[2] + integral[3] + integral[4]) / 5;

      /* PSK carrier recovery PLL */
      if (prevsample * sample < 0) {   /* Zero crossing */
        if (!g) {
          if ( fmod(w * lo_time + lo_phase, M_PI) >= M_PI/2 )
            lo_phase += 0.04;
          else
            lo_phase -= 0.04;
          lo_phase = fmod(lo_phase, 2*M_PI);
          g = TRUE;
        }
      }

      /* Data clock recovery PLL */
      if (x * x0 < 0) {       /* Data Change */
        if (!f) {
          if (j < 0x8000) p =  2048;  /* Early */
          else            p = -2048;  /* Late */
          f = TRUE;
        }
      }

      x0         = x;
      prevsample = sample;
    } else {
      exit(EXIT_FAILURE);
    }
  }
  bit_phase = (j + p) & _16BIT;
  bit       = (x > 0);

  /* Differential coding */
  diffbit = bit ^ prevbit;
  prevbit = bit;

  return(diffbit);

}

// Calculate the syndrome of a 26-bit vector
unsigned int syndrome (unsigned int vector) {

  int k;
  unsigned int SyndReg=0,l,bit;

  // Figure B.4

  for (k=25; k>=0; k--) {
    bit = (vector & (1 << k));

    // Store leftmost bit of register
    l       = (SyndReg & 0x200);

    // Rotate register
    SyndReg = (SyndReg << 1) & _10BIT;

    // Premultiply input by x^325 mod g(x)
    if (bit) SyndReg ^= 0x31B;

    // Division mod 2 by generator polynomial
    if (l)   SyndReg ^= 0x1B9;
  }

  return(SyndReg);
}

// When a block has uncorrectable errors, dump the group received so far
void blockerror () {
  int k;
  unsigned char datalen=0,buf=0;

  if (rcvd[A]) {
    datalen = 1;

    if (rcvd[B]) {
      datalen = 2;

      if (rcvd[C] || rcvd[Ci]) {
        datalen = 3;
      }
    }

    fwrite(&datalen, 1, 1, stdout);
    for (k = 0; k < datalen; k++) fwrite(&grp_data[k], 1, 2, stdout);
    fflush(stdout);

  } else if (rcvd[Ci]) {
    datalen = 1;
    fwrite(&datalen, 1, 1, stdout);
    fwrite(&grp_data[2], 1, 2, stdout);
    fflush(stdout);
  }

  errblock[BlkPointer % 50] = TRUE;

  erbloks = 0;
  for (k = 0; k < 50; k ++) erbloks += errblock[k];

  // Sync is lost when >45 out of last 50 blocks are erroneous (C.1.2)
  if (insync && erbloks > 45) {
    insync = FALSE;
    for (k = 0; k < 50; k ++) errblock[k] = FALSE;
    buf = 0x00;
    fwrite(&buf, 1, 1, stdout);
    fflush(stdout);
  }
  rcvd[A] = rcvd[B] = rcvd[C] = rcvd[Ci] = rcvd[D] = FALSE;
}

int main() {

  int          k, doCorrect=FALSE, prevsync=0;
  int          i=0, syncb[5] = {FALSE}, lefttoread=26;
  unsigned int block = 0, pi=0, wideblock = 0;
  unsigned int message, bitcount = 0, prevbitcount = 0;
  unsigned int j, l, err, SyndReg, dist;
  unsigned char datalen=0, buf=0;

  // Offset words A, B, C, C', D
  unsigned int offset[5] = { 0x0FC, 0x198, 0x168, 0x350, 0x1B4 };

  // Map offset numbers to block numbers
  unsigned int ofs2block[5] = { 0, 1, 2, 2, 3 };

  while(1) {

    // Compensate for clock slip corrections
    bitcount += 26-lefttoread;

    // Read from radio
    for (i=0; i < (insync ? lefttoread : 1); i++, bitcount++)
      wideblock = (wideblock << 1) + get_bit();
    
    lefttoread = 26;
    wideblock &= _28BIT;  // wideblock is a buffer containing the block + 1 adjacent
                          // bit on both sides

    block      = (wideblock >> 1) & _26BIT;

    // Find the offsets for which the syndrome is zero
    for (j = A; j <= D; j ++) syncb[j] = (syndrome(block ^ offset[j]) == 0);



    /* Acquire synchronization */

    if (!insync) {

      if ( syncb[A] | syncb[B] | syncb[C] | syncb[Ci] | syncb[D] ) {

        for (j = A; j <= D; j ++) {
          if (syncb[j]) {
            
            dist = bitcount - prevbitcount;

            if (   dist % 26 == 0
                && dist <= 156
                && (ofs2block[prevsync] + dist/26) % 4 == ofs2block[j] ) {
              insync = TRUE;
              expofs = j;
              buf = 0xFF;
              fwrite(&buf, 1, 1, stdout);
              fflush(stdout);
              break;
            } else {
              prevbitcount = bitcount;
              prevsync     = j;
            }
          }
        }
      }
    }



    /* Synchronous decoding */
   
    if (insync) {

      BlkPointer ++;

      message = block >> 10;
      
      // If expecting C but we only got a Ci sync pulse, we have a Ci block
      if (expofs == C && !syncb[C] && syncb[Ci]) expofs = Ci;

      // If this block offset won't give a sync pulse
      if (!syncb[expofs]) {

        // If it's a correct PI, the error was probably in the check bits and hence is ignored
        if      (expofs == A && message == pi && pi != 0) syncb[A]  = TRUE;
        else if (expofs == C && message == pi && pi != 0) syncb[Ci] = TRUE;

        // Detect & correct clock slips (C.1.2)

        else if   (expofs == A && pi != 0 && ( (wideblock >> 12) & _16BIT ) == pi) {
          message     = pi;
          wideblock >>= 1;
          syncb[A]    = TRUE;
        } else if (expofs == A && pi != 0 && ( (wideblock >> 10) & _16BIT ) == pi) {
          message     = pi;
          wideblock   = (wideblock << 1) + get_bit();
          syncb[A]    = TRUE;
          lefttoread  = 25;
        }

        // Detect & correct burst errors (B.2.2)
     
        else if (doCorrect) { 
          SyndReg = syndrome(block ^ offset[expofs]);

          for (k = 0; k < 15; k ++) {
            if (k > 0) {
              l       =  SyndReg &  0x200;
              SyndReg = (SyndReg << 1) & _10BIT;
              if (l)     SyndReg ^= 0x1B9;
            }
            if ((SyndReg & _5BIT) == 0) {

              err            = (SyndReg >> k)  & _16BIT;
              message        = (block   >> 10) ^ err;
              syncb[expofs] = TRUE;
              break;
            }

          }
        }

        // If still no sync pulse
        if ( !syncb[expofs] ) blockerror();

      }

      // Error-free block received
      if (syncb[expofs]) {

        grp_data[ofs2block[expofs]] = message;
        errblock[BlkPointer % 50]   = FALSE;
        rcvd[expofs]                = TRUE;

        if (expofs == A) pi         = message;

        // If a complete group is received
        if (rcvd[A] && rcvd[B] && (rcvd[C] || rcvd[Ci]) && rcvd[D]) {

          // Print it out
          datalen = 4;
          if (!fwrite(&datalen, 1, 1, stdout)) return(EXIT_FAILURE);
          for (k = 0; k < datalen; k++) fwrite(&grp_data[k], 1, 2, stdout);
          fflush(stdout);
        }
      }
      
      // The block offset we're expecting next
      expofs = (expofs == C ? D : (expofs + 1) % 5);

      if (expofs == A) rcvd[A] = rcvd[B] = rcvd[C] = rcvd[Ci] = rcvd[D] = FALSE;

    }
    
  }
  return (EXIT_SUCCESS);
}
