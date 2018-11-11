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
#include "src/options.h"

#include <getopt.h>
#include <iostream>

#include "config.h"

namespace redsea {

Options GetOptions(int argc, char** argv) {
  Options options;

  static struct option long_options[] = {
    { "input-bits",    no_argument, 0, 'b'},
    { "channels",      1,           0, 'c'},
    { "feed-through",  no_argument, 0, 'e'},
    { "bler",          no_argument, 0, 'E'},
    { "file",          1,           0, 'f'},
    { "input-hex",     no_argument, 0, 'h'},
    { "loctable",      1,           0, 'l'},
    { "show-partial",  no_argument, 0, 'p'},
    { "samplerate",    1,           0, 'r'},
    { "timestamp",     1,           0, 't'},
    { "rbds",          no_argument, 0, 'u'},
    { "version",       no_argument, 0, 'v'},
    { "output-hex",    no_argument, 0, 'x'},
    { "help",          no_argument, 0, '?'},
    {0,                0,           0,  0}};

  int option_index = 0;
  int option_char;

  while ((option_char = getopt_long(argc, argv, "bc:eEf:hl:pr:t:uvx",
                                    long_options,
         &option_index)) >= 0) {
    switch (option_char) {
      case 'b':
        options.input_type = InputType::ASCIIbits;
        break;
      case 'c':
        options.num_channels = std::atoi(optarg);
        if (options.num_channels < 1) {
          std::cerr << "error: number of channels must be greater than 0"
                    << '\n';
          options.exit_failure = true;
        }
        break;
      case 'e':
        options.feed_thru = true;
        break;
      case 'E':
        options.bler = true;
        break;
      case 'f':
        options.sndfilename = std::string(optarg);
        options.input_type = InputType::MPX_sndfile;
        break;
      case 'h':
        options.input_type = InputType::Hex;
        break;
      case 'x':
        options.output_type = OutputType::Hex;
        break;
      case 'p':
        options.show_partial = true;
        break;
      case 'r': {
        std::string optstr(optarg);
        double factor = 1.0;
        if (optstr.size() > 1) {
          if (tolower(optstr.back()) == 'k')
            factor = 1000.0;
          else if (toupper(optstr.back()) == 'M')
            factor = 1000000.0;
        }
        options.samplerate = std::atof(optarg) * factor;
        if (options.samplerate < kMinimumSampleRate_Hz) {
          std::cerr << "error: sample rate must be " << kMinimumSampleRate_Hz
                    << " Hz or higher\n";
          options.exit_failure = true;
        }
        break;
      }
      case 't':
        options.timestamp = true;
        options.time_format = std::string(optarg);
        break;
      case 'u':
        options.rbds = true;
        break;
      case 'l':
        options.loctable_dirs.push_back(std::string(optarg));
        break;
      case 'v':
        options.print_version = true;
        options.exit_success = true;
        break;
      case '?':
        options.print_usage = true;
        options.exit_success = true;
        break;
      default:
        options.print_usage = true;
        options.exit_failure = true;
        break;
    }
    if (options.exit_success)
      break;
  }

  if (options.feed_thru && options.input_type == InputType::MPX_sndfile) {
    std::cerr << "error: feed-thru is not supported for audio file inputs"
      << '\n';
    options.exit_failure = true;
  }

  if (options.num_channels > 1 && options.input_type != InputType::MPX_stdin &&
      options.input_type != InputType::MPX_sndfile) {
    std::cerr << "error: multi-channel input is only supported for MPX signals"
              << '\n';
    options.exit_failure = true;
  }

  return options;
}

}  // namespace redsea
