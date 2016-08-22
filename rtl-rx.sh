#!/bin/sh
rtl_fm -M fm -l 0 -A std -p 0 -s 228k -F 9 $@ | ./src/redsea
