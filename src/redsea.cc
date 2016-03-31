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

#include <complex>

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

int main() {

  double subcarr_phi  = 0;
  double clock_offset = 0;
  double prevclock    = 0;
  double prev_bb      = 0;
  double acc          = 0;
  int    numsamples   = 0;
  double fsc          = FC_0;

  while (true) {
    int16_t sample[IBUFLEN];
    int bytesread = fread(sample, sizeof(int16_t), IBUFLEN, stdin);
    if (bytesread < 1) exit(0);

    for (int i = 0; i < bytesread; i++) {

      /* Subcarrier downmix & phase recovery */

      subcarr_phi    += 2 * M_PI * fsc * (1.0/FS);
      std::complex<double> subcarr_bb =
        filter_lp_2400_iq(sample[i] / 32768.0 * std::polar(1.0, subcarr_phi));

      double pll_beta = 50;

      double d_phi_sc = 2.0*filter_lp_pll(real(subcarr_bb) * imag(subcarr_bb));
      subcarr_phi -= pll_beta * d_phi_sc;
      fsc         -= .5 * pll_beta * d_phi_sc;

      /* 1187.5 Hz clock */

      double clock_phi = subcarr_phi / 48.0 + clock_offset;
      double lo_clock  = (fmod(clock_phi, 2*M_PI) < M_PI ? 1 : -1);

      /* Clock phase recovery */

      if (sign(prev_bb) != sign(real(subcarr_bb))) {
        double d_cphi = fmod(clock_phi, M_PI);
        if (d_cphi >= M_PI_2) d_cphi -= M_PI;
        clock_offset -= 0.005 * d_cphi;
      }

      /* Decimate band-limited signal */
      if (numsamples % 8 == 0) {

        /* biphase symbol integrate & dump */
        acc += real(subcarr_bb) * lo_clock;

        if (sign(lo_clock) != sign(prevclock)) {
          biphase(acc);
          acc = 0;
        }

        prevclock = lo_clock;
      }

      numsamples ++;

      prev_bb = real(subcarr_bb);

    }
  }
}
