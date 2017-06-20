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
#include "src/common.h"
#include "src/block_sync.h"
#include "src/groups.h"

namespace redsea {

void PrintUsage() {
  std::cout <<
     "radio_command | ./src/redsea [OPTIONS]\n"
     "\n"
     "-b, --input-bits       Input is ASCII bit stream (011010110...)\n\n"
     "-e, --feed-through     Echo the input signal to stdout and print\n"
     "                       decoded groups to stderr\n\n"
     "-f, --file             Use an audio file as input\n\n"
     "-h, --input-hex        Input is hex groups in the RDS Spy format\n\n"
     "-l, --loctable DIR     Load TMC location table from a directory in TMC\n"
     "                       Exchange format\n\n"
     "-p, --show-partial     Display PS and RadioText before completely\n"
     "                       received (as partial_ps, partial_radiotext)\n\n"
     "-r, --samplerate       Set input sample frequency - will resample\n"
     "                       (slow) if this differs from 171000 Hz\n\n"
     "-t, --timestamp FORMAT Add time of decoding to JSON groups, see\n"
     "                       man strftime for formatting options (or\n"
     "                       try \"%c\")\n\n"
     "-u, --rbds             Use RBDS (North American) program types\n\n"
     "-v, --version          Print version\n\n"
     "-x, --output-hex       Output is hex groups in the RDS Spy format\n";
}

void PrintVersion() {
#ifdef DEBUG
  std::cout << PACKAGE_STRING << "-debug by OH2EIQ" << std::endl;
#else
  std::cout << PACKAGE_STRING << " by OH2EIQ" << std::endl;
#endif
}

Options GetOptions(int argc, char** argv) {
  redsea::Options options;

  static struct option long_options[] = {
    { "input-bits",    no_argument, 0, 'b'},
    { "feed-through",  no_argument, 0, 'e'},
    { "file",          1,           0, 'f'},
    { "input-hex",     no_argument, 0, 'h'},
    { "output-hex",    no_argument, 0, 'x'},
    { "show-partial",  no_argument, 0, 'p'},
    { "samplerate",    1,           0, 'r'},
    { "timestamp",     1,           0, 't'},
    { "rbds",          no_argument, 0, 'u'},
    { "help",          no_argument, 0, '?'},
    { "loctable",      1,           0, 'l'},
    { "version",       no_argument, 0, 'v'},
    {0,                0,           0,  0}};

  int option_index = 0;
  int option_char;

  while ((option_char = getopt_long(argc, argv, "bef:hl:pr:t:uvx", long_options,
         &option_index)) >= 0) {
    switch (option_char) {
      case 'b':
        options.input_type = redsea::INPUT_ASCIIBITS;
        break;
      case 'e':
        options.feed_thru = true;
        break;
      case 'f':
#ifdef HAVE_SNDFILE
        options.sndfilename = std::string(optarg);
        options.input_type = redsea::INPUT_MPX_SNDFILE;
#else
        std::cerr << "Sorry, redsea was built without libsndfile."
                  << std::endl;
        options.just_exit = true;
#endif
        break;
      case 'h':
        options.input_type = redsea::INPUT_RDSSPY;
        break;
      case 'x':
        options.output_type = redsea::OUTPUT_HEX;
        break;
      case 'p':
        options.show_partial = true;
        break;
      case 'r':
        options.samplerate = std::atoi(optarg);
        if (options.samplerate < 128000.f) {
          std::cerr << "error: sample rate must be 128000 Hz or higher"
                    << std::endl;
          options.just_exit = true;
        }
        break;
      case 't':
        options.timestamp = true;
        options.time_format = std::string(optarg);
        break;
      case 'u':
        options.rbds = true;
        break;
      case 'l':
        options.loctable_dir = std::string(optarg);
        break;
      case 'v':
        PrintVersion();
        options.just_exit = true;
        break;
      case '?':
      default:
        PrintUsage();
        options.just_exit = true;
        break;
    }
    if (options.just_exit)
      break;
  }

  if (options.feed_thru && options.input_type == INPUT_MPX_SNDFILE) {
    std::cerr << "error: feed-thru is not supported for audio file inputs"
              << std::endl;
    options.just_exit = true;
  }

  return options;
}

}  // namespace redsea

int main(int argc, char** argv) {
  redsea::Options options = redsea::GetOptions(argc, argv);

  if (options.just_exit)
    return EXIT_FAILURE;

#ifndef HAVE_LIQUID
  if (options.input_type == redsea::INPUT_MPX_STDIN ||
      options.input_type == redsea::INPUT_MPX_SNDFILE) {
    std::cerr << "can't demodulate MPX: redsea was compiled without liquid-dsp"
              << std::endl;
    return EXIT_FAILURE;
  }
#endif

  uint16_t pi = 0x0000, prev_new_pi = 0x0000, new_pi = 0x0000;
  redsea::BlockStream block_stream(options);
  redsea::Station station(pi, options);

  // Line buffering
  if (options.feed_thru)
    setvbuf(stderr, NULL, _IOLBF, 2048);
  else
    setvbuf(stdout, NULL, _IOLBF, 2048);

  while (!(std::cin.eof() || block_stream.eof())) {
    redsea::Group group = (options.input_type == redsea::INPUT_RDSSPY ?
        redsea::NextGroupRSpy(options.feed_thru) :
        block_stream.NextGroup());

    if (group.has_pi()) {
      // Repeated PI confirms change
      prev_new_pi = new_pi;
      new_pi = group.pi();

      if (new_pi == prev_new_pi || options.input_type == redsea::INPUT_RDSSPY) {
        if (new_pi != pi)
          station = redsea::Station(new_pi, options);
        pi = new_pi;
      } else {
        continue;
      }
    }

#ifdef DEBUG
    printf("b:%f,", block_stream.t());
#endif

    if (options.output_type == redsea::OUTPUT_HEX) {
      redsea::PrintHexGroup(group, options.feed_thru ? &std::cerr : &std::cout);
    } else {
      station.UpdateAndPrint(group, options.feed_thru ?
                             &std::cerr : &std::cout);
    }
  }

  return EXIT_SUCCESS;
}
