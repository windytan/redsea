bin_PROGRAMS = redsea
redsea_CPPFLAGS = -Wall -Wextra -Wstrict-overflow -Wshadow -Wuninitialized -pedantic $(DBG_FLAGS)
#redsea_LDADD = $(GTKMM_LIBS) @SNDFILE_LIBS@ -lfftw3 $(PORTAUDIO_LIBS)
redsea_LDADD = -L. -lwdsp
redsea_SOURCES = redsea.cc filters.cc blockstream.cc bitstream.cc groups.cc
