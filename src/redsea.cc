/*
 * redsea - RDS decoder
 * Copyright (c) Oona Räisänen OH2EIQ (windyoona@gmail.com)
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 */
#include <getopt.h>
#include <iostream>

#include "config.h"
#include "src/channel.h"
#include "src/common.h"
#include "src/block_sync.h"
#include "src/groups.h"
#include "src/options.h"
#include "src/subcarrier.h"

namespace redsea {

void PrintUsage() {
  std::cout <<
    "radio_command | redsea [OPTIONS]\n"
    "\n"
    "By default, a 171 kHz single-channel 16-bit MPX signal is expected via\n"
    "stdin.\n"
    "\n"
    "-b, --input-bits       Input is an unsynchronized ASCII bit stream\n"
    "                       (011010110...). All characters but '0' and '1'\n"
    "                       are ignored.\n"
    "\n"
    "-c, --channels CHANS   Number of channels in the raw input signal. Each\n"
    "                       channel is demodulated independently.\n"
    "\n"
    "-e, --feed-through     Echo the input signal to stdout and print\n"
    "                       decoded groups to stderr.\n"
    "\n"
    "-E, --bler             Display the average block error rate, or the\n"
    "                       percentage of blocks that had errors before\n"
    "                       error correction. Averaged over the last 12\n"
    "                       groups. For hex input, this is the percentage\n"
    "                       of missing blocks.\n"
    "\n"
    "-f, --file FILENAME    Use an audio file as MPX input. All formats\n"
    "                       readable by libsndfile should work.\n"
    "\n"
    "-h, --input-hex        The input is in the RDS Spy hex format.\n"
    "\n"
    "-l, --loctable DIR     Load TMC location table from a directory in TMC\n"
    "                       Exchange format.\n"
    "\n"
    "-p, --show-partial     Under noisy conditions, redsea may not be able to\n"
    "                       fully receive all information. Multi-group data\n"
    "                       such as PS names, RadioText, and alternative\n"
    "                       frequencies are especially vulnerable. This option\n"
    "                       makes it display them even if not fully received,\n"
    "                       as partial_{ps,radiotext,alt_kilohertz}.\n"
    "\n"
    "-r, --samplerate RATE  Set stdin sample frequency in Hz. Will resample\n"
    "                       (slow) if this differs from 171000 Hz.\n"
    "\n"
    "-t, --timestamp FORMAT Add time of decoding to JSON groups; see\n"
    "                       man strftime for formatting options (or\n"
    "                       try \"%c\").\n"
    "\n"
    "-u, --rbds             RBDS mode; use North American program type names\n"
    "                       and \"back-calculate\" the station's call sign from\n"
    "                       its PI code. Note that this calculation gives an\n"
    "                       incorrect call sign for most stations that transmit\n"
    "                       TMC.\n"
    "\n"
    "-v, --version          Print version string and exit.\n"
    "\n"
    "-x, --output-hex       Output hex groups in the RDS Spy format,\n"
    "                       suppressing JSON output.\n";
}

void PrintVersion() {
#ifdef DEBUG
  std::cout << PACKAGE_STRING << "-debug by OH2EIQ" << '\n';
#else
  std::cout << PACKAGE_STRING << " by OH2EIQ" << '\n';
#endif
}

}  // namespace redsea

int main(int argc, char** argv) {
  redsea::Options options = redsea::GetOptions(argc, argv);

  if (options.print_usage)
    redsea::PrintUsage();

  if (options.print_version)
    redsea::PrintVersion();

  if (options.exit_failure)
    return EXIT_FAILURE;

  if (options.exit_success)
    return EXIT_SUCCESS;

#ifndef HAVE_LIQUID
  if (options.input_type == redsea::InputType::MPX_stdin ||
      options.input_type == redsea::InputType::MPX_sndfile) {
    std::cerr << "error: redsea was compiled without liquid-dsp"
              << '\n';
    return EXIT_FAILURE;
  }
#endif

  redsea::MPXReader mpx(options);
  options.samplerate = mpx.samplerate();
  options.num_channels = mpx.num_channels();

  /* When we don't have MPX input, there are no channels. But we want at least
   * 1 Channel anyway. Also, we need a sample rate for the Subcarrier
   * constructor.
   */
  if (options.input_type != redsea::InputType::MPX_sndfile &&
      options.input_type != redsea::InputType::MPX_stdin) {
    options.num_channels = 1;
    options.samplerate = redsea::kTargetSampleRate_Hz;
  }

  std::vector<redsea::Channel> channels;
  for (int n_channel = 0; n_channel < options.num_channels; n_channel++)
    channels.emplace_back(redsea::Channel(options, n_channel));

  redsea::AsciiBitReader ascii_reader(options);

  while (true) {
    bool eof = false;
    if (options.input_type == redsea::InputType::MPX_stdin ||
        options.input_type == redsea::InputType::MPX_sndfile) {
      eof = mpx.eof();
    } else if (options.input_type == redsea::InputType::ASCIIbits) {
      eof = ascii_reader.eof();
    } else if (options.input_type == redsea::InputType::Hex) {
      eof = std::cin.eof();
    }

    if (eof)
      break;

    if (options.input_type == redsea::InputType::MPX_stdin ||
        options.input_type == redsea::InputType::MPX_sndfile)
      mpx.FillBuffer();

    for (int n_channel = 0; n_channel < options.num_channels; n_channel++) {
      switch (options.input_type) {
        case redsea::InputType::MPX_stdin:
        case redsea::InputType::MPX_sndfile:
          channels[n_channel].ProcessChunk(mpx.ReadChunk(n_channel));
          break;

        case redsea::InputType::ASCIIbits:
          channels[n_channel].ProcessBit(ascii_reader.ReadNextBit());
          break;

        case redsea::InputType::Hex:
          channels[n_channel].ProcessGroup(redsea::ReadNextHexGroup(options));
          break;
      }
    }
  }

  return EXIT_SUCCESS;
}
