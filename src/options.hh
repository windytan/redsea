/*
 * Copyright (c) Oona Räisänen
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
#ifndef OPTIONS_H_
#define OPTIONS_H_

#include <cstdint>
#include <string>
#include <vector>

namespace redsea {

enum class InputType { MPX_stdin, MPX_sndfile, ASCIIbits, Hex, TEF6686 };

enum class OutputType { Hex, JSON };

struct Options {
  bool rbds{};
  bool feed_thru{};
  bool show_partial{};
  bool exit_success{};
  bool print_usage{};
  bool print_version{};
  bool timestamp{};
  bool bler{};
  bool show_raw{};
  bool is_rate_defined{};
  bool is_num_channels_defined{};
  bool use_fec{true};
  bool streams{};
  float samplerate{};
  std::uint32_t num_channels{1};
  InputType input_type{InputType::MPX_stdin};
  OutputType output_type{OutputType::JSON};
  std::vector<std::string> loctable_dirs;
  std::string sndfilename;
  std::string time_format;
};

Options getOptions(int argc, char** argv);

}  // namespace redsea
#endif  // OPTIONS_H_
