#!/usr/bin/perl
#
# Oona Räisänen (windytan) 2014

use warnings;

while (not eof STDIN) {
  read(STDIN, $a, 1);
  if ($a =~ /[a-fA-F0-9]/) {
    for (reverse (0..3)) {
      print ((hex($a) >> $_) & 1);
    }
  }
}
