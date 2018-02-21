#!/bin/sh

  echo "FM-RDS extraction with ReDSea using an RTLSDR stick"
  echo "and output to RDSSPY compatible sample file in parallel"
  echo
  echo "Example: rtl-rx_rdsspy.sh -f 90.4M"
  echo
  echo "The script will first analyze the RDS PI code and then"
  echo "the output is written to stdout and to a file called "
  echo "[PI-Code]_[Date]_[Time]_[Freq].spy"
  echo
  echo "In order to stop please press CTRL+C"
  echo

rtl_fm -M fm -l 0 -A std -p 0 -s 171k -g 40 -F 9 $@ | redsea -x | tee `head -12l | tail -1l | cut -d" " -f1`_`date +%F`_`date +%H`-`date +%M`-`date +%S`_$2Hz.spy
