# redsea

redsea is a command-line utility that decodes
[RDS](http://en.wikipedia.org/wiki/Radio_Data_System) data from FM broadcasts
and prints it to the terminal. It works with rtl_fm.

[explanatory blog post](http://www.windytan.com/2015/02/receiving-rds-with-rtl-sdr.html)

## Requirements

* Linux or OSX
* g++
* GNU autotools
* wdsp (TBA)
* [rtl-sdr](http://sdr.osmocom.org/trac/wiki/rtl-sdr)

## Compiling

```
autoreconf --install
./configure
make
```

## Usage

```
rtl_fm -M -fm 87.9M -l 0 -A std -p 0 -s 250k -F 9 | ./src/redsea
```

## Licensing

```
Copyright (c) Oona Räisänen OH2EIQ (windyoona@gmail.com)

Permission to use, copy, modify, and/or distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
```
