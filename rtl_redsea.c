/* Redsea RDS recoder
 * Oona Räisänen OH2EIQ */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#define FS   250000.0
#define FC_0 57000.0
#define BUFS 1024

#ifdef DEBUG
char dbit,sbit;
int tot_errs[2];
int reading_frame;
double fc;
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

double filter_bp_57k(double input) {

  /* Digital filter designed by mkfilter/mkshape/gencode   A.J. Fisher
   *    Command line: mkfilter -Bu -Bp -o 5 -a 2.1200000000e-01
   *                  2.4400000000e-01 -l
   */

  static double gain = 1.326631022e+05;
  static double xv[10+1], yv[10+1];

  xv[0] = xv[1]; xv[1] = xv[2]; xv[2] = xv[3]; xv[3] = xv[4]; xv[4] = xv[5];
  xv[5] = xv[6]; xv[6] = xv[7]; xv[7] = xv[8]; xv[8] = xv[9]; xv[9] = xv[10];
  xv[10] = input / gain;
  yv[0] = yv[1]; yv[1] = yv[2]; yv[2] = yv[3]; yv[3] = yv[4]; yv[4] = yv[5];
  yv[5] = yv[6]; yv[6] = yv[7]; yv[7] = yv[8]; yv[8] = yv[9]; yv[9] = yv[10];
  yv[10] =   (xv[10] - xv[0]) + 5 * (xv[2] - xv[8]) + 10 * (xv[6] - xv[4])
               + ( -0.5209978985 * yv[0]) + (  0.7684509019 * yv[1])
               + ( -3.3976584040 * yv[2]) + (  3.6145943934 * yv[3])
               + ( -8.2427299426 * yv[4]) + (  6.2405088675 * yv[5])
               + ( -9.3877192266 * yv[6]) + (  4.6902163298 * yv[7])
               + ( -5.0210158398 * yv[8]) + (  1.2948307430 * yv[9]);
  return yv[10];
}

double filter_lp_2400_iq(double input, int iq) {

  /* Digital filter designed by mkfilter/mkshape/gencode   A.J. Fisher
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
               + (  0.8498599655 * yv[iq][0]) + ( -4.3875359464 * yv[iq][1])
               + (  9.0628533836 * yv[iq][2]) + ( -9.3625201736 * yv[iq][3])
               + (  4.8373424748 * yv[iq][4]);
  return yv[iq][5];
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
    double qua = (1.0 * abs(tot_errs[0] - tot_errs[1]) / (tot_errs[0] + tot_errs[1])) * 100;
    fprintf(stderr, "qual: %3.0f%%  pll: %.1f Hz\n", qua, fc);
#endif
    tot_errs[0] = 0;
    tot_errs[1] = 0;
  }

  prev_acc = acc;
  counter = (counter + 1) % 800;
}

int main(int argc, char **argv) {

  int16_t  sample[BUFS];
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
  double prevdemod    = 0;
  double acc          = 0;
  double prevsample   = 0;
  double sample_f     = 0;
  double demod[2]     = {0};
  int    c;
  int    fmfreq       = 0;
  int    bytesread;



#ifdef DEBUG
  sbit = 0;
  dbit = 0;
  reading_frame = 0;
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
  U = popen("sox -c 5 -r 250000 -t .s16 - dbg-out.wav", "w");
  FILE *IQ;
  IQ = popen("sox -c 4 -r 250000 -t .s16 - dbg-out-iq.wav", "w");
  FILE *RAW;
  RAW = popen("sox -c 1 -r 250000 -t .s16 - dbg-out-raw.wav", "w");
#endif

  while (1) {
    bytesread = fread(sample, sizeof(int16_t), BUFS, stdin);
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

#ifdef DEBUG
      outbuf[0] = lo_clock * 16000;
      fwrite(outbuf, sizeof(int16_t), 1, U);
#endif

      /* Subcarrier band-pass */
      sample_f = filter_bp_57k(sample[i] / 32768.0);

      /* DSB demodulate */
      demod[0] = (sample_f * lo_iq[0]);
      demod[1] = (sample_f * lo_iq[1]);

#ifdef DEBUG
      outbuf[0] = sample[i];
      fwrite(outbuf, sizeof(int16_t), 1, RAW);
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


      /* refine sampling instant */
      if (prevdemod * demod[0] <= 0) {
        d_phi = fmod(clock_phi, M_PI);
        if (d_phi >= M_PI_2) d_phi -= M_PI;
        clock_offset -= 0.01 * d_phi;
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

      /* PLL */
      double pll_alpha = 0.00005;
      double pll_beta = sqrt(pll_alpha);
      double zc;

      if (sign(sample_f) != sign(prevsample)) {
        zc = sample_f / (prevsample - sample_f) / FS;
        d_phi = fmod(lo_phi, 2 * M_PI) - (zc * fc * 2 * M_PI);
        if (d_phi >= M_PI)  d_phi = -2 * M_PI + d_phi;
        if (d_phi <= -M_PI) d_phi =  2 * M_PI - d_phi;
        fc     -= pll_alpha * d_phi;
        lo_phi -= pll_beta  * d_phi;
      }

#ifdef DEBUG
      outbuf[0] = reading_frame * 16000;
      fwrite(outbuf, sizeof(int16_t), 1, U);
#endif

      /* For zero-crossing detection */
      prevdemod  = demod[0];
      prevclock  = lo_clock;
      prevsample = sample_f;
    }

  }
}
