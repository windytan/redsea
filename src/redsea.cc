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

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "filters.h"

#define FS      250000.0
#define FC_0    57000.0
#define IBUFLEN 4096
#define OBUFLEN 128

int tot_errs[2] = {0};
int reading_frame = 0;

void bit(char b) {
  static int nbit = 0;
  printf("%d", b);
  if (nbit % OBUFLEN == 0)
    fflush(0);
  nbit++;
}

void print_delta(char b) {
  static int dbit = 0;
  bit(b ^ dbit);
  dbit = b;
}

int sign(double a) {
  return (a >= 0 ? 1 : 0);
}

void biphase(double acc) {
  static double prev_acc = 0;
  static int    counter = 0;

  if (sign(acc) != sign(prev_acc)) {
    tot_errs[counter % 2] ++;
  }

  if (counter % 2 == reading_frame) {
    print_delta(sign(acc + prev_acc));
  }
  if (counter == 0) {
    if (tot_errs[1 - reading_frame] < tot_errs[reading_frame]) {
      reading_frame = 1 - reading_frame;
    }
    tot_errs[0] = 0;
    tot_errs[1] = 0;
  }

  prev_acc = acc;
  counter = (counter + 1) % 800;
}

int main(int argc, char **argv) {

  int16_t  sample[IBUFLEN];

  double subcarr_phi    = 0;
  double subcarr_bb[2]  = {0};
  double clock_offset   = 0;
  double clock_phi      = 0;
  double lo_clock       = 0;
  double prevclock      = 0;
  double prev_bb        = 0;
  double d_phi_sc       = 0;
  double d_cphi         = 0;
  double acc            = 0;
  double subcarr_sample = 0;
  int    c;
  int    fmfreq         = 0;
  int    bytesread;
  int    numsamples     = 0;
  double loop_out       = 0;
  double prev_loop      = 0;

  double fsc             = 57000;

  while ((c = getopt (argc, argv, "f:")) != -1)
    switch (c) {
      case 'f':
        fmfreq = atoi(optarg);
        break;
      case '?':
        fprintf (stderr, "Unknown option `-%c`\n", optopt);
        return EXIT_FAILURE;
      default:
        break;
    }

  while (1) {
    bytesread = fread(sample, sizeof(int16_t), IBUFLEN, stdin);
    if (bytesread < 1) exit(0);

    int i;
    for (i = 0; i < bytesread; i++) {

      /* Subcarrier downmix & phase recovery */

      subcarr_phi    += 2 * M_PI * fsc * (1.0/FS);
      subcarr_bb[0]  = filter_lp_2400_iq(sample[i] / 32768.0 * cos(subcarr_phi), 0);
      subcarr_bb[1]  = filter_lp_2400_iq(sample[i] / 32768.0 * sin(subcarr_phi), 1);

      double pll_beta  = 50;

      d_phi_sc     = 2*filter_lp_pll(subcarr_bb[1] * subcarr_bb[0]);
      //loop_out     = d_phi_sc + pll_alpha * prev_loop;
      subcarr_phi -= pll_beta * d_phi_sc;//prev_loop;
      fsc         -= .5 * pll_beta * d_phi_sc;//prev_loop;
      //prev_loop    = loop_out;

      /* 1187.5 Hz clock */

      clock_phi = subcarr_phi / 48.0 + clock_offset;
      lo_clock  = (fmod(clock_phi, 2*M_PI) < M_PI ? 1 : -1);
      //lo_clock = sin(clock_phi);

      /* Clock phase recovery */

      if (sign(prev_bb) != sign(subcarr_bb[0])) {
        d_cphi = fmod(clock_phi, M_PI);
        if (d_cphi >= M_PI_2) d_cphi -= M_PI;
        clock_offset -= 0.005 * d_cphi;
      }

      /* Decimate band-limited signal */
      if (numsamples % 8 == 0) {

        /* biphase symbol integrate & dump */
        acc += subcarr_bb[0] * lo_clock;

        if (sign(lo_clock) != sign(prevclock)) {
          biphase(acc);
          acc = 0;
        }

        prevclock = lo_clock;
      }

      numsamples ++;

      prev_bb = subcarr_bb[0];

    }
  }
}
