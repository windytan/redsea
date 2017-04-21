#!/bin/sh
rtl_fm -M fm -l 0 -A std -p 0 -s 171k -g 40 -F 9 $@ | redsea -x | tee `head -12l | tail -1l | cut -d" " -f1`_`date +%F`_`date +%H`-`date +%M`-`date +%S`_$2Hz.spy
