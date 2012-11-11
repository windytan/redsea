CC = gcc

CFLAGS = -Wall -Wextra -std=gnu99 -pedantic

OFLAGS = -O3

all: bits bits-ic downmix


bits: bits.o
	$(CC) $(CFLAGS) $(OFLAGS) bits.o -lm -o bits

bits-ic: bits-ic.o
	$(CC) $(CFLAGS) $(OFLAGS) bits-ic.o -o bits-ic

downmix: downmix.o
	$(CC) $(CFLAGS) $(OFLAGS) downmix.o -lm -o downmix

bits.o: bits.c bits.h
	$(CC) $(CFLAGS) $(OFLAGS) -c bits.c -o bits.o

bits-ic.o: bits-ic.c bits.h
	$(CC) $(CFLAGS) $(OFLAGS) -c bits-ic.c -o bits-ic.o

downmix.o: downmix.c
	$(CC) $(CFLAGS) $(OFLAGS) -c downmix.c -o downmix.o

clean:
	rm -f *.o
