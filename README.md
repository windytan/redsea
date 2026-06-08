# redsea RDS decoder

**redsea** is a lightweight command-line [FM-RDS](https://en.wikipedia.org/wiki/Radio_Data_System)
decoder that supports many [RDS features][Wiki: Features].

[![release](https://img.shields.io/github/release/windytan/redsea.svg)](https://github.com/windytan/redsea/releases/latest)
[![linux](https://github.com/windytan/redsea/workflows/linux/badge.svg)](https://github.com/windytan/redsea/actions/workflows/linux.yml?query=branch%3Amaster)
[![macos](https://github.com/windytan/redsea/workflows/macos/badge.svg)](https://github.com/windytan/redsea/actions/workflows/macos.yml?query=branch%3Amaster)
[![windows](https://github.com/windytan/redsea/workflows/windows/badge.svg)](https://github.com/windytan/redsea/actions/workflows/windows.yml?query=branch%3Amaster)

It can decode RDS live into [newline-delimited JSON](https://jsonlines.org/) where
each line corresponds to one RDS group; or it can print them as raw hex.

[About RTL-SDR]: http://www.rtl-sdr.com/about-rtl-sdr
[Wiki: Features]: https://github.com/windytan/redsea/wiki/Supported-RDS-features
[Wiki: Input]: https://github.com/windytan/redsea/wiki/Input-formats

Simple usage (note the recommended sampling rate of 171k):

```bash
rtl_fm -f 89.3M -s 171k - | redsea -r 171k
```

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
  * [Requirements](#requirements)
  * [Troubleshooting](#troubleshooting)
    * [Can't find liquid-dsp on macOS](#cant-find-liquid-dsp-on-macos)
    * [Can't find liquid-dsp on Linux](#cant-find-liquid-dsp-on-linux)
  * [Contributing](#contributing)
  * [Licensing](#licensing)

## How to install

Redsea is written in C++ and needs to be built from source, but this is not very complicated. Commands are provided
below.

### 1. Install dependencies

On Ubuntu:

```bash
sudo apt install git build-essential meson libsndfile1-dev libliquid-dev
```

Or on older Debians:

```bash
sudo apt-get install python3-pip ninja-build build-essential libsndfile1-dev libliquid-dev nlohmann-json3-dev
pip3 install --user meson
```

Or on macOS using Homebrew:

```bash
brew install meson libsndfile liquid-dsp nlohmann-json
xcode-select --install
```

Meson will later download nlohmann-json for you if it can't be found in the package repositories.

It's also possible to build redsea on Windows, either in Cygwin or by building
an `*.exe` with MSYS2/MinGW. This is a bit more involved - [instructions][Wiki: Windows build] are in the wiki.

[Wiki: Windows build]: https://github.com/windytan/redsea/wiki/Installation#windows

### 2. Get redsea

Downloading a [release version](https://github.com/windytan/redsea/releases) is recommended.

Alternatively, if you wish to have the latest snapshot, you can also clone this git repository.
The snapshots are work-in-progress, but we attempt to always keep the main branch in a working condition.

```bash
git clone https://github.com/windytan/redsea.git
cd redsea
```

### 3. Compile redsea

```bash
meson setup build && cd build
meson compile
```

If your build machine has very little RAM (e.g. Raspberry Pi) please refer to our guide
on [building on a low-end system][Wiki: Building on a low-end system].

Now the binary executable is compiled and you can run it!

You can install the binary using `meson install` if you so wish. By default,
it will be installed under `/usr/local`, but this can be changed by providing
e.g. `meson setup build --prefix /usr/local`. See the
[Meson guide](https://mesonbuild.com/Quick-guide.html#compiling-a-meson-project)
for more.

If you cloned the repository you can later get the latest updates and recompile:

```bash
git pull
cd build && meson compile
```

[Wiki: Building on a low-end system]: https://github.com/windytan/redsea/wiki/Building-on-a-low‐end-system

## Usage
We have more [usage examples][Wiki: Use cases] in the wiki.

See the full list of [command line options][Wiki: Command line options] in the wiki
or type `redsea --help`.

### From RTL-SDR to JSON

Redsea reads an MPX signal from stdin by default. It expects the input
to be [raw](https://en.wikipedia.org/wiki/Raw_audio_format) 16-bit signed-integer PCM.
This means it can easily be used in real-time with `rtl_fm`.

Here's an example command that listens to 87.9 MHz using `rtl_fm` and displays
the RDS groups:

```bash
rtl_fm -M fm -l 0 -A std -p 0 -s 171k -g 20 -F 9 -f 87.9M | redsea -r 171k
```

### From SPY files to JSON

`*.spy` (or `*.rds`) is the common hexadecimal RDS logging format.

```bash
redsea --input hex < sample_hex_file.spy
```

### From WAV files to hex

Use the `-f` option to read WAV or other audio files that have a header. Redsea will
automatically detect the sample rate and encoding. You can even read a WAV
from a pipe (stdin) using `-f -`.

```bash
redsea -f mpx_input.wav --output hex
```

Note that not all `rtl_fm` versions write a WAV header with parameter `-E wav`. You might
need to do that with sox, see `rtl_fm  -f 104900k -s 171k - | sox -t raw -r 171k -es -b 16 - -c 1 104900.wav`

[Wiki: Use cases]: https://github.com/windytan/redsea/wiki/Use-cases
[Wiki: Command line options]: https://github.com/windytan/redsea/wiki/Command-line-options

## Requirements

### System

* Linux/macOS/Windows
* For realtime decoding, a Raspberry Pi 1 or faster (see [benchmark results](https://github.com/windytan/redsea/wiki/Benchmark-results)).

### Runtime dependencies

* libiconv
* libsndfile
* [liquid-dsp][liquid-dsp]
* nlohmann-json

[liquid-dsp]: https://github.com/jgaeddert/liquid-dsp/releases/tag/v1.3.2

### For building

* Linux/macOS/Cygwin/MSYS2+MinGW
* C++17 compiler (tested on GCC 9.5)
* meson + ninja
* enough RAM (see [building on a low-end system][Wiki: Building on a low-end system])

### Testing (optional, for development)

* Catch2
* Perl
* SoX

See CONTRIBUTING.md for how to build and run the tests.

## Troubleshooting

### Can't find liquid-dsp on macOS

If you've installed [liquid-dsp][liquid-dsp] yet meson can't find it, it's
possible that XCode command line tools aren't installed. Run this command to fix
it:

```bash
xcode-select --install
```

### Can't find liquid-dsp on Linux

Try running this in the terminal:

```bash
sudo ldconfig
```

## Contributing

We welcome bug reports and documentation contributions. Or take a peek at our
[open issues](https://github.com/windytan/redsea/issues) to see where we could use a hand. New code is contributed using pull requests; See
[CONTRIBUTING](CONTRIBUTING.md) for more information.

## Licensing

Redsea is released under the MIT license, which means it is copyrighted to Oona
Räisänen OH2EIQ yet you're free to use it provided that the copyright
information is not removed. (iconvpp has its own license.) See LICENSE.

**Note**: This software is not safety certified. Do not rely on it for emergency
communication, accurate traffic / weather information, or life-critical situations.
