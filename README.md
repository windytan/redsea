# redsea RDS decoder

redsea is a lightweight command-line [FM-RDS](https://en.wikipedia.org/wiki/Radio_Data_System)
decoder that supports many [RDS features][Wiki: Features].

[![release](https://img.shields.io/github/release/windytan/redsea.svg)](https://github.com/windytan/redsea/releases/latest)
[![linux](https://github.com/windytan/redsea/workflows/linux/badge.svg)](https://github.com/windytan/redsea/actions/workflows/linux.yml?query=branch%3Amaster)
[![macos](https://github.com/windytan/redsea/workflows/macos/badge.svg)](https://github.com/windytan/redsea/actions/workflows/macos.yml?query=branch%3Amaster)
[![windows](https://github.com/windytan/redsea/workflows/windows/badge.svg)](https://github.com/windytan/redsea/actions/workflows/windows.yml?query=branch%3Amaster)

It prints [newline-delimited JSON](https://jsonlines.org/) where
each line corresponds to one RDS group. It can also print "raw" undecoded hex blocks (`--output hex`).
Please refer to the wiki for [input data formats][Wiki: Input].

Redsea is compatible with the [RTL-SDR][About RTL-SDR] USB radio stick via the
`rtl_fm` tool, and any other SDR via a tool like `csdr` (see [wiki][Wiki: Use cases]). It can decode MPX from
raw PCM or audio files, ASCII bitstreams, the hex format used by RDS Spy, or the TEF6686 serial format.

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

  * [How to install](#how-to-install)
  * [Usage](#usage)
    * [Full usage](#full-usage)
    * [Formatting and filtering the JSON output](#formatting-and-filtering-the-json-output)
  * [Requirements](#requirements)
  * [Troubleshooting](#troubleshooting)
    * [Can't find liquid-dsp on macOS](#cant-find-liquid-dsp-on-macos)
    * [Can't find liquid-dsp on Linux](#cant-find-liquid-dsp-on-linux)
  * [Contributing](#contributing)
  * [Licensing](#licensing)

## How to install

Redsea needs to be built from source, but this is not very complicated. Commands are provided
below (you can skip the `$` at the start of each command).

### 1. Install dependencies

On Ubuntu:

        $ sudo apt install git build-essential meson libsndfile1-dev libliquid-dev

Or on older Debians:

        $ sudo apt-get install python3-pip ninja-build build-essential libsndfile1-dev libliquid-dev nlohmann-json3-dev
        $ pip3 install --user meson

Or on macOS using Homebrew:

        $ brew install meson libsndfile liquid-dsp nlohmann-json
        $ xcode-select --install

Meson will later download nlohmann-json for you if it can't be found in the package repositories.

It's also possible to build redsea on Windows, either in Cygwin or by building
an .exe with MSYS2/MinGW. This is a bit more involved - instructions are in [the wiki][Wiki: Windows build].

[Wiki: Windows build]: (https://github.com/windytan/redsea/wiki/Installation#windows).

### 2. Get redsea

Downloading a [release version](https://github.com/windytan/redsea/releases) is recommended.

Alternatively, if you wish to have the latest snapshot, you can also clone this git repository.
The snapshots are work-in-progress, but we attempt to always keep the main branch in a working condition.

        $ git clone https://github.com/windytan/redsea.git
        $ cd redsea

### 3. Compile redsea

        $ meson setup build && cd build
        $ meson compile

Now the binary executable is compiled and you can run it!

You can install the binary using `meson install` if you so wish. By default,
it will be installed under `/usr/local`, but this can be changed by providing
e.g. `meson setup build --prefix /usr/local`. See the
[Meson guide](https://mesonbuild.com/Quick-guide.html#compiling-a-meson-project)
for more.

If you cloned the repository you can later get the latest updates and recompile:

        $ git pull
        $ cd build && meson compile

If your Linux system has very little RAM (e.g. Raspberry Pi), please use the below
line instead of `meson compile` to limit the number of build processes:

        $ taskset -c 0 meson compile

The compilation will still require at least 500 MB of free RAM.

## Usage

Redsea reads an MPX signal from stdin by default. It expects the input
to be raw 16-bit signed-integer PCM.

Here's an example command that listens to 87.9 MHz using `rtl_fm` and displays
the RDS groups:

```bash
rtl_fm -M fm -l 0 -A std -p 0 -s 171k -g 20 -F 9 -f 87.9M | redsea -r 171k
```

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
                         mpx  MPX as raw mono S16LE PCM. Remember to also
                              specify --samplerate. If you're reading from a
                              sound file with headers (WAV, FLAC, ...) don't
                              specify this.
                         tef  Serial data from the TEF6686 tuner.

-l, --loctable DIR     Load TMC location table from a directory in TMC Exchange
                       format. This option can be specified multiple times to
                       load several location tables.

--no-fec               Disable forward error correction; always reject blocks
                       with incorrect syndromes. In noisy conditions, fewer errors
                       will slip through, but also fewer blocks in total; see wiki
                       for discussion.

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

-s, --streams          Decode RDS2 data streams 1, 2, and 3, if they exist.

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


## Requirements

### Runtime

* Linux/macOS/Windows
* For realtime decoding, a Raspberry Pi 1 or faster
* libiconv 1.16
* libsndfile 1.0.31
* [liquid-dsp][liquid-dsp] release 1.3.2
* nlohmann-json
* `rtl_fm` (from [rtl-sdr](http://sdr.osmocom.org/trac/wiki/rtl-sdr)) or any
   other source that can output demodulated FM multiplex signals

[liquid-dsp]: https://github.com/jgaeddert/liquid-dsp/releases/tag/v1.3.2

### Build

* Linux/macOS/Cygwin/MSYS2+MinGW
* C++14 compiler
* meson + ninja
* 500 MB of free memory for non-parallel build ([breakdown](https://github.com/windytan/redsea/issues/120#issuecomment-2559254338))

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

We welcome bug reports and documentation contributions. Or take a peek at our
[open issues](https://github.com/windytan/redsea/issues) to see where we could use a hand. See
[CONTRIBUTING](CONTRIBUTING.md) for more information.

Also, if a station in your area is transmitting an interesting RDS feature
that should be implemented in redsea, I would be happy to see a minute or
two's worth of hex data using the `--output hex` switch. You could use a
gist or an external pastebin service and post a link to it in our Github
Discussions.

## Licensing

Redsea is released under the MIT license, which means it is copyrighted to Oona
Räisänen OH2EIQ yet you're free to use it provided that the copyright
information is not removed. (iconvpp has its own license.) See LICENSE.

**Note**: This software is not safety certified. Do not rely on it for emergency
communication, accurate traffic / weather information, or life-critical situations.
