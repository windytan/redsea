AC_PREREQ([2.69])
AC_INIT([redsea], [0.7-dev], [oona@kapsi.fi])
AM_INIT_AUTOMAKE([1.10 -Wall -Werror foreign])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([
 Makefile
 src/Makefile
])

AC_PROG_CXX
AC_PROG_RANLIB
AC_LANG([C++])
AX_CXX_COMPILE_STDCXX_11(,[mandatory])
CXXFLAGS='-Wall -std=c++11'

AC_OUTPUT
