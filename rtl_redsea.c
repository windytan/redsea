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

#define FS     250000.0
#define FC_0   57000.0
#define BUFLEN 1024

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
  if (nbit % 104 == 0)
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
    fprintf(stderr, "qual: %3.0f%%  pll: %.1f Hz\n", qua, fc);
#endif
    tot_errs[0] = 0;
    tot_errs[1] = 0;
  }

  prev_acc = acc;
  counter = (counter + 1) % 800;
}

int main(int argc, char **argv) {

  int16_t  sample[BUFLEN];
#ifdef DEBUG
  fc = FC_0;
#else
  double fc = FC_0;
#endif

  double lo_phi       = 0;
  double lo_iq[2]     = {0};
  double clock_offset = 0;
  double clock_phi    = 0;
  double lo_clock     = 0;
  double prevclock    = 0;
  double d_phi        = 0;
  double d_pphi       = 0;
  double d_cphi       = 0;
  double prevdemod    = 0;
  double acc          = 0;
  double prevsample   = 0;
  double sample_f     = 0;
  double demod[2]     = {0};
  double fp           = 19000;
  double pilot;
  double pilot_phi    = 0;
  double pilot_lo;
  double prev_pilot   = 0;
  int    c;
  int    fmfreq       = 0;
  int    bytesread;

#ifdef DEBUG
  sbit = 0;
  dbit = 0;
  reading_frame = 0;
  qua = 0;
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
  float foutbuf[1];
  FILE *U;
  U = popen("sox -c 6 -r 250000 -t .s16 - dbg-out.wav", "w");
  FILE *IQ;
  IQ = popen("sox -c 4 -r 250000 -t .s16 - dbg-out-iq.wav", "w");
  FILE *RAW;
  RAW = popen("sox -c 3 -r 250000 -t .f32 - dbg-out-pilot.wav", "w");
#endif

  while (1) {
    bytesread = fread(sample, sizeof(int16_t), BUFLEN, stdin);
    if (bytesread < 1) exit(0);

    int i;
    for (i = 0; i < bytesread; i++) {

      /* 57 kHz local oscillator */
      lo_phi += 2 * M_PI * fc * (1.0/FS);
      lo_iq[0] = sin(lo_phi);
      lo_iq[1] = cos(lo_phi);

      /* 1187.5 Hz clock */
      clock_phi = lo_phi / 48 + clock_offset;
      lo_clock  = (fmod(clock_phi, 2*M_PI) >= M_PI ? 1 : -1);

      /* Subcarrier band-pass */
      sample_f = filter_bp_57k(sample[i] / 32768.0);

#ifdef DEBUG
      outbuf[0] = lo_iq[0] * 16000;
      fwrite(outbuf, sizeof(int16_t), 1, U);
      outbuf[0] = sample_f * 16000;
      fwrite(outbuf, sizeof(int16_t), 1, U);
#endif

      /* DSB demodulate */
      demod[0] = (sample_f * lo_iq[0]);
      demod[1] = (sample_f * lo_iq[1]);

      pilot = filter_bp_19k(sample[i] / 32768.0);
      double ppll_alpha = 0.000001;
      double ppll_beta = sqrt(ppll_alpha);
      double pzc=0;

      pilot_phi += 2 * M_PI * fp * (1.0/FS);
      pilot_lo = sin(pilot_phi);
      if (sign(prev_pilot) != sign(pilot)) {
        pzc = pilot / (prev_pilot - pilot) / FS;
        d_pphi = fmod(pilot_phi, 2 * M_PI) + (pzc * fp * 2 * M_PI)
          - (pilot < 0 ? M_PI : 0);
        if (d_pphi >= M_PI) d_pphi -= 2*M_PI;
      }

      fp        -= ppll_alpha * d_pphi;
      pilot_phi -= ppll_beta  * d_pphi;
      fc         = fp * 3;
      prev_pilot = pilot;



#ifdef DEBUG
      foutbuf[0] = pilot;
      fwrite(foutbuf, sizeof(float), 1, RAW);
      foutbuf[0] = pilot_lo;
      fwrite(foutbuf, sizeof(float), 1, RAW);
      foutbuf[0] = d_pphi / M_PI;
      fwrite(foutbuf, sizeof(float), 1, RAW);
      outbuf[0] = demod[0] * 32000;
      fwrite(outbuf, sizeof(int16_t), 1, IQ);
      outbuf[0] = demod[1] * 32000;
      fwrite(outbuf, sizeof(int16_t), 1, IQ);
#endif

      /* Anti-alias & data-shaping low-pass */
      demod[0] = filter_lp_2400_iq(demod[0], 0);
      demod[1] = filter_lp_2400_iq(demod[1], 1);

#ifdef DEBUG
      outbuf[0] = demod[0] * 32000;
      fwrite(outbuf, sizeof(int16_t), 1, IQ);
      outbuf[0] = demod[1] * 32000;
      fwrite(outbuf, sizeof(int16_t), 1, IQ);
#endif


      /* Clock phase recovery */
      if (prevdemod * demod[0] <= 0) {
        d_cphi = fmod(clock_phi, M_PI);
        if (d_cphi >= M_PI_2) d_cphi -= M_PI;
        clock_offset -= 0.01 * d_cphi;
      }

      /* biphase symbol integrate & dump */
      acc += demod[0] * lo_clock;

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
#endif

      /* Subcarrier phase recovery */
      double pll_alpha = 0.00002;
      double pll_beta = sqrt(pll_alpha);
      double zc;

      if (sign(sample_f) != sign(prevsample)) {
        zc = sample_f / (prevsample - sample_f) / FS;
        d_phi = fmod(lo_phi + zc * fc * 2 * M_PI, M_PI);
        if (d_phi >= M_PI_2)  d_phi -= M_PI;
        //fc     -= pll_alpha * d_phi;
      }
      lo_phi -= pll_beta  * d_phi;

#ifdef DEBUG
      outbuf[0] = d_phi / M_PI * 32768;
      fwrite(outbuf, sizeof(int16_t), 1, U);
#endif

      /* For zero-crossing detection */
      prevdemod  = demod[0];
      prevclock  = lo_clock;
      prevsample = sample_f;
    }

  }
}
