rtl_redsea: rtl_redsea.c
	gcc -std=gnu99 -o rtl_redsea rtl_redsea.c -lm

clean:
	rm rtl_redsea
