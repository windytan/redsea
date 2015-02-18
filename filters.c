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

double filter_bp_57k(double input) {

  /* Digital filter designed by mkfilter/mkshape/gencode   A.J. Fisher
     Command line: mkfilter -Bu -Bp -o 5 -a 2.1200000000e-01
                   2.4400000000e-01 -l
   */

  static double gain = 1.326631022e+05;
  static double xv[10+1], yv[10+1];

  xv[0] = xv[1]; xv[1] = xv[2]; xv[2] = xv[3]; xv[3] = xv[4]; xv[4] = xv[5];
  xv[5] = xv[6]; xv[6] = xv[7]; xv[7] = xv[8]; xv[8] = xv[9]; xv[9] = xv[10];
  xv[10] = input / gain;
  yv[0] = yv[1]; yv[1] = yv[2]; yv[2] = yv[3]; yv[3] = yv[4]; yv[4] = yv[5];
  yv[5] = yv[6]; yv[6] = yv[7]; yv[7] = yv[8]; yv[8] = yv[9]; yv[9] = yv[10];
  yv[10] = (xv[10] - xv[0]) + 5 * (xv[2] - xv[8]) + 10 * (xv[6] - xv[4])
             + ( -0.5209978985 * yv[0]) + (  0.7684509019 * yv[1])
             + ( -3.3976584040 * yv[2]) + (  3.6145943934 * yv[3])
             + ( -8.2427299426 * yv[4]) + (  6.2405088675 * yv[5])
             + ( -9.3877192266 * yv[6]) + (  4.6902163298 * yv[7])
             + ( -5.0210158398 * yv[8]) + (  1.2948307430 * yv[9]);
  return yv[10];
}

double filter_bp_19k(double input) {

  /* Digital filter designed by mkfilter/mkshape/gencode   A.J. Fisher
     Command line: mkfilter -Bu -Bp -o 5 -a 7.4000000000e-02
                   7.8000000000e-02 -l
   */

  static double gain = 3.760742395e+05;
  static double xv[10+1], yv[10+1];

  xv[0] = xv[1]; xv[1] = xv[2]; xv[2] = xv[3]; xv[3] = xv[4]; xv[4] = xv[5];
  xv[5] = xv[6]; xv[6] = xv[7]; xv[7] = xv[8]; xv[8] = xv[9]; xv[9] = xv[10];
  xv[10] = input / gain;
  yv[0] = yv[1]; yv[1] = yv[2]; yv[2] = yv[3]; yv[3] = yv[4]; yv[4] = yv[5];
  yv[5] = yv[6]; yv[6] = yv[7]; yv[7] = yv[8]; yv[8] = yv[9]; yv[9] = yv[10];
  yv[10] = (xv[10] - xv[0]) + 5 * (xv[2] - xv[8]) + 10 * (xv[6] - xv[4])
             + ( -0.5372920725 * yv[0]) + (  5.0803085385 * yv[1])
             + (-22.2541223260 * yv[2]) + ( 59.3339705100 * yv[3])
             + (-106.4893684300 * yv[4]) + (134.3324034600 * yv[5])
             + (-120.5923369100 * yv[6]) + ( 76.0883715920 * yv[7])
             + (-32.3147780360 * yv[8]) + (  8.3524517692 * yv[9]);
  return yv[10];
}

double filter_lp_2400_iq(double input, int iq) {

  /* Digital filter designed by mkfilter/mkshape/gencode A.J. Fisher
     Command line: mkfilter -Bu -Lp -o 5 -a 8.0000000000e-03
                   0.0000000000e+00 -l */

  static double gain = 1.080611891e+08;
  static double xv[2][5+1], yv[2][5+1];

  xv[iq][0] = xv[iq][1]; xv[iq][1] = xv[iq][2]; xv[iq][2] = xv[iq][3];
  xv[iq][3] = xv[iq][4]; xv[iq][4] = xv[iq][5];
  xv[iq][5] = input / gain;
  yv[iq][0] = yv[iq][1]; yv[iq][1] = yv[iq][2]; yv[iq][2] = yv[iq][3];
  yv[iq][3] = yv[iq][4]; yv[iq][4] = yv[iq][5];
  yv[iq][5] = (xv[iq][0] + xv[iq][5]) + 5 * (xv[iq][1] + xv[iq][4])
                + 10 * (xv[iq][2] + xv[iq][3])
                + ( 0.8498599655 * yv[iq][0]) + ( -4.3875359464 * yv[iq][1])
                + ( 9.0628533836 * yv[iq][2]) + ( -9.3625201736 * yv[iq][3])
                + ( 4.8373424748 * yv[iq][4]);
  return yv[iq][5];
}

double filter_lp_pll(double input, int iq) {

  /* Digital filter designed by mkfilter/mkshape/gencode A.J. Fisher
     Command line: mkfilter -Bu -Lp -o 5 -a 8.0000000000e-03
                   0.0000000000e+00 -l */

  static double gain = 5.604550418e+09;
  static double xv[2][5+1], yv[2][5+1];

  xv[iq][0] = xv[iq][1]; xv[iq][1] = xv[iq][2]; xv[iq][2] = xv[iq][3];
  xv[iq][3] = xv[iq][4]; xv[iq][4] = xv[iq][5];
  xv[iq][5] = input / gain;
  yv[iq][0] = yv[iq][1]; yv[iq][1] = yv[iq][2]; yv[iq][2] = yv[iq][3];
  yv[iq][3] = yv[iq][4]; yv[iq][4] = yv[iq][5];
  yv[iq][5] = (xv[iq][0] + xv[iq][5]) + 5 * (xv[iq][1] + xv[iq][4])
                + 10 * (xv[iq][2] + xv[iq][3])
                + (  0.9294148707 * yv[iq][0]) + ( -4.7151053641 * yv[iq][1])
                + (  9.5687692520 * yv[iq][2]) + ( -9.7098810864 * yv[iq][3])
                + (  4.9268023221 * yv[iq][4]);
  return yv[iq][5];
}
