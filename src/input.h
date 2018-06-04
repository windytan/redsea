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
#include <array>
#include <vector>

#include "config.h"

#include <sndfile.h>

#include "src/common.h"
#include "src/groups.h"
#include "src/options.h"

namespace redsea {

constexpr int   kInputChunkSize   = 4096;
constexpr float kMaxResampleRatio = kTargetSampleRate_Hz / kMinimumSampleRate_Hz;
constexpr int   kBufferSize       = kInputChunkSize * kMaxResampleRatio + 1;

template<int N=kBufferSize>
class MPXBuffer {
 public:
  std::array<float, N> data;
  size_t used_size;
};

class MPXReader {
 public:
  explicit MPXReader(const Options& options);
  ~MPXReader();
  bool eof() const;
  void FillBuffer();
  MPXBuffer<>& ReadChunk(int channel);
  float samplerate() const;
  int num_channels() const;

 private:
  int num_channels_;
  sf_count_t chunk_size_;
  bool is_eof_;
  bool feed_thru_;
  MPXBuffer<> buffer_;
  MPXBuffer<> buffer_singlechan_;
  SF_INFO sfinfo_;
  SNDFILE* file_;
  SNDFILE* outfile_;
  sf_count_t num_read_;
};

class AsciiBitReader {
 public:
  explicit AsciiBitReader(const Options& options);
  ~AsciiBitReader();
  bool ReadNextBit();
  bool eof() const;

 private:
  bool is_eof_;
  bool feed_thru_;
};

Group ReadNextHexGroup(const Options& options);

}  // namespace redsea
#endif  // INPUT_H_
