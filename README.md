# redsea

redsea is a command-line utility that decodes
[RDS](http://en.wikipedia.org/wiki/Radio_Data_System) data from FM broadcasts
and prints it to the terminal. It works well with rtl_fm.

[explanatory blog post](http://www.windytan.com/2015/02/receiving-rds-with-rtl-sdr.html)

## Features

Readsea takes a wideband FM multiplex signal as input. It decodes the following info from RDS:

* Program Identification code (PI)
* Program Service name (PS)
* Radiotext (RT)
* Traffic Program (TP) and Traffic Announcement (TA) flags
* Program Type (PTY)
* Alternate Frequencies (AF)

Redsea is a light-weight command line utility.

Output format is currently JSON structures.

## Requirements

* Linux/OSX
* C++11 compiler
* GNU autotools
* [wdsp](https://github.com/windytan/wdsp)
* rtl_fm (from [rtl-sdr](http://sdr.osmocom.org/trac/wiki/rtl-sdr)) or any other source that can output FM multiplex signals

## Compiling

```
autoreconf --install
./configure
make
```

## Usage

Live reception with rtl_fm:

```
rtl_fm -M fm -f 87.9M -l 0 -A std -p 0 -s 228k -F 9 | ./src/redsea
```

Decoding a pre-recorded multiplex signal via SoX:

```
sox multiplex.wav -t .s16 -r 228k - | ./src/redsea
```

## Input format

Redsea expects an FM multiplex signal, i.e. the full baseband signal of an FM station, as a single-channel, 16-bit, signed-integer PCM stream sampled at 228 kHz via standard input.

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
