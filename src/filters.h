/* Part of redsea RDS decoder */

#ifndef FILTERS_H
#define FILTERS_H

#include <complex>

std::complex<double> filter_lp_2400_iq(std::complex<double> input);
double filter_lp_pll(double input);

#endif
