CC = gcc

CFLAGS = -Wall -Wextra -std=gnu99 -pedantic

OFLAGS = -O3

all: bits downmix


bits: bits.o
	$(CC) $(CFLAGS) $(OFLAGS) bits.o -lm -o bits

downmix: downmix.o
	$(CC) $(CFLAGS) $(OFLAGS) downmix.o -lm -o downmix

bits.o: bits.c bits.h
	$(CC) $(CFLAGS) $(OFLAGS) -c bits.c -o bits.o

downmix.o: downmix.c
	$(CC) $(CFLAGS) $(OFLAGS) -c downmix.c -o downmix.o

clean:
	rm -f *.o
