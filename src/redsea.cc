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
#include <iostream>

#include "config.h"
#include "src/channel.h"
#include "src/common.h"
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
    "                       Exchange format. This option can be specified\n"
    "                       multiple times to load several location tables.\n"
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

int ProcessMPXInput(Options options) {

#ifndef HAVE_LIQUID
  std::cerr << "error: redsea was compiled without liquid-dsp"
            << '\n';
  return EXIT_FAILURE;
#endif

  MPXReader mpx(options);
  options.samplerate = mpx.samplerate();
  options.num_channels = mpx.num_channels();

  if (mpx.error())
    return EXIT_FAILURE;

  std::vector<Channel> channels;
  std::vector<std::unique_ptr<Subcarrier>> subcarriers;
  for (int i = 0; i < options.num_channels; i++) {
    channels.emplace_back(options, i);
    subcarriers.push_back(std::make_unique<Subcarrier>(options));
  }

  while (!mpx.eof()) {
    mpx.FillBuffer();
    for (int i = 0; i < options.num_channels; i++) {
      channels[i].ProcessBits(
        subcarriers[i]->ProcessChunk(
          mpx.ReadChunk(i)
        )
      );
    }
  }

  return EXIT_SUCCESS;
}

int ProcessASCIIBitsInput(Options options) {
  Channel channel(options, 0);
  AsciiBitReader ascii_reader(options);

  while (!ascii_reader.eof()) {
    channel.ProcessBit(ascii_reader.ReadBit());
  }

  return EXIT_SUCCESS;
}

int ProcessHexInput(Options options) {
  Channel channel(options, 0);

  while (!std::cin.eof()) {
    channel.ProcessGroup(ReadHexGroup(options));
  }

  return EXIT_SUCCESS;
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

  switch (options.input_type) {
    case redsea::InputType::MPX_stdin:
    case redsea::InputType::MPX_sndfile:
      return ProcessMPXInput(options);
      break;

    case redsea::InputType::ASCIIbits:
      return ProcessASCIIBitsInput(options);
      break;

    case redsea::InputType::Hex:
      return ProcessHexInput(options);
      break;
  }
}
