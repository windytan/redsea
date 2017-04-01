# redsea

redsea is a lightweight command-line
[RDS](http://en.wikipedia.org/wiki/Radio_Data_System) decoder for Linux/macOS.
It works with any [RTL-SDR](http://www.rtl-sdr.com/about-rtl-sdr/) USB radio
stick using the `rtl_fm` tool. It can also decode raw ASCII bitstream, the hex
format used by RDS Spy, and multiplex signals (MPX).

RDS groups are printed to the terminal as line-delimited JSON objects
or, optionally, undecoded hex blocks (`-x`).

[![Build Status](https://travis-ci.org/windytan/redsea.svg?branch=master)](https://travis-ci.org/windytan/redsea)

## Compiling

You will need git, the [liquid-dsp](https://github.com/jgaeddert/liquid-dsp)
library, and GNU autotools. On macOS (OSX) you will also need XCode command-line
tools (`xcode-select --install`).

1. Clone the repository (unless you downloaded a release zip file):

        $ git clone https://github.com/windytan/redsea.git
        $ cd redsea

2. Compile redsea:

        $ ./autogen.sh && ./configure && make

3. Install:

        $ make install

It is also simple to later pull the latest updates and recompile:

        $ git pull
        $ ./autogen.sh && ./configure && make clean && make
        $ make install

For a slower machine it can take some time to compile the TMC support. This can
be disabled (`./configure --disable-tmc`).

If you only need to decode hex or binary input and don't need demodulation,
you can compile redsea without liquid-dsp (`./configure --without-liquid`).

## Usage

### Live decoding with rtl_fm

The full command is:

    $ rtl_fm -M fm -l 0 -A std -p 0 -s 171k -g 40 -F 9 -f 87.9M | redsea

There's a shorthand shell script called `rtl-rx.sh`:

    $ ./rtl-rx.sh -f 87.9M

Command line options to this script are passed on to `rtl_fm`. Station frequency
(`-f`) is mandatory. It may also be helpful to set `-p` to the ppm error in the
crystal. Gain is set to 40 dB by default. The script can be modified to include
additional parameters to redsea as well.

For Raspberry Pi 1 it's necessary to change `-A std` to `-A fast`. This
changes the arctan calculation in the FM demodulator to a fast integer
approximation, so that more cycles will be left to redsea.

Note that `rtl_fm` will tune a bit off; this is expected behavior and is done to
avoid the DC spike.

### Decoding MPX from a file or via sound card

If you have `sox` installed, it's easy to decode audio files containing a
demodulated FM carrier. Note that the file must have around 128k samples per
second or more.

    $ sox multiplex.wav -t .s16 -r 171k -c 1 - | redsea

If your sound card supports recording at high sample rates (e.g. 192 kHz) you
can also decode the MPX output of an FM tuner or RDS encoder:

    $ rec -t .s16 -r 171k -c 1 - | redsea

### Tips for output formatting

The JSON output can be tidied and/or colored using `jq`:

    $ ./rtl-rx.sh -f 87.9M | jq

It can also be used to extract only certain fields, for instance the program
type:

    $ ./rtl-rx.sh -f 87.9M | jq '.prog_type'

The output can be timestamped using `ts`, found in `moreutils`:

    $ ./rtl-rx.sh -f 87.9M | ts

This timestamp can be formatted using `strftime` style format strings, see `man
ts` and `man strftime`.

### Full usage

```
radio_command | redsea [OPTIONS]

-b, --input-ascii      Input is ASCII bit stream (011010110...)
-e, --feed-through     Echo the input signal to stdout and print decoded groups
                       to stderr
-h, --input-hex        Input is hex groups in the RDS Spy format
-x, --output-hex       Output is hex groups in the RDS Spy format
-p. --show-partial     Display PS and RadioText before completely received (as
                       partial_ps, partial_radiotext)
-r, --samplerate       Set input sample frequency - will resample
                       (slow) if this differs from 171000 Hz
-u, --rbds             Use RBDS (North American) program types
-l, --loctable DIR     Load TMC location table from a directory in TMC Exchange
                       format
-v, --version          Print version
```

By default, the input (via stdin) is demodulated FM multiplex (MPX) with 16-bit
mono samples at 171 kHz. The output format defaults to newline-delimited JSON.

## Requirements

* Linux or macOS
* C++11 compiler
* GNU autotools
* libiconv
* [liquid-dsp](https://github.com/jgaeddert/liquid-dsp)
* `rtl_fm` (from [rtl-sdr](http://sdr.osmocom.org/trac/wiki/rtl-sdr)) or any
   other source that can output demodulated FM multiplex signals

## Features

Redsea decodes the following basic info from RDS:

* Program Identification code (PI)
* Program Service name (PS)
* RadioText (RT)
* Traffic Program (TP) and Traffic Announcement (TA) flags
* Music/Speech (M/S) flag
* Program Type (PTY)
* Alternative Frequencies (AF)
* Clock Time and Date (CT)
* Program Item Number (PIN)
* Enhanced Other Networks (EON) information

And also these Open Data Applications:

* RadioText Plus (RT+)
* Traffic Message Channel (TMC)

## Contributing

[Bug reports](https://github.com/windytan/redsea/issues) are welcome. Also, if a
station in your area is transmitting an interesting Open Data application that
should be implemented in redsea, I would be happy to see a minute or two's worth
of hex data using the `-x` switch.

## Licensing

Redsea is released under the MIT license, which means it is copyrighted to Oona
Räisänen OH2EIQ yet you're free to use it provided that the copyright
information is not removed. (jsoncpp and iconvpp have their own licenses.)
See LICENSE.
