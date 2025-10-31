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
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>

#include "src/constants.hh"
#include "src/util/maybe.hh"

namespace redsea {

namespace {

void warn(const std::string& message) {
  static_cast<void>(std::fprintf(stderr, "redsea: warning: %s\n", message.c_str()));
}

// \brief Parse a possibly SI-suffixed number out of a C-string.
template <typename T>
Maybe<T> parseSI(char* cstr) {
  static_assert(std::is_same_v<T, float> || std::is_same_v<T, std::int32_t>);
  if (cstr == nullptr || *cstr == '\0') {
    return {};
  }

  errno        = 0;
  char* endptr = cstr;
  T value;
  if constexpr (std::is_same_v<T, float>)
    value = std::strtof(cstr, &endptr);
  else
    value = std::strtol(cstr, &endptr, 10);

  if (errno == ERANGE) {
    // Value out of range
    return {};
  }

  T factor = 1;
  if (std::tolower(*endptr) == 'k') {
    factor = 1000;
  } else if (std::toupper(*endptr) == 'M') {
    factor = 1'000'000;
  } else if (*endptr != '\0') {
    // Extra characters
    return {};
  }

  return Maybe<T>{value * factor};
}

}  // namespace

/// @throws std::runtime_error for option validation errors
/// @note May also print a warning to stderr
Options getOptions(int argc, char** argv) {
  Options options;
  int fec_flag{1};
  int time_offset_flag{0};
  int help_flag{0};
  bool has_custom_input_type{};

  // Channels take up memory, and we don't want to fill it up by accident.
  constexpr int kMaxNumChannels{32};

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
      {"help",         no_argument,       &help_flag,   1},
      {nullptr,        0,                 nullptr,   0  }
  }};
  // clang-format on

  int option_index{};
  int option_char{};

  while ((option_char = ::getopt_long(argc, argv, "bc:eEf:hi:l:o:pr:Rst:uvx", long_options.data(),
                                      &option_index)) >= 0) {
    switch (option_char) {
      // Some flag
      case 0: break;
      case 'b':  // For backwards compatibility
        options.input_type    = InputType::ASCIIbits;
        has_custom_input_type = true;
        break;
      case 'c': {
        const auto parsed_channels = parseSI<std::int32_t>(optarg);
        if (!parsed_channels.has_value || parsed_channels.value <= 0 ||
            parsed_channels.value > kMaxNumChannels) {
          throw std::runtime_error("check the number of channels");
        }
        options.num_channels            = static_cast<std::uint32_t>(parsed_channels.value);
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
        options.input_type    = InputType::Hex;
        has_custom_input_type = true;
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
        has_custom_input_type = true;
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
      case 'x': options.output_type = OutputType::Hex; break;  // For backwards compatibility
      case 'p': options.show_partial = true; break;
      case 'r': {
        const auto parsed_rate = parseSI<float>(optarg);
        if (!parsed_rate.has_value) {
          throw std::runtime_error("check the sample rate parameter");
        }
        if (parsed_rate.value < kMinimumSampleRate_Hz ||
            parsed_rate.value > kMaximumSampleRate_Hz) {
          throw std::runtime_error("sample rate was set to " + std::to_string(parsed_rate.value) +
                                   ", but it must be between " +
                                   std::to_string(kMinimumSampleRate_Hz) + " and " +
                                   std::to_string(kMaximumSampleRate_Hz) + " Hz");
        }
        options.samplerate             = parsed_rate.value;
        options.is_custom_rate_defined = true;
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
      case '?':
      default:
        options.print_usage = true;
        options.init_error  = true;
        break;
    }
    if (options.init_error)
      break;
  }

  options.early_exit      = options.print_usage || options.print_version;
  options.use_fec         = (fec_flag == 1);
  options.time_from_start = (time_offset_flag == 1);

  if (argc > optind) {
    options.print_usage = true;
    options.init_error  = true;
    options.early_exit  = true;
  }

  if (help_flag == 1) {
    options.print_usage = true;
    options.early_exit  = true;
  }

  //
  // Fatal validation errors - we don't know what the user asked redsea to do
  //

  if (has_custom_input_type && !options.sndfilename.empty()) {
    throw std::runtime_error("incompatible options: --input and --file");
  }

  if (options.feed_thru && options.input_type == InputType::MPX_sndfile) {
    throw std::runtime_error("feed-thru is not supported for MPX file input (try via stdin)");
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
    throw std::runtime_error("--time-from-start only works for MPX input");
  }

  //
  // Warnings - we can start the program, but results may be surprising!
  // https://en.wikipedia.org/wiki/Principle_of_least_astonishment
  //

  if (!options.use_fec &&
      (options.input_type == InputType::Hex || options.input_type == InputType::TEF6686)) {
    warn("--no-fec ignored for hex or tef6686 input");
  }

  if (options.show_partial && options.output_type == OutputType::Hex) {
    warn("--show-partial ignored for hex output");
  }

  if (options.show_raw && options.output_type == OutputType::Hex) {
    warn("--show-raw ignored for hex output");
  }

  if (!options.loctable_dirs.empty() && options.output_type == OutputType::Hex) {
    warn("--loctable ignored for hex output");
  }

  if (options.bler && options.output_type == OutputType::Hex) {
    warn("--bler ignored for hex output");
  }

  // --rbds doesn't have any effect for hex output either, but we choose not to warn about it

  if (options.is_custom_rate_defined) {
    if (options.input_type != InputType::MPX_stdin &&
        options.input_type != InputType::MPX_sndfile) {
      // Strictly, it's only supported for raw PCM, but we'll print it as a warning from input.cc
      throw std::runtime_error("sample rate is only supported for MPX input");
    }
  }

  const bool assuming_raw_mpx{options.input_type == InputType::MPX_stdin && !options.print_usage &&
                              !options.print_version && !options.init_error};

  if (assuming_raw_mpx && !options.is_custom_rate_defined) {
    warn("raw MPX sample rate not defined, assuming " +
         std::to_string(static_cast<int>(kTargetSampleRate_Hz)) + " Hz");

    options.samplerate = kTargetSampleRate_Hz;
  }

  if (options.streams && options.input_type == InputType::Hex) {
    warn("--streams has no effect for hex input (streams are read automatically)");
  }

  return options;
}

}  // namespace redsea
