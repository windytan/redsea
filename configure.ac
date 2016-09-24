AC_PREREQ([2.69])
AC_INIT([redsea], [0.7.3], [oona@kapsi.fi], [redsea],
        [https://github.com/windytan/redsea])
AM_INIT_AUTOMAKE([1.10 -Wall -Werror foreign])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([
 Makefile
 src/Makefile
])

AC_ARG_WITH([liquid],
            [AS_HELP_STRING([--without-liquid],
              [disable support for demodulation using liquid-dsp])],
            [],
            [with_liquid=yes])

LIQUID=
AS_IF([test "x$with_liquid" != xno],
  [AC_CHECK_LIB([liquid], [main],
    [AC_SUBST([LIQUID], ["-lliquid"])
     AC_DEFINE([HAVE_LIQUID], [1],
               [Define if you have liquid])
    ],
    [AC_MSG_FAILURE(
       [Could not find liquid-dsp (--without-liquid to disable)])])])

AC_PROG_CXX
AC_PROG_RANLIB
AC_LANG([C++])
AX_CXX_COMPILE_STDCXX_11(,[mandatory])
CXXFLAGS='-Wall -std=c++11'

AC_OUTPUT
