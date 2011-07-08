/* downmix.c -- part of redsea RDS decoder (c) OH2-250
 *
 * 19 kHz downconverter
 * PCM is 48k 16bit single-channel little-endian signed-integer
 *
 * Creates heterodynes of the RDS signal; result needs to be downpass filtered
 *
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>

#define BUFLEN 2048

int main() {

  unsigned int bufptr = 0;
  double       freq   = 2 * M_PI * (19000.0 / 48000.0); // radians per sample
  double       phase  = -M_PI;
  short int    pcm;
  short int    outbuf[BUFLEN];

  /* Read PCM data from stdin */
  while (read(0, &pcm, 2)) {

    /* Downmix */
    outbuf[bufptr] = pcm * cos(phase) + .5;
    
    /* Advance local oscillator phase */
    phase += freq;
    if (phase >= M_PI) phase -= 2 * M_PI;

    /* Output PCM when buffer is full */
    if (++bufptr == BUFLEN) {
      if (!write(1, &outbuf, 2 * BUFLEN)) return (EXIT_FAILURE);
      fflush(stdout);
      bufptr = 0;
    }

  }

  return (EXIT_SUCCESS);
}
