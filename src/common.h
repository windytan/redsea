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
#ifndef COMMON_H_
#define COMMON_H_

#include <string>

namespace redsea {

const float kTargetSampleRate_Hz  = 171000.0f;

enum eInputType {
  INPUT_MPX_STDIN, INPUT_MPX_SNDFILE, INPUT_ASCIIBITS, INPUT_RDSSPY
};

enum eOutputType {
  OUTPUT_HEX, OUTPUT_JSON
};

enum eCodeTable {
  G0, G1, G2
};

enum eOpenDataApp {
  ODA_UNKNOWN, ODA_TMC, ODA_RTPLUS
};

struct Options {
  Options() : rbds(false), feed_thru(false), show_partial(false),
              just_exit(false), timestamp(false),
              samplerate(kTargetSampleRate_Hz),
              input_type(INPUT_MPX_STDIN), output_type(OUTPUT_JSON) {}
  bool rbds;
  bool feed_thru;
  bool show_partial;
  bool just_exit;
  bool timestamp;
  float samplerate;
  eInputType input_type;
  eOutputType output_type;
  std::string loctable_dir;
  std::string sndfilename;
  std::string time_format;
};

}  // namespace redsea
#endif  // COMMON_H_
