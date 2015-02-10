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
#endif

void bit(char b) {
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

void biphase(double acc) {
  static double prevacc = 0;
  static int counter = 0;
  static int errs = 0;
  counter ++;
  if (counter > 1) {
    if (acc >= 0 && prevacc >= 0) {
      bit(1);
      counter = 0;
      errs = 0;
    } else if (acc < 0 && prevacc < 0) {
      bit(0);
      counter = 0;
      errs = 0;
    } else {
      // tolerate 1 erroneous symbol
      bit(acc + prevacc >= 0 ? 1 : 0);
      if (errs == 0) counter = 0;
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

  double xv[2][4] = {{0}};
  double yv[2][4] = {{0}};
  double demod[2] = {0};
  double filtd[2] = {0};

  double gain = 6.605354574;

#ifdef DEBUG
  sbit = 0;
  dbit=0;
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
  U = popen("sox -c 6 -r 250000 -t .s16 - dbg-out.wav", "w");
#endif

  while (1) {
    if (fread(sample, sizeof(int16_t), 1, stdin) == 0) exit(0);
    val = sample[0] / 32768.0;

#ifdef DEBUG
    outbuf[0] = sample[0];
    fwrite(outbuf, sizeof(int16_t), 1, U);
#endif

    /* 57 kHz local oscillator */
    lo_phi += 2 * M_PI * fc * (1.0/FS);
    lo_iq[0] = cos(lo_phi);
    lo_iq[1] = sin(lo_phi);

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
      xv[iq][3] = demod[iq] / gain;
      yv[iq][0] = yv[iq][1]; yv[iq][1] = yv[iq][2]; yv[iq][2] = yv[iq][3];
      yv[iq][3] =   (xv[iq][0] + xv[iq][3]) + 3 * (xv[iq][1] + xv[iq][2])
                   + (  0.0105030013 * yv[iq][0]) + ( -0.3368436806 * yv[iq][1])
                   + (  0.1152020629 * yv[iq][2]);
      filtd[iq] = yv[iq][3];
    }

#ifdef DEBUG
    outbuf[0] = filtd[0] * 16000;
    fwrite(outbuf, sizeof(int16_t), 1, U);
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

    if (prevclock * lo_clock <= 0) {
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
    at = atan2(-filtd[1], filtd[0]);
    if (at > M_PI_2)  at -= M_PI;
    if (at < -M_PI_2) at += M_PI;
    fc += 0.002 * at;
    if (fc > FC_0 + 20) fc -= 30;
    if (fc < FC_0 - 20) fc += 30;

    /* For zero-crossing detection */
    prevdemod = filtd[0];
    prevclock = lo_clock;

  }
}
