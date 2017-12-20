# redsea RDS decoder

redsea is a lightweight command-line [FM-RDS][Wikipedia: RDS] decoder for
Linux/macOS, written by Oona Räisänen. It can be used with any
[RTL-SDR][About RTL-SDR] USB radio stick with the `rtl_fm` tool. It can also
decode raw ASCII bitstream, the hex format used by RDS Spy, and audio files
containing multiplex signals (MPX).

Decoded RDS groups are printed to the terminal as line-delimited JSON objects
or, optionally, undecoded hex blocks (`-x`). Please refer to the wiki for
[JSON field descriptions][Wiki: JSON] and [input data formats][Wiki: Input].

[![Build Status](https://travis-ci.org/windytan/redsea.svg?branch=master)](https://travis-ci.org/windytan/redsea)

[Wikipedia: RDS]: http://en.wikipedia.org/wiki/Radio_Data_System
[About RTL-SDR]: http://www.rtl-sdr.com/about-rtl-sdr
[Wiki: JSON]: https://github.com/windytan/redsea/wiki/JSON-field-descriptions
[Wiki: Input]: https://github.com/windytan/redsea/wiki/Input-formats

## Contents

  * [Installation](#installation)
  * [Usage](#usage)
    * [Full usage](#full-usage)
    * [Formatting and filtering the JSON output](#formatting-and-filtering-the-json-output)
  * [Requirements](#requirements)
  * [Troubleshooting](#troubleshooting)
    * [Can't find liquid-dsp on macOS](#cant-find-liquid-dsp-on-macos)
    * [Can't find liquid-dsp on Linux](#cant-find-liquid-dsp-on-linux)
  * [Contributing](#contributing)
  * [Licensing](#licensing)

## Installation

You will need git, the [liquid-dsp][liquid-dsp] library, and GNU autotools.
Audio files can be decoded if libsndfile is installed. On macOS (OSX) you will
also need XCode command-line tools (`xcode-select --install`).

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

[liquid-dsp]: https://github.com/jgaeddert/liquid-dsp

## Usage

Please refer to the [wiki][Wiki: Use cases] for more details and usage examples.

[Wiki: Use cases]: https://github.com/windytan/redsea/wiki/Use-cases


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

-u, --rbds             RBDS mode; use North American program type names and
                       "back-calculate" the station's call sign from its PI
                       code. Note that this calculation gives an incorrect call
                       sign for most stations that transmit TMC.

-v, --version          Print version string.

-x, --output-hex       Output hex groups in the RDS Spy format,
                       suppressing JSON output.
```

### Formatting and filtering the JSON output

The JSON output can be tidied and/or colored using `jq`:

    $ rtl_fm ... | redsea | jq

It can also be used to extract only certain fields, for instance the program
type:

    $ rtl_fm ... | redsea | jq '.prog_type'


## Requirements

* Linux or macOS
* For realtime decoding, a Raspberry Pi 1 or faster
* ~8 MB of free memory (~128 MB for RDS-TMC)
* C++11 compiler
* GNU autotools
* libiconv
* libsndfile (optional)
* [liquid-dsp][liquid-dsp]
* `rtl_fm` (from [rtl-sdr](http://sdr.osmocom.org/trac/wiki/rtl-sdr)) or any
   other source that can output demodulated FM multiplex signals

## Troubleshooting

### Can't find liquid-dsp on macOS

If you've installed [liquid-dsp][liquid-dsp] yet `configure` can't find it, it's
possible that XCode command line tools aren't installed. Run this command to fix
it:

    xcode-select --install

### Can't find liquid-dsp on Linux

Try running this in the terminal:

    sudo ldconfig

## Contributing

[Bug reports](https://github.com/windytan/redsea/issues) are welcome. Be
prepared to check back with GitHub occasionally for clarifying questions.

Also, if a station in your area is transmitting an interesting RDS feature
that should be implemented in redsea, I would be happy to see a minute or
two's worth of hex data using the `-x` switch.

## Licensing

Redsea is released under the MIT license, which means it is copyrighted to Oona
Räisänen OH2EIQ yet you're free to use it provided that the copyright
information is not removed. (jsoncpp and iconvpp have their own licenses.)
See LICENSE.
