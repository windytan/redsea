/* Part of redsea RDS decoder */

#ifndef FILTERS_H
#define FILTERS_H

#include <complex>

double filter_bp_57k(double input);
double filter_bp_19k(double input);
std::complex<double> filter_lp_2400_iq(std::complex<double> input);
double filter_lp_pll(double input);

#endif
