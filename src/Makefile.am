bin_PROGRAMS = redsea
redsea_CPPFLAGS = -std=$(CPP_STD) -Wall -Wextra -Wstrict-overflow -Wshadow -Wdouble-promotion \
                  -Wundef -Wpointer-arith -Wcast-align -Wcast-qual -Wuninitialized -pedantic \
									-Wno-deprecated-declarations \
									$(MACPORTS_CF) $(RFLAGS) $(WIN_CF)
redsea_LDADD = $(MACPORTS_LD) $(LIBC) $(LIQUID) $(ICONV) $(SNDFILE)
redsea_SOURCES = redsea.cc common.cc input.cc subcarrier.cc block_sync.cc groups.cc \
								 tables.cc rdsstring.cc tmc/tmc.cc tmc/locationdb.cc util.cc \
								 channel.cc options.cc liquid_wrappers.cc ../ext/json/jsoncpp.cpp
