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
#include "src/common.h"

namespace redsea {

Options getOptions(int argc, char** argv) {
  Options options;
  int fec_flag{1};

  const struct option long_options[] = {
      {"input-bits",   no_argument,       nullptr,   'b'},
      {"channels",     required_argument, nullptr,   'c'},
      {"feed-through", no_argument,       nullptr,   'e'},
      {"bler",         no_argument,       nullptr,   'E'},
      {"file",         required_argument, nullptr,   'f'},
      {"input-hex",    no_argument,       nullptr,   'h'},
      {"input",        required_argument, nullptr,   'i'},
      {"loctable",     required_argument, nullptr,   'l'},
      {"output",       required_argument, nullptr,   'o'},
      {"show-partial", no_argument,       nullptr,   'p'},
      {"samplerate",   required_argument, nullptr,   'r'},
      {"show-raw",     no_argument,       nullptr,   'R'},
      {"timestamp",    required_argument, nullptr,   't'},
      {"rbds",         no_argument,       nullptr,   'u'},
      {"version",      no_argument,       nullptr,   'v'},
      {"output-hex",   no_argument,       nullptr,   'x'},
      {"no-fec",       no_argument,       &fec_flag, 0  },
      {"help",         no_argument,       nullptr,   '?'},
      {nullptr,        0,                 nullptr,   0  }
  };

  int option_index = 0;
  int option_char;

  while ((option_char = getopt_long(argc, argv, "bc:eEf:hi:l:o:pr:Rt:uvx", long_options,
                                    &option_index)) >= 0) {
    switch (option_char) {
      case 0:  // Flag
        break;
      case 'b':  // For backwards compatibility
        options.input_type = InputType::ASCIIbits;
        break;
      case 'c': {
        const int num_channels = std::atoi(optarg);
        if (num_channels < 1) {
          std::cerr << "error: number of channels must be greater than 0" << '\n';
          options.exit_failure = true;
        } else {
          options.num_channels = static_cast<uint32_t>(num_channels);
        }
        options.is_num_channels_defined = true;
        break;
      }
      case 'e': options.feed_thru = true; break;
      case 'E': options.bler = true; break;
      case 'f':
        options.sndfilename = std::string(optarg);
        options.input_type  = InputType::MPX_sndfile;
        break;
      case 'h':  // For backwards compatibility
        options.input_type = InputType::Hex;
        break;
      case 'i': {
        const std::string input_type(optarg);
        if (input_type == "hex") {
          options.input_type = InputType::Hex;
        } else if (input_type == "mpx") {
          options.input_type = InputType::MPX_stdin;
        } else if (input_type == "tef") {
          options.input_type = InputType::TEF6686;
        } else if (input_type == "bits") {
          options.input_type = InputType::ASCIIbits;
        } else {
          std::cerr << "error: unknown input format '" << input_type << "'" << std::endl;
          options.exit_failure = true;
        }
        break;
      }
      case 'o': {
        const std::string output_type(optarg);
        if (output_type == "hex") {
          options.output_type = OutputType::Hex;
        } else if (output_type == "json") {
          options.output_type = OutputType::JSON;
        } else {
          std::cerr << "error: unknown output format '" << output_type << "'" << std::endl;
          options.exit_failure = true;
        }
        break;
      }
      case 'x':  // For backwards compatibility
        options.output_type = OutputType::Hex;
        break;
      case 'p': options.show_partial = true; break;
      case 'r': {
        const std::string optstr(optarg);
        double factor = 1.0;
        if (optstr.size() > 1) {
          if (tolower(optstr.back()) == 'k')
            factor = 1000.0;
          else if (toupper(optstr.back()) == 'M')
            factor = 1000000.0;
        }
        options.samplerate = static_cast<float>(std::atof(optarg) * factor);
        if (options.samplerate < kMinimumSampleRate_Hz) {
          std::cerr << "error: sample rate was set to " << options.samplerate << ", but it must be "
                    << kMinimumSampleRate_Hz << " Hz or higher\n";
          options.exit_failure = true;
        }
        options.is_rate_defined = true;
        break;
      }
      case 'R': options.show_raw = true; break;
      case 't':
        options.timestamp   = true;
        options.time_format = std::string(optarg);
        break;
      case 'u': options.rbds = true; break;
      case 'l': options.loctable_dirs.push_back(std::string(optarg)); break;
      case 'v':
        options.print_version = true;
        options.exit_success  = true;
        break;
      case '?':
        options.print_usage  = true;
        options.exit_success = true;
        break;
      default:
        options.print_usage  = true;
        options.exit_failure = true;
        break;
    }
    if (options.exit_success)
      break;
  }

  options.use_fec = fec_flag;

  if (argc > optind) {
    options.print_usage  = true;
    options.exit_failure = true;
  }

  if (options.feed_thru && options.input_type == InputType::MPX_sndfile) {
    std::cerr << "error: feed-thru is not supported for audio file inputs" << '\n';
    options.exit_failure = true;
  }

  if (options.num_channels > 1 && options.input_type != InputType::MPX_stdin &&
      options.input_type != InputType::MPX_sndfile) {
    std::cerr << "error: multi-channel input is only supported for MPX signals" << '\n';
    options.exit_failure = true;
  }

  const bool assuming_raw_mpx{options.input_type == InputType::MPX_stdin && !options.print_usage &&
                              !options.exit_failure && !options.exit_success};

  if (assuming_raw_mpx && !options.is_rate_defined) {
    std::cerr << "{\"warning\":\"raw MPX sample rate not defined, assuming " << kTargetSampleRate_Hz
              << " Hz\"}" << '\n';
    options.samplerate = kTargetSampleRate_Hz;
  }

  return options;
}

}  // namespace redsea
