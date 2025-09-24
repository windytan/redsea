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
#include "src/options.hh"

#include <getopt.h>

#include <array>
#include <cctype>
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

#include "src/constants.hh"

namespace redsea {

/// @throws std::runtime_error for option validation errors
/// @note May also print a warning to stderr
Options getOptions(int argc, char** argv) {
  Options options;
  int fec_flag{1};
  int time_offset_flag{0};

  // clang-format off
  const std::array<option, 21> long_options{{
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
      {"streams",      no_argument,       nullptr,   's'},
      {"version",      no_argument,       nullptr,   'v'},
      {"output-hex",   no_argument,       nullptr,   'x'},
      {"no-fec",       no_argument,       &fec_flag, 0  },
      {"time-from-start", no_argument,    &time_offset_flag,   1},
      {"help",         no_argument,       nullptr,   '?'},
      {nullptr,        0,                 nullptr,   0  }
  }};
  // clang-format on

  int option_index{};
  int option_char{};

  while ((option_char = ::getopt_long(argc, argv, "bc:eEf:hi:l:o:pr:Rst:uvx", long_options.data(),
                                      &option_index)) >= 0) {
    switch (option_char) {
      case 0:  // Flag
        break;
      case 'b':  // For backwards compatibility
        options.input_type = InputType::ASCIIbits;
        break;
      case 'c': {
        options.num_channels = static_cast<std::uint32_t>(std::strtoul(optarg, nullptr, 10));
        // Channels take up memory, and we don't want to fill it up by accident.
        constexpr int kMaxNumChannels{32};
        if (errno == ERANGE || options.num_channels < 1 || options.num_channels > kMaxNumChannels) {
          throw std::runtime_error("check the number of channels");
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
          throw std::runtime_error("unknown input format '" + input_type + "'");
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
          throw std::runtime_error("unknown output format '" + output_type + "'");
        }
        break;
      }
      case 'x':  // For backwards compatibility
        options.output_type = OutputType::Hex;
        break;
      case 'p': options.show_partial = true; break;
      case 'r': {
        const std::string optstr(optarg);
        float factor = 1.f;
        if (optstr.size() > 1) {
          if (std::tolower(optstr.back()) == 'k')
            factor = 1e3f;
          else if (std::toupper(optstr.back()) == 'M')
            factor = 1e6f;
        }
        options.samplerate = std::strtof(optarg, nullptr) * factor;
        if (errno == ERANGE) {
          throw std::runtime_error("check the sample rate");
        }
        if (options.samplerate < kMinimumSampleRate_Hz ||
            options.samplerate > kMaximumSampleRate_Hz) {
          throw std::runtime_error("sample rate was set to " + std::to_string(options.samplerate) +
                                   ", but it must be between " +
                                   std::to_string(kMinimumSampleRate_Hz) + " and " +
                                   std::to_string(kMaximumSampleRate_Hz) + " Hz\n");
        }
        options.is_rate_defined = true;
        break;
      }
      case 'R': options.show_raw = true; break;
      case 's': options.streams = true; break;
      case 't':
        options.timestamp   = true;
        options.time_format = std::string(optarg);
        break;
      case 'u': options.rbds = true; break;
      case 'l': options.loctable_dirs.emplace_back(optarg); break;
      case 'v': options.print_version = true; break;
      case '?': options.print_usage = true; break;
      default:
        options.print_usage = true;
        options.init_error  = true;
        break;
    }
    if (options.init_error)
      break;
  }

  options.early_exit      = options.print_usage || options.print_version;
  options.use_fec         = fec_flag;
  options.time_from_start = time_offset_flag;

  if (argc > optind) {
    options.print_usage = true;
    options.init_error  = true;
    options.early_exit  = true;
  }

  if (options.feed_thru && options.input_type == InputType::MPX_sndfile) {
    throw std::runtime_error("feed-thru is not supported for audio file inputs");
  }

  if (options.num_channels > 1 && options.input_type != InputType::MPX_stdin &&
      options.input_type != InputType::MPX_sndfile) {
    throw std::runtime_error("multi-channel input is only supported for MPX signals");
  }

  if (options.streams && options.input_type != InputType::MPX_sndfile &&
      options.input_type != InputType::MPX_stdin && options.input_type != InputType::Hex) {
    throw std::runtime_error("RDS2 data streams are only supported for MPX and hex input");
  }

  if (options.time_from_start && options.input_type != InputType::MPX_stdin &&
      options.input_type != InputType::MPX_sndfile) {
    throw std::runtime_error("Time from start can only be shown for MPX input");
  }

  const bool assuming_raw_mpx{options.input_type == InputType::MPX_stdin && !options.print_usage &&
                              !options.print_version && !options.init_error};

  if (assuming_raw_mpx && !options.is_rate_defined) {
    std::cerr << R"(warning: raw MPX sample rate not defined, assuming )" << kTargetSampleRate_Hz
              << R"( Hz)" << std::endl;
    options.samplerate = kTargetSampleRate_Hz;
  }

  return options;
}

}  // namespace redsea
