# redsea RDS decoder

redsea is a lightweight command-line
[FM-RDS](http://en.wikipedia.org/wiki/Radio_Data_System) decoder for Linux/macOS,
written by Oona Räisänen. It can be used with any
[RTL-SDR](http://www.rtl-sdr.com/about-rtl-sdr/) USB radio stick with the
`rtl_fm` tool. It can also decode raw ASCII bitstream, the hex format used by
RDS Spy, and audio files containing multiplex signals (MPX).

Decoded RDS groups are printed to the terminal as line-delimited JSON objects
or, optionally, undecoded hex blocks (`-x`).

Note that in the 0.x versions the names and formats of the JSON fields and
command line options may still change.

[![Build Status](https://travis-ci.org/windytan/redsea.svg?branch=master)](https://travis-ci.org/windytan/redsea)

## Contents

  * [Installation](#installation)
  * [Usage](#usage)
    * [Live decoding with rtl_fm](#live-decoding-with-rtl_fm)
    * [Listening and decoding live](#listening-and-decoding-live)
    * [Decoding MPX from a file or via sound card](#decoding-mpx-from-a-file-or-via-sound-card)
    * [Formatting and filtering the JSON output](#formatting-and-filtering-the-json-output)
    * [Full usage](#full-usage)
  * [Requirements](#requirements)
  * [Supported RDS features](#supported-rds-features)
  * [Troubleshooting](#troubleshooting)
    * [Can't find liquid-dsp on macOS](#cant-find-liquid-dsp-on-macos)
  * [Contributing](#contributing)
  * [Licensing](#licensing)

## Installation

You will need git, the [liquid-dsp](https://github.com/jgaeddert/liquid-dsp)
library, and GNU autotools. Audio files can be decoded if libsndfile is
installed. On macOS (OSX) you will also need XCode command-line tools
(`xcode-select --install`).

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

For Raspberry Pi 1 it's necessary to change `-A std` to `-A fast`. This
way more more CPU cycles will be left to redsea.

Note that `rtl_fm` will tune a bit off; this is normal and is done to
avoid the DC spike.

### Listening and decoding live

The `--feed-through` option allows you to use both the original signal and
the decoded RDS via different streams. For example, the signal can be listened
to using `sox`, while RDS groups are printed to stderr:

    $ rtl_fm -M fm -l 0 -A std -p 0 -s 171k -g 40 -F 9 -f 87.9M |\
    > redsea --feed-through |\
    > play -t .s16 -r 171k -c 1 -

### Decoding MPX from a file or via sound card

It's easy to decode audio files containing a demodulated FM carrier. Note that
the file must have around 128k samples per second or more; 171k will work
fastest.

    $ redsea -f multiplex.wav

If your sound card supports recording at high sample rates (192 kHz) you
can also decode the MPX output of an FM tuner or RDS encoder, for instance
with this `sox` command:

    $ rec -t .s16 -r 171k -c 1 - | redsea

By default, the raw MPX input is assumed to be 16-bit signed-integer
single-channel samples at 171 kHz.

### Formatting and filtering the JSON output

The JSON output can be tidied and/or colored using `jq`:

    $ rtl_fm ... | redsea | jq

It can also be used to extract only certain fields, for instance the program
type:

    $ rtl_fm ... | redsea | jq '.prog_type'

Details of the output format are described in the schema file (schema.json).

### Full usage

```
radio_command | redsea [OPTIONS]

-b, --input-bits       Input is an unsynchronized ASCII bit stream
                       (011010110...). All characters but '0' and '1'
                       are ignored.

-e, --feed-through     Echo the input signal to stdout and print
                       decoded groups to stderr.

-E, --bler             Display the average block error rate, or the
                       percentage of blocks that had errors before
                       error correction. Averaged over the last 12
                       groups. For hex input, this is the percentage
                       of missing blocks.

-f, --file FILENAME    Use an audio file as MPX input. All formats
                       readable by libsndfile should work.

-h, --input-hex        The input is in the RDS Spy hex format.

-l, --loctable DIR     Load TMC location table from a directory in TMC
                       Exchange format.

-p, --show-partial     Display PS and RadioText before completely
                       received (as partial_ps, partial_radiotext).

-r, --samplerate RATE  Set stdin sample frequency in Hz. Will resample
                       (slow) if this differs from 171000 Hz.

-t, --timestamp FORMAT Add time of decoding to JSON groups; see
                       man strftime for formatting options (or
                       try "%c").

-u, --rbds             Use RBDS (North American) program types.

-v, --version          Print version string.

-x, --output-hex       Output hex groups in the RDS Spy format,
                       suppressing JSON output.
```

## Requirements

* Linux or macOS
* For realtime decoding, a 700 MHz ARMv6 CPU or faster
* ~8 MB of free memory (~128 MB for RDS-TMC)
* C++11 compiler
* GNU autotools
* libiconv
* libsndfile (optional)
* [liquid-dsp](https://github.com/jgaeddert/liquid-dsp)
* `rtl_fm` (from [rtl-sdr](http://sdr.osmocom.org/trac/wiki/rtl-sdr)) or any
   other source that can output demodulated FM multiplex signals

## Supported RDS features

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
* Decoder Identification (DI)
* Enhanced Other Networks (EON) information

And also these Open Data Applications:

* RadioText Plus (RT+)
* Traffic Message Channel (RDS-TMC)

## Troubleshooting

### Can't find liquid-dsp on macOS

If you've installed [liquid-dsp](https://github.com/jgaeddert/liquid-dsp) yet
`configure` can't find it, it's possible that XCode command line tools aren't
installed. Run this command to fix it:

    xcode-select --install

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
