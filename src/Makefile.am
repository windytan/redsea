bin_PROGRAMS = redsea
redsea_CPPFLAGS = -std=c++11 -Wall -Wextra -Wstrict-overflow -Wshadow \
									-Wuninitialized -pedantic $(RFLAGS)
redsea_LDADD = -lc $(LIQUID) $(ICONV) $(SNDFILE)
redsea_SOURCES = redsea.cc ascii_in.cc subcarrier.cc block_sync.cc groups.cc \
								 tables.cc rdsstring.cc tmc/tmc.cc tmc/locationdb.cc util.cc \
								 liquid_wrappers.cc jsoncpp.cpp
