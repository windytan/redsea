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

#ifdef DEBUG
char dbit,sbit;
int tot_errs[2];
int reading_frame;
double fsc;
double qua;
#endif

void print_delta(char b) {
  static int nbit = 0;
#ifdef DEBUG
  sbit = (b ^ dbit ? 1 : -1);
#else
  static int dbit = 0;
#endif
  printf("%d", b ^ dbit);
  if (nbit % OBUFLEN == 0)
    fflush(0);
  dbit = b;
  nbit++;
}

void bit(char b) {

}

int sign(double a) {
  return (a >= 0 ? 1 : 0);
}

void biphase(double acc) {
  static double prev_acc = 0;
  static int    counter = 0;
#ifndef DEBUG
  static int    reading_frame  = 0;
  static int    tot_errs[2]     = {0};
#endif

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
#ifdef DEBUG
    qua = (1.0 * abs(tot_errs[0] - tot_errs[1]) /
          (tot_errs[0] + tot_errs[1])) * 100;
    fprintf(stderr, "frame: %d  errs: %3d %3d  qual: %3.0f%%  pll: %.1f\n",
        reading_frame, tot_errs[0], tot_errs[1], qua, fsc);
#endif
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
  int numsamples = 0;
  double loop_out = 0;
  double prev_d_phi_sc = 0;

#ifdef DEBUG
  sbit = 0;
  dbit = 0;
  reading_frame = 0;
  qua = 0;
  fsc = FC_0;
  double t = 0;
#else
  double fsc             = 57000;
#endif

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

#ifdef DEBUG
  int16_t outbuf[1];
  FILE *U;
  U = popen("sox -c 4 -r 250000 -t .s16 - dbg-out.wav", "w");
  FILE *IQ;
  IQ = popen("sox -c 2 -r 250000 -t .s16 - dbg-out-iq.wav", "w");
  FILE *STATS;
  STATS = fopen("stats.csv", "w");
  fprintf(STATS, "t,fp,d_pphi,d_phi_sc,clock_offset,qua\n");
#endif

  while (1) {
    bytesread = fread(sample, sizeof(int16_t), IBUFLEN, stdin);
    if (bytesread < 1) exit(0);

    int i;
    for (i = 0; i < bytesread; i++) {


      /* Subcarrier downmix & phase recovery */

      subcarr_phi    += 2 * M_PI * fsc * (1.0/FS);
      subcarr_bb[0]  = filter_lp_2400_iq(sample[i] / 32768.0 * cos(subcarr_phi), 0);
      subcarr_bb[1]  = filter_lp_2400_iq(sample[i] / 32768.0 * sin(subcarr_phi), 1);

      double pll_beta = 0.0001;

      d_phi_sc     = atan2(subcarr_bb[1], subcarr_bb[0]);
      if (d_phi_sc >= M_PI_2) {
        d_phi_sc -= M_PI;
      } else if (d_phi_sc <= -M_PI_2) {
        d_phi_sc += M_PI;
      }
      loop_out = d_phi_sc + 0.9375 * prev_d_phi_sc;
      subcarr_phi -= pll_beta * loop_out;
      fsc         -= pll_beta * loop_out;
      prev_d_phi_sc = d_phi_sc;

      /* 1187.5 Hz clock */

      clock_phi = subcarr_phi / 48.0 + clock_offset;
      lo_clock  = (fmod(clock_phi, 2*M_PI) < M_PI ? 1 : -1);
      //lo_clock = sin(clock_phi);

#ifdef DEBUG
      outbuf[0] = lo_clock * 6000;
      fwrite(outbuf, sizeof(int16_t), 1, U);
      outbuf[0] = subcarr_bb[0] * 32000;
      fwrite(outbuf, sizeof(int16_t), 1, IQ);
      outbuf[0] = subcarr_bb[1] * 32000;
      fwrite(outbuf, sizeof(int16_t), 1, IQ);
#endif

      /* Clock phase recovery */

      if (sign(prev_bb) != sign(subcarr_bb[0])) {
        d_cphi = fmod(clock_phi, M_PI);
        if (d_cphi >= M_PI_2) d_cphi -= M_PI;
        clock_offset -= 0.005 * d_cphi;
      }

#ifdef DEBUG
      outbuf[0] = acc * 800;
      fwrite(outbuf, sizeof(int16_t), 1, U);
      sbit = 0;
#endif

      /* Decimate band-limited signal */
      if (numsamples % 2 == 0) {

        /* biphase symbol integrate & dump */
        //acc += atan2(subcarr_bb[1], subcarr_bb[0]) * lo_clock;
        acc += subcarr_bb[0] * lo_clock;

        if (sign(lo_clock) != sign(prevclock)) {
          biphase(acc);
          acc = 0;
        }

        prevclock = lo_clock;
      }

      numsamples ++;

#ifdef DEBUG
      outbuf[0] = dbit * 16000;
      fwrite(outbuf, sizeof(int16_t), 1, U);
      outbuf[0] = sbit * 16000;
      fwrite(outbuf, sizeof(int16_t), 1, U);
      t += 1.0/FS;
      if (numsamples % 125 == 0)
        fprintf(STATS,"%f,%f,%f,%f,%f\n",
            t,fsc,d_phi_sc,clock_offset,qua);
#endif

      prev_bb = subcarr_bb[0];

    }
  }
}
