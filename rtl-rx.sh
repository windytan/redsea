#!/bin/sh

  echo "FM-RDS extraction with ReDSea using an RTLSDR stick"
  echo
  echo "Example: rtl-rx.sh -f 90.4M"
  echo "will tune to 90.4 MHz and"
  echo "the output will be written to stdout"
  echo 
  echo "For more formatting options please see"
  echo "https://github.com/windytan/redsea#tips-for-output-formatting"
  echo
  echo "In order to stop recording please press CTRL+C"
  echo

rtl_fm -M fm -l 0 -A std -p 0 -s 171k -g 40 -F 9 $@ | redsea
