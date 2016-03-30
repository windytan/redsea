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

#include "filters.h"

#include <complex>

std::complex<double> filter_lp_2400_iq(std::complex<double> input) {

  /* Digital filter designed by mkfilter/mkshape/gencode A.J. Fisher
     Command line: /www/usr/fisher/helpers/mkfilter -Bu -Lp -o 10
     -a 4.8000000000e-03 0.0000000000e+00 -l */

  static std::complex<double> xv[2+1], yv[2+1];

  xv[0] = xv[1]; xv[1] = xv[2];
  xv[2] = input / 4.491730007e+03;
  yv[0] = yv[1]; yv[1] = yv[2];
  yv[2] =   (xv[0] + xv[2]) + 2.0 * xv[1]
    + ( -0.9582451124 * yv[0]) + (  1.9573545869 * yv[1]);
  return yv[2];
}

double filter_lp_pll(double input) {

  static double xv[1+1], yv[1+1];

  xv[0] = xv[1];
  xv[1] = input / 3.716236217e+01;
  yv[0] = yv[1];
  yv[1] =   (xv[0] + xv[1])
    + (  0.9461821078 * yv[0]);
  return yv[1];
}
