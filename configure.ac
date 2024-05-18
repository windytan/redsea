AC_PREREQ([2.69])
AC_INIT([redsea], [0.22-SNAPSHOT], [oona@kapsi.fi], [redsea],
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

AC_ARG_WITH([macports],
            [AS_HELP_STRING([--without-macports],
               [disable check for MacPorts directories when compiling])])

AC_ARG_ENABLE([tmc],
            [AS_HELP_STRING([--disable-tmc],
              [disable support for Traffic Message Channel (TMC)])])

AS_IF([test "x$enable_tmc" != xno],
  [AC_DEFINE([ENABLE_TMC], [1],
             [Define to compile TMC])
  ])

LIQUID=
AS_IF([test "x$with_liquid" != xno],
  [AC_CHECK_LIB([liquid], [symtrack_cccf_create],
    [AC_SUBST([LIQUID], ["-lliquid"])
     AC_DEFINE([HAVE_LIQUID], [1],
               [Define if you have liquid])
    ],
    [AC_MSG_FAILURE(
       [Could not find liquid-dsp (use --without-liquid to disable)])])])

AS_IF([test "x$with_liquid" != xno],
  [AC_CHECK_LIB([liquid], [modemcf],
    [AC_DEFINE([MODEM_IS_MODEMCF], [1],
               [modem is called modemcf])])])

MACPORTS_LD=
AS_IF([test "x$with_macports" != xno],
  [AC_CHECK_FILE(/opt/local/lib,
    [AC_SUBST([MACPORTS_LD], ["-L/opt/local/lib"])])])

MACPORTS_CF=
AS_IF([test "x$with_macports" != xno],
  [AC_CHECK_FILE(/opt/local/lib,
    [AC_SUBST([MACPORTS_CF], ["-I/opt/local/include"])])])

AC_CHECK_LIB([sndfile], [main],
  [AC_SUBST([SNDFILE], ["-lsndfile"])
   AC_DEFINE([HAVE_SNDFILE], [1],
             [Define if you have libsndfile])
  ],
  [AC_MSG_FAILURE(
     [Could not find libsndfile])])

WIN_CF=
LIBC=

AC_MSG_CHECKING([whether compiling for Windows])
AC_COMPILE_IFELSE([AC_LANG_SOURCE([[
#if !defined(_WIN32) && !defined(_WIN64)
#error not compiling for Windows
#endif
]])], 
  # Windows
  [AC_MSG_RESULT([yes]); AC_SUBST([WIN_CF], ["-D_USE_MATH_DEFINES=1"])],
  # Unix
  [AC_MSG_RESULT([no]); AC_SUBST([LIBC], ["-lc"])])

CPP_STD=c++14
AC_CANONICAL_HOST
case "${host_os}" in
  darwin*|msys*|mingw*)
      AC_SUBST([ICONV], ["-liconv"])
      ;;
  cygwin*)
      AC_SUBST([ICONV], ["-liconv"])
      AC_SUBST([CPP_STD], ["gnu++14"])
      ;;
esac

AC_PROG_CXX
AC_PROG_RANLIB
AC_LANG([C++])

AC_OUTPUT
