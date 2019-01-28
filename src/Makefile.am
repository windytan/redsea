bin_PROGRAMS = redsea
redsea_CPPFLAGS = -std=c++14 -Wall -Wextra -Wstrict-overflow -Wshadow -Wdouble-promotion -Wundef -Wpointer-arith -Wcast-align -Wcast-qual -Wuninitialized -pedantic \
									$(MACPORTS_CF) $(RFLAGS)
redsea_LDADD = $(MACPORTS_LD) -lc $(LIQUID) $(ICONV) $(SNDFILE)
redsea_SOURCES = redsea.cc input.cc subcarrier.cc block_sync.cc groups.cc \
								 tables.cc rdsstring.cc tmc/tmc.cc tmc/locationdb.cc util.cc \
								 channel.cc options.cc liquid_wrappers.cc jsoncpp.cpp
