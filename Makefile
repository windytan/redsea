rtl_redsea: rtl_redsea.c filters.c filters.h
	gcc -std=gnu99 -o rtl_redsea filters.c rtl_redsea.c -lm

clean:
	rm rtl_redsea
