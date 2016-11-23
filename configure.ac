AC_PREREQ([2.69])
AC_INIT([redsea], [0.8.0], [oona@kapsi.fi], [redsea],
        [https://github.com/windytan/redsea])
AM_INIT_AUTOMAKE([1.10 -Wall -Werror foreign subdir-objects])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([
 Makefile
 src/Makefile
])

AC_ARG_WITH([liquid],
            [AS_HELP_STRING([--without-liquid],
              [disable support for demodulation using liquid-dsp])])

AC_ARG_ENABLE([tmc],
            [AS_HELP_STRING([--disable-tmc],
              [disable support for Traffic Message Channel (TMC)])])

AS_IF([test "x$enable_tmc" != xno],
  [AC_DEFINE([ENABLE_TMC], [1],
             [Define to compile TMC])
  ])

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
#AX_CXX_COMPILE_STDCXX_11(,[mandatory])
#CXXFLAGS='-Wall -std=gnu++11'

AC_OUTPUT
