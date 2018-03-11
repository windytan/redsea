bin_PROGRAMS = redsea
redsea_CPPFLAGS = -std=c++11 -Wall -Wextra -Wstrict-overflow -Wshadow \
									$(MACPORTS_CF) \
									-Wuninitialized -pedantic $(RFLAGS)
redsea_LDADD = $(MACPORTS_LD) -lc $(LIQUID) $(ICONV) $(SNDFILE)
redsea_SOURCES = redsea.cc input.cc subcarrier.cc block_sync.cc groups.cc \
								 tables.cc rdsstring.cc tmc/tmc.cc tmc/locationdb.cc util.cc \
								 channel.cc options.cc liquid_wrappers.cc jsoncpp.cpp
