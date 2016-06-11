bin_PROGRAMS = redsea
redsea_CPPFLAGS = -Wall -Wextra -Wstrict-overflow -Wshadow -Wuninitialized -pedantic $(DBG_FLAGS)
#redsea_LDADD = $(GTKMM_LIBS) @SNDFILE_LIBS@ -lfftw3 $(PORTAUDIO_LIBS)
redsea_LDADD = -L. -lwdsp -lc -lliquid
redsea_SOURCES = redsea.cc mpx2bits.cc bits2blocks.cc groups.cc data.cc rdsstring.cc tmc.cc tmc_events.cc util.cc
