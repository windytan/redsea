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
double fc;
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
        reading_frame, tot_errs[0], tot_errs[1], qua, fc);
#endif
    tot_errs[0] = 0;
    tot_errs[1] = 0;
  }

  prev_acc = acc;
  counter = (counter + 1) % 800;
}

int main(int argc, char **argv) {

  int16_t  sample[IBUFLEN];
#ifdef DEBUG
  fc = FC_0;
#else
  double fc = FC_0;
#endif

  double subcarr_phi    = 0;
  double subcarr_bb[2]  = {0};
  double clock_offset   = 0;
  double clock_phi      = 0;
  double lo_clock       = 0;
  double prevclock      = 0;
  double prev_bb        = 0;
  double d_phi_sc       = 0;
  double d_pphi         = 0;
  double d_cphi         = 0;
  double acc            = 0;
  double subcarr_sample = 0;
  double fp             = 19000;
  double pilot_sample;
  double pilot_phi      = 0;
  int    c;
  int    fmfreq         = 0;
  int    bytesread;

#ifdef DEBUG
  sbit = 0;
  dbit = 0;
  reading_frame = 0;
  qua = 0;
  double t = 0;
  int numsamples = 0;
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
  fprintf(STATS, "t,fp,d_pphi,qual\n");
#endif

  while (1) {
    bytesread = fread(sample, sizeof(int16_t), IBUFLEN, stdin);
    if (bytesread < 1) exit(0);

    int i;
    for (i = 0; i < bytesread; i++) {


      /* Pilot tone recovery */

      pilot_sample      = filter_bp_19k(sample[i] / 32768.0);
      double pilot_bb_i, pilot_bb_q;

      pilot_phi  += 2 * M_PI * fp * (1.0/FS);
      pilot_bb_i  = filter_lp_pll(cos(pilot_phi) * pilot_sample, 0);
      pilot_bb_q  = filter_lp_pll(sin(pilot_phi) * pilot_sample, 1);

      double ppll_alpha = 0.0001;
      double ppll_beta  = sqrt(ppll_alpha) * .01;
      d_pphi      = atan2(pilot_bb_q, pilot_bb_i);
      pilot_phi  -= ppll_beta  * d_pphi;
      fp         -= ppll_alpha * d_pphi;
      fc          = fp * 3;


      /* Subcarrier downmix & phase recovery */

      subcarr_phi   += 2 * M_PI * fc * (1.0/FS);
      subcarr_sample = filter_bp_57k(sample[i] / 32768.0);
      subcarr_bb[0]  = filter_lp_2400_iq(subcarr_sample * cos(subcarr_phi), 0);
      subcarr_bb[1]  = filter_lp_2400_iq(subcarr_sample * sin(subcarr_phi), 1);

      double pll_beta = 0.00001;

      d_phi_sc     = atan2(subcarr_bb[1], subcarr_bb[0]) + M_PI;
      d_phi_sc     = fmod(d_phi_sc, M_PI) - M_PI_2;
      subcarr_phi -= pll_beta * d_phi_sc;

      /* 1187.5 Hz clock */

      clock_phi = subcarr_phi / 48 + clock_offset;
      lo_clock  = (fmod(clock_phi, 2*M_PI) >= M_PI ? 1 : -1);

#ifdef DEBUG
      outbuf[0] = lo_clock * 16000;
      fwrite(outbuf, sizeof(int16_t), 1, U);
      outbuf[0] = subcarr_bb[0] * 32000;
      fwrite(outbuf, sizeof(int16_t), 1, IQ);
      outbuf[0] = subcarr_bb[1] * 32000;
      fwrite(outbuf, sizeof(int16_t), 1, IQ);
#endif

      /* Clock phase recovery */

      if (sign(prev_bb) != sign(subcarr_bb[1])) {
        d_cphi = fmod(clock_phi, M_PI);
        if (d_cphi >= M_PI_2) d_cphi -= M_PI;
        clock_offset -= 0.001 * d_cphi;
      }

      /* biphase symbol integrate & dump */
      acc += subcarr_bb[1] * lo_clock;

#ifdef DEBUG
      outbuf[0] = acc * 800;
      fwrite(outbuf, sizeof(int16_t), 1, U);
      sbit = 0;
#endif

      if (sign(lo_clock) != sign(prevclock)) {
        biphase(acc);
        acc = 0;
      }

#ifdef DEBUG
      outbuf[0] = dbit * 16000;
      fwrite(outbuf, sizeof(int16_t), 1, U);
      outbuf[0] = sbit * 16000;
      fwrite(outbuf, sizeof(int16_t), 1, U);
      t += 1.0/FS;
      numsamples ++;
      if (numsamples % 125 == 0)
        fprintf(STATS,"%f,%f,%f,%f,%f,%f\n",
            t,fp,d_pphi,d_phi_sc,clock_offset,qua);
#endif

      prevclock = lo_clock;
      prev_bb = subcarr_bb[1];

    }
  }
}
