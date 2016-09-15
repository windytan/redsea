# redsea

redsea is an experiment at building a lightweight command-line
[RDS](http://en.wikipedia.org/wiki/Radio_Data_System) decoder.
It works mainly with `rtl_fm` but can also decode raw ASCII bitstream,
the hex format used by RDS Spy, and MPX input via a sound card. Redsea
has been successfully compiled on Linux and OSX.

## Compiling

You will need git, [liquid-dsp](https://github.com/jgaeddert/liquid-dsp), and GNU autotools.

1. Clone the repository:

        $ git clone https://github.com/windytan/redsea.git
        $ cd redsea

2. Run autotools:

        $ autoreconf --install

3. Compile redsea:

        $ ./configure
        $ make

If you get an error message about the STDCXX_11 macro or an unexpected token, try installing `autoconf-archive`.

## Usage

```
radio_command | ./src/redsea [-b | -x]

-b    Input is ASCII bit stream (011010110...)
-h    Input is hex groups in the RDS Spy format
-x    Output is hex groups in the RDS Spy format
```

By default, the input (via stdin) is MPX with 16-bit mono samples at 228 kHz. The output
format defaults to line delimited JSON.

### Live decoding with rtl_fm

There's a convenience shell script called `rtl-rx.sh`:

    $ ./rtl-rx.sh -f 87.9M

Command line options are passed on to `rtl_fm`. Station frequency (`-f`) is mandatory. It may also be helpful to set `-p` to the ppm error in the crystal. (Note that `rtl_fm` will tune a bit off; this is expected behavior.) Gain is set to 40 dB by default. The script can be modified to include additional parameters to redsea as well.

### Decoding a pre-recorded signal with SoX

    $ sox multiplex.wav -t .s16 -r 228k -c 1 - | ./src/redsea

The signal should be FM demodulated and have enough bandwidth to accommodate the RDS subcarrier (> 60 kHz).

### Decoding MPX via sound card

If your sound card supports recording at 192 kHz, and you have `sox` installed, you can also decode the MPX output of an FM tuner or RDS encoder:

    $ rec -t .s16 -r 228k -c 1 - | ./src/redsea

## Requirements

* Linux/OSX
* C++11 compiler
* GNU autotools
* [liquid-dsp](https://github.com/jgaeddert/liquid-dsp)
* `rtl_fm` (from [rtl-sdr](http://sdr.osmocom.org/trac/wiki/rtl-sdr)) or any other source that can output demodulated FM multiplex signals

## Features

Redsea decodes the following basic info from RDS:

* Program Identification code (PI)
* Program Service name (PS)
* RadioText (RT)
* Traffic Program (TP) and Traffic Announcement (TA) flags
* Program Type (PTY)
* Alternate Frequencies (AF)
* Clock Time and Date (CT)
* Program Item Number (PIN)
* Enhanced Other Networks (EON) information

And also these Open Data Applications:

* RadioText Plus (RT+)
* Traffic Message Channel (TMC)

## Contributing

Bug reports are welcome. Also, if a station in your area is transmitting
an interesting Open Data application that should be implemented in redsea,
I would be happy to see a minute or two's worth of hex data using the `-x`
switch.

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
