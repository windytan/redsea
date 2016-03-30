# redsea

redsea is a command-line utility that decodes
[RDS](http://en.wikipedia.org/wiki/Radio_Data_System) data from FM broadcasts
and prints it to the terminal. It works with rtl_fm.

[explanatory blog post](http://www.windytan.com/2015/02/receiving-rds-with-rtl-sdr.html)

## Requirements

* Linux or OSX
* g++
* GNU autotools
* [rtl-sdr](http://sdr.osmocom.org/trac/wiki/rtl-sdr)
* Perl &gt;= 5.12

## Compiling

```
autoreconf --install
./configure
make
```

## Usage

### Simple

Receive RDS of a channel at 94.0 MHz:

```
perl redsea.pl 94.0M
```

### Full

```
perl redsea.pl [-hlst] [-p <error>] [-g <gain>] FREQ

    -h          display this help and exit
    -l          print groups in long format
    -s          print groups in short format (default)
    -t          print an ISO timestamp before each group
    -p <error>  parts-per-million error, passed to rtl_fm (optional;
                allows for faster PLL lock if set correctly)
    FREQ        station frequency in Hz, can be SI suffixed (94.0M)
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
