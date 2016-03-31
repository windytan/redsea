#ifndef FILTERS_H_
#define FILTERS_H_

#include <complex>

std::complex<double> filter_lp_2400_iq(std::complex<double> input);
double filter_lp_pll(double input);

#endif
