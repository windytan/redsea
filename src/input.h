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
#ifndef INPUT_H_
#define INPUT_H_

#include <cstdint>
#include <vector>

#include "config.h"

#ifdef HAVE_SNDFILE
#include <sndfile.h>
#endif

#include "src/groups.h"

namespace redsea {

class MPXReader {
 public:
  bool eof() const;
  virtual std::vector<float> ReadBlock() = 0;
  virtual float samplerate() const = 0;

 protected:
  bool is_eof_;
};

class StdinReader : public MPXReader {
 public:
  explicit StdinReader(const Options& options);
  ~StdinReader();
  std::vector<float> ReadBlock() override;
  float samplerate() const override;

 private:
  float samplerate_;
  int16_t* buffer_;
  bool feed_thru_;
};

#ifdef HAVE_SNDFILE
class SndfileReader : public MPXReader {
 public:
  explicit SndfileReader(const Options& options);
  ~SndfileReader();
  std::vector<float> ReadBlock() override;
  float samplerate() const override;

 private:
  SF_INFO info_;
  SNDFILE* file_;
  float* buffer_;
};
#endif

class AsciiBits {
 public:
  explicit AsciiBits(const Options& options);
  ~AsciiBits();
  int NextBit();
  bool eof() const;

 private:
  bool is_eof_;
  bool feed_thru_;
};

Group ReadNextHexGroup(const Options& options);

}  // namespace redsea
#endif  // INPUT_H_
