/* Redsea RDS recoder
 * Oona Räisänen OH2EIQ */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define FS   250000.0
#define FC_0 57000.0

#ifdef DEBUG
char dbit,sbit;
int errs;
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
  static double prevacc = 0;
  static int    counter = 0;
#ifndef DEBUG
  static int    errs    = 0;
#endif

  counter ++;
  if (counter > 1) {
    if (sign(acc) == sign(prevacc)) {
      print_delta(sign(acc));
      counter = 0;
      errs    = 0;
   } else {
      // tolerate 5 noisy symbols
      print_delta(sign(acc + prevacc));
      if (errs < 5) counter = 0;
      errs++;
    }
  }
  prevacc = acc;
}

int main(int argc, char **argv) {

  int16_t sample[1];
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
  double prevval = 0;

  double xv[2][4] = {{0}};
  double yv[2][4] = {{0}};
  double demod[2] = {0};
  double filtd[2] = {0};

  double lpf_gain = 6.605354574;

#ifdef DEBUG
  sbit = 0;
  dbit=0;
  errs = 0;
#endif

  int c;
  int fmfreq = 0;

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
  U = popen("sox -c 5 -r 250000 -t .s16 - dbg-out.wav", "w");
  FILE *IQ;
  IQ = popen("sox -c 2 -r 250000 -t .s16 - dbg-out-iq.wav", "w");
#endif

  while (1) {
    if (fread(sample, sizeof(int16_t), 1, stdin) == 0) exit(0);
    val = sample[0] / 32768.0;

    /* 57 kHz local oscillator */
    lo_phi += 2 * M_PI * fc * (1.0/FS);
    lo_iq[0] = sin(lo_phi);
    lo_iq[1] = cos(lo_phi);

    /* 1187.5 Hz clock */
    clock_phi = lo_phi / 48 + clock_offset;
    lo_clock = (fmod(clock_phi, 2*M_PI) >= M_PI ? 1 : -1);

#ifdef DEBUG
    outbuf[0] = lo_clock * 16000;
    fwrite(outbuf, sizeof(int16_t), 1, U);
#endif

    /* DSB demodulate */
    demod[0] = (val * lo_iq[0]);
    demod[1] = (val * lo_iq[1]);

    /* Butterworth lopass */
    for (int iq=0;iq<=1;iq++) {
      xv[iq][0] = xv[iq][1]; xv[iq][1] = xv[iq][2]; xv[iq][2] = xv[iq][3];
      xv[iq][3] = demod[iq] / lpf_gain;
      yv[iq][0] = yv[iq][1]; yv[iq][1] = yv[iq][2]; yv[iq][2] = yv[iq][3];
      yv[iq][3] =   (xv[iq][0] + xv[iq][3]) + 3 * (xv[iq][1] + xv[iq][2])
                   + (  0.0105030013 * yv[iq][0]) + ( -0.3368436806 * yv[iq][1])
                   + (  0.1152020629 * yv[iq][2]);
      filtd[iq] = yv[iq][3];
    }

#ifdef DEBUG
    outbuf[0] = filtd[0] * 16000;
    fwrite(outbuf, sizeof(int16_t), 1, IQ);
    outbuf[0] = filtd[1] * 16000;
    fwrite(outbuf, sizeof(int16_t), 1, IQ);
#endif

    /* refine sampling instant */
    if (prevdemod * filtd[0] <= 0) {
      d_phi = fmod(clock_phi, M_PI);
      if (d_phi >= M_PI_2) d_phi -= M_PI;
      clock_offset -= 0.01 * d_phi;
    }

    /* biphase symbol integrate & dump */
    acc += filtd[0] * lo_clock;

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

    /* PLL */
    double pll_alpha = 0.0002;
    double pll_beta = sqrt(pll_alpha);
    double zc;

    if (sign(val) != sign(prevval)) {
      zc = val / (prevval - val) / FS;
      d_phi = fmod(lo_phi, 2 * M_PI) - (zc * fc * 2 * M_PI);
      if (d_phi >= M_PI)  d_phi = -2 * M_PI + d_phi;
      if (d_phi <= -M_PI) d_phi =  2 * M_PI - d_phi;
      fc     -= pll_alpha * d_phi;
      lo_phi -= pll_beta  * d_phi;
    }

#ifdef DEBUG
    outbuf[0] = (errs > 5 ? 1 : 0) * 16000;
    fwrite(outbuf, sizeof(int16_t), 1, U);
#endif

    /* For zero-crossing detection */
    prevdemod = filtd[0];
    prevclock = lo_clock;
    prevval   = val;

  }
}
