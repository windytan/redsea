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
#include <cstdint>
#include <exception>
#include <iostream>
#include <memory>
#include <vector>

#include "config.h"
#include "src/channel.hh"
#include "src/dsp/subcarrier.hh"
#include "src/groups.hh"
#include "src/io/input.hh"
#include "src/options.hh"

namespace redsea {

void printUsage() {
  std::cout
      << "radio_command | redsea [OPTIONS]\n"
         "redsea [OPTIONS] -r <samplerate> < raw_signal_file.s16\n\n"
         "-b, --input-bits       Same as --input bits (for backwards compatibility).\n"
         "\n"
         "-c, --channels CHANS   Number of channels in the raw input signal. Channels are\n"
         "                       interleaved streams of samples that are demodulated\n"
         "                       independently.\n"
         "\n"
         "-e, --feed-through     Echo the input signal to stdout and print decoded groups\n"
         "                       to stderr. This only works for raw PCM.\n"
         "\n"
         "-E, --bler             Display the average block error rate, or the percentage\n"
         "                       of blocks that had errors before error correction.\n"
         "                       Averaged over the last 12 groups. For hex input, this is\n"
         "                       the percentage of missing blocks.\n"
         "\n"
         "-f, --file FILENAME    Read MPX input from a wave file with headers (.wav,\n"
         "                       .flac, ...). If you have headered wave data via stdin,\n"
         "                       use '-'. Or you can specify another format with --input.\n"
         "\n"
         "-h, --input-hex        Same as --input hex (for backwards compatibility).\n"
         "\n"
         "-i, --input FORMAT     Decode input as FORMAT (see the redsea wiki in github\n"
         "                       for more info).\n"
         "                         bits Unsynchronized ASCII bit stream (01101011...). All "
         "characters\n"
         "                              but '0' and '1' are ignored.\n"
         "                         hex  RDS Spy hex format. (Timestamps will be ignored)\n"
         "                         mpx  MPX as raw mono S16LE PCM. Remember to also specify\n"
         "                              --samplerate. If you're reading from a sound file with "
         "headers\n"
         "                              (WAV, FLAC, ...) don't specify this.\n"
         "                         tef  Serial data from the TEF6686 tuner.\n"
         "\n"
         "-l, --loctable DIR     Load TMC location table from a directory in TMC Exchange\n"
         "                       format. This option can be specified multiple times to\n"
         "                       load several location tables.\n"
         "\n"
         "--no-fec               Disable forward error correction; always reject blocks\n"
         "                       with incorrect syndromes. In noisy conditions, fewer errors\n"
         "                       will slip through, but also fewer blocks in total. See wiki\n"
         "                       for discussion.\n"
         "\n"
         "-o, --output FORMAT    Print output as FORMAT:\n"
         "                         hex  RDS Spy hex format.\n"
         "                         json Newline-delimited JSON (default).\n"
         "\n"
         "-p, --show-partial     Under noisy conditions, redsea may not be able to fully\n"
         "                       receive all information. Multi-group data such as PS\n"
         "                       names, RadioText, and alternative frequencies are\n"
         "                       especially vulnerable. This option makes it display them\n"
         "                       even if not fully received, prefixed with partial_.\n"
         "\n"
         "-r, --samplerate RATE  Set sample frequency of raw PCM input in Hz. Will\n"
         "                       resample if this differs from 171000 Hz.\n"
         "\n"
         "-R, --show-raw         Include raw group data as hex in the JSON stream.\n"
         "\n"
         "-s, --streams          Decode RDS2 data streams 1, 2, and 3, if they exist.\n"
         "\n"
         "-t, --timestamp FORMAT Add time of decoding to JSON groups; see man strftime\n"
         "                       for formatting options (or try \"%c\"). Use \"%f\" to add\n"
         "                       hundredths of seconds.\n"
         "\n"
         "-u, --rbds             RBDS mode; use North American program type names and\n"
         "                       \"back-calculate\" the station's call sign from its PI\n"
         "                       code. Note that this calculation gives an incorrect call\n"
         "                       sign for most stations that transmit TMC.\n"
         "\n"
         "-v, --version          Print version string and exit.\n"
         "\n"
         "-x, --output-hex       Same as --output hex (for backwards compatibility).\n"
         "\n";
}

void printVersion() {
#ifdef DEBUG
  std::cout << "redsea " << VERSION << "-debug by OH2EIQ" << '\n';
#else
  std::cout << "redsea " << VERSION << " by OH2EIQ" << '\n';
#endif
}

// \return Process exit code
int processMPXInput(Options options) {
  MPXReader mpx;

  try {
    mpx.init(options);
  } catch (BeyondEofError&) {
    printUsage();
    return EXIT_FAILURE;
  } catch (const std::exception& e) {
    std::cerr << "error: Can't open audio file: " << e.what() << "\n";
    return EXIT_FAILURE;
  }

  options.samplerate   = mpx.getSamplerate();
  options.num_channels = mpx.getNumChannels();

  auto& output_stream = options.feed_thru ? std::cerr : std::cout;

  const int num_data_streams = options.streams ? 4 : 1;

  // Each PCM channel is matched with 1 subcarrier set
  std::vector<std::unique_ptr<Channel>> channels;
  std::vector<std::unique_ptr<SubcarrierSet>> subcarriers;
  for (std::uint32_t ch = 0; ch < options.num_channels; ch++) {
    channels.emplace_back(std::make_unique<Channel>(options, ch, output_stream));
    subcarriers.push_back(std::make_unique<SubcarrierSet>(options.samplerate));
  }

  while (!mpx.eof()) {
    mpx.fillBuffer();
    for (std::uint32_t ch = 0; ch < options.num_channels; ch++) {
      const auto bits = subcarriers[ch]->processChunk(mpx.readChunk(ch), num_data_streams);
      for (int n_stream = 0; n_stream < num_data_streams; n_stream++) {
        channels[ch]->processBits(bits, n_stream);
        if (channels[ch]->getSecondsSinceCarrierLost() > 10.f &&
            subcarriers[ch]->getSecondsSinceLastReset() > 5.f) {
          subcarriers[ch]->reset();
          channels[ch]->resetPI();
        }
      }
    }
  }

  for (std::uint32_t ch = 0; ch < options.num_channels; ch++) channels[ch]->flush();

  return EXIT_SUCCESS;
}

// \return Process exit code
int processASCIIBitsInput(const Options& options) {
  Channel channel(options, 0, options.feed_thru ? std::cerr : std::cout);
  AsciiBitReader ascii_reader(options);

  while (!ascii_reader.eof()) {
    channel.processBit(ascii_reader.readBit(), 0);
  }

  channel.flush();

  return EXIT_SUCCESS;
}

int processHexInput(const Options& options) {
  Channel channel(options, 0, options.feed_thru ? std::cerr : std::cout);

  while (!std::cin.eof()) {
    const auto group = readHexGroup(options);
    channel.processGroup(group, group.getDataStream());
  }

  return EXIT_SUCCESS;
}

// \return Process exit code
int processTEFInput(const Options& options) {
  Channel channel(options, 0, options.feed_thru ? std::cerr : std::cout);

  while (!std::cin.eof()) {
    channel.processGroup(readTEFGroup(options), 0);
  }

  return EXIT_SUCCESS;
}

}  // namespace redsea

int main(int argc, char** argv) {
  redsea::Options options;

  try {
    options = redsea::getOptions(argc, argv);
  } catch (const std::exception& e) {
    std::cerr << "error: " << e.what() << std::endl;
    return EXIT_FAILURE;
  }

  if (options.print_usage) {
    redsea::printUsage();
  }

  if (options.print_version) {
    redsea::printVersion();
  }

  if (options.early_exit) {
    return options.init_error ? EXIT_FAILURE : EXIT_SUCCESS;
  }

  switch (options.input_type) {
    case redsea::InputType::MPX_stdin:
    case redsea::InputType::MPX_sndfile: return processMPXInput(options);
    case redsea::InputType::ASCIIbits:   return processASCIIBitsInput(options);
    case redsea::InputType::Hex:         return processHexInput(options);
    case redsea::InputType::TEF6686:     return processTEFInput(options);
  }
}
