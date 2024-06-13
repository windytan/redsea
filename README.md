# redsea RDS decoder

redsea is a lightweight command-line FM-RDS decoder supporting a large [subset of RDS features][Wiki: Features].

[![release](https://img.shields.io/github/release/windytan/redsea.svg)](https://github.com/windytan/redsea/releases/latest)
![build](https://github.com/windytan/redsea/workflows/build/badge.svg)

Decoded RDS groups are printed to the terminal as [line-delimited JSON](https://jsonlines.org/) objects
or, optionally, undecoded hex blocks (`-x`). Please refer to the wiki for
[input data formats][Wiki: Input].

Redsea can be used with any [RTL-SDR][About RTL-SDR] USB radio stick with the
`rtl_fm` tool, or any other SDR via `csdr`, for example. It can also
decode raw ASCII bitstream, the hex format used by RDS Spy, and audio files
containing multiplex signals (MPX). These use cases are documented in
the [wiki][Wiki: Use cases].

[About RTL-SDR]: http://www.rtl-sdr.com/about-rtl-sdr
[Wiki: Features]: https://github.com/windytan/redsea/wiki/Supported-RDS-features
[Wiki: Input]: https://github.com/windytan/redsea/wiki/Input-formats

Example output:

```json
{"pi":"0xD3C2","group":"0A","ps":"MDR JUMP","di":{"dynamic_pty":true},"is_music":true,"prog_type":"Pop music","ta":false,"tp":false}
{"pi":"0xD3C2","group":"2A","prog_type":"Pop music","tp":false}
{"pi":"0xD3C2","group":"2A","radiotext":"Das Leichteste der Welt von Silbermond JETZT AUF MDR JUMP","prog_type":"Pop music","tp":false}
{"pi":"0xD3C2","group":"12A","prog_type":"Pop music","radiotext_plus":{"item_running":true,"item_toggle":1,"tags":[{"content-type":"item.title","data":"Das Leichteste der Welt"},{"content-type":"item.artist","data":"Silbermond"}]},"tp":false}
{"pi":"0xD3C2","group":"3A","open_data_app":{"app_name":"RadioText+ (RT+)","oda_group":"12A"},"prog_type":"Pop music","tp":false}
```

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

These commands should be run in the terminal. Don't type the `$` in the
beginning.

1. Install the prerequisites. On Ubuntu:

        $ sudo apt install git ninja-build build-essential python3-pip libsndfile1-dev libliquid-dev
        $ pip3 install --user meson

Or on macOS using Homebrew:

        $ brew install meson libsndfile liquid-dsp
        $ xcode-select --install

2. Clone the repository (unless you downloaded a release zip file):

        $ git clone https://github.com/windytan/redsea.git
        $ cd redsea

3. Compile redsea:

        $ meson setup build && cd build && meson compile

How to later get the latest updates and recompile:

        $ git pull
        $ cd build && meson compile

[liquid-dsp]: https://github.com/jgaeddert/liquid-dsp/releases/tag/v1.3.2

Build instructions for Windows are in [the wiki][Wiki: Windows build], for both Cygwin and
building an .exe with MSYS2/MinGW.

[Wiki: Windows build]: (https://github.com/windytan/redsea/wiki/Installation#windows).

## Usage

By default, an MPX signal is expected via stdin (raw 16-bit signed-integer PCM).

The simplest way to view RDS groups using `rtl_fm` is:

    rtl_fm -M fm -l 0 -A std -p 0 -s 171k -g 20 -F 9 -f 87.9M | redsea -r 171k

Please refer to the [wiki][Wiki: Use cases] for more details and usage examples.

[Wiki: Use cases]: https://github.com/windytan/redsea/wiki/Use-cases


### Full usage

```
radio_command | redsea [OPTIONS]
redsea -f WAVEFILE

-b, --input-bits       Same as --input bits (for backwards compatibility).

-c, --channels CHANS   Number of channels in the raw input signal. Channels are
                       interleaved streams of samples that are demodulated
                       independently.

-e, --feed-through     Echo the input signal to stdout and print decoded groups
                       to stderr. This only works for raw PCM.

-E, --bler             Display the average block error rate, or the percentage
                       of blocks that had errors before error correction.
                       Averaged over the last 12 groups. For hex input, this is
                       the percentage of missing blocks.

-f, --file FILENAME    Read MPX input from a wave file with headers (.wav,
                       .flac, ...). If you have headered wave data via stdin,
                       use '-'. Or you can specify another format with --input.

-h, --input-hex        Same as --input hex (for backwards compatibility).

-i, --input FORMAT     Decode input as FORMAT (see the redsea wiki in github
                       for more info).
                         bits Unsynchronized ASCII bit stream (011010110...).
                              All characters but '0' and '1' are ignored.
                         hex  RDS Spy hex format. (Timestamps will be ignored)
                         pcm  MPX as raw mono S16LE PCM. Remember to also
                              specify --samplerate. If you're reading from a
                              sound file with headers (WAV, FLAC, ...) don't
                              specify this.
                         tef  Serial data from the TEF6686 tuner.

-l, --loctable DIR     Load TMC location table from a directory in TMC Exchange
                       format. This option can be specified multiple times to
                       load several location tables.

-o, --output FORMAT    Print output as FORMAT:
                         hex  RDS Spy hex format.
                         json Newline-delimited JSON (default).

-p, --show-partial     Under noisy conditions, redsea may not be able to fully
                       receive all information. Multi-group data such as PS
                       names, RadioText, and alternative frequencies are
                       especially vulnerable. This option makes it display them
                       even if not fully received, prefixed with partial_.

-r, --samplerate RATE  Set sample frequency of raw PCM input in Hz. Will
                       resample if this differs from 171000 Hz.

-R, --show-raw         Include raw group data as hex in the JSON stream.

-t, --timestamp FORMAT Add time of decoding to JSON groups; see man strftime
                       for formatting options (or try "%c"). Use "%f" to add
                       hundredths of seconds.

-u, --rbds             RBDS mode; use North American program type names and
                       "back-calculate" the station's call sign from its PI
                       code. Note that this calculation gives an incorrect call
                       sign for most stations that transmit TMC.

-v, --version          Print version string and exit.

-x, --output-hex       Same as --output hex (for backwards compatibility).
```

### Formatting and filtering the JSON output

The JSON output can be tidied and/or colored using `jq`:

    $ rtl_fm ... | redsea | jq

It can also be used to extract only certain fields, for instance the program
type:

    $ rtl_fm ... | redsea | jq '.prog_type'


## Requirements

### Runtime

* Linux/macOS/Windows
* For realtime decoding, a Raspberry Pi 1 or faster
* ~8 MB of free memory (~128 MB for RDS-TMC)
* libiconv 1.16
* libsndfile 1.0.31
* [liquid-dsp][liquid-dsp] release 1.3.2
* `rtl_fm` (from [rtl-sdr](http://sdr.osmocom.org/trac/wiki/rtl-sdr)) or any
   other source that can output demodulated FM multiplex signals

### Build

* Linux/macOS/Cygwin/MSYS2+MinGW
* C++14 compiler
* meson + ninja

### Testing

* Catch2

## Troubleshooting

### Can't find liquid-dsp on macOS

If you've installed [liquid-dsp][liquid-dsp] yet meson can't find it, it's
possible that XCode command line tools aren't installed. Run this command to fix
it:

    xcode-select --install

### Can't find liquid-dsp on Linux

Try running this in the terminal:

    sudo ldconfig

## Contributing

Bug reports are welcome. See [CONTRIBUTING](CONTRIBUTING.md) for more
information.

Also, if a station in your area is transmitting an interesting RDS feature
that should be implemented in redsea, I would be happy to see a minute or
two's worth of hex data using the `-x` switch. You could use a gist or an
external pastebin service and post a link to it.

## Licensing

Redsea is released under the MIT license, which means it is copyrighted to Oona
Räisänen OH2EIQ yet you're free to use it provided that the copyright
information is not removed. (jsoncpp and iconvpp have their own licenses.)
See LICENSE.

This software should never be relied on for emergency communication, accurate
traffic / weather information, or when your life is on the line.
