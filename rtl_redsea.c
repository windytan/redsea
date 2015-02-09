/* Redsea RDS recoder
 * Oona Räisänen OH2EIQ */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define FS   250000.0
#define FC_0 57000.0

int nbit;

void bit(char b) {
  printf("%d", b);
  if (nbit % 104 == 0)
    fflush(0);
  nbit++;
}

int main(int argc, char **argv) {

  char commd[1024];
  int16_t sample[1];
  int16_t outbuf[2];
  double val = 0;
  double fc = FC_0;

  double lo_phi = 0;
  double lo_iq[2] = {0};
  double clock_offset = 0;
  double clock_phi = 0;
  double lo_clock = 0;
  double prevclock = 0;
  double d_phi = 0;
  double prevdemod = 0;
  double acc = 0;
  double at = 0;

  double xv[2][4] = {{0}};
  double yv[2][4] = {{0}};
  double demod[2] = {0};
  double filtd[2] = {0};

  double gain = 6.605354574;

  char dbit=0, prev_dbit=0;

  int c;
  int fmfreq = 0;

  FILE *S;

  nbit = 0;

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

  sprintf(commd, "rtl_fm -f %d -M fm -l 0 -A std -s 250000 | sox -c 1 -t .s16 -r %.0f - -t .s16 - sinc 53000-61000 gain 15 2>/dev/null", FS, fmfreq);
  S = popen(commd, "r");

  while (1) {
    if (fread(sample, sizeof(int16_t), 1, S) == 0) exit(0);
    val = sample[0] / 32768.0;

    /* 57 kHz local oscillator */
    lo_phi += 2 * M_PI * fc * (1.0/FS);
    lo_iq[0] = cos(lo_phi);
    lo_iq[1] = sin(lo_phi);

    /* 1187.5 Hz clock */
    clock_phi = lo_phi / 48 + clock_offset;
    lo_clock = sin(clock_phi);

    /* DSB demod & Butterworth lopass */
    demod[0] = (val * lo_iq[0]);
    demod[1] = (val * lo_iq[1]);
    for (int iq=0;iq<=1;iq++) {
      xv[iq][0] = xv[iq][1]; xv[iq][1] = xv[iq][2]; xv[iq][2] = xv[iq][3];
      xv[iq][3] = demod[iq] / gain;
      yv[iq][0] = yv[iq][1]; yv[iq][1] = yv[iq][2]; yv[iq][2] = yv[iq][3];
      yv[iq][3] =   (xv[iq][0] + xv[iq][3]) + 3 * (xv[iq][1] + xv[iq][2])
                   + (  0.0105030013 * yv[iq][0]) + ( -0.3368436806 * yv[iq][1])
                   + (  0.1152020629 * yv[iq][2]);
      filtd[iq] = yv[iq][3];
    }
    demod[0] = filtd[0];

    /* refine sampling instant */
    if (prevdemod * demod[0] <= 0) {
      d_phi = fmod(clock_phi, 2*M_PI);
      d_phi -= M_PI;
      clock_offset -= 0.01 * d_phi;

      //fprintf(stderr, "%.3f\n",d_phi);
    }

    /* biphase symbol integrate & dump */
    acc += demod[0] * lo_clock;
    if (prevclock < 0 && lo_clock >= 0) {
      dbit = (acc < 0 ? 0 : 1);
      bit(dbit ^ prev_dbit);
      prev_dbit = dbit;
      acc = 0;
    }

    /* PLL */
    at = atan2(-filtd[1], filtd[0]);
    if (at > M_PI_2)  at -= M_PI;
    if (at < -M_PI_2) at += M_PI;
    fc += 0.002 * at;
    if (fc > 57020) fc -= 30;
    if (fc < 56980) fc += 30;

    /* For zero-crossing detection */
    prevdemod = demod[0];
    prevclock = lo_clock;

  }
}
