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

#include <array>
#include <cstdint>
#include <exception>

#include <sndfile.h>

#include "src/common.h"
#include "src/groups.h"
#include "src/options.h"

namespace redsea {

constexpr int kInputChunkSize     = 8192;
constexpr float kMaxResampleRatio = kTargetSampleRate_Hz / kMinimumSampleRate_Hz;
constexpr int kBufferSize         = static_cast<int>(kInputChunkSize * kMaxResampleRatio) + 1;

template <int N = kBufferSize>
class MPXBuffer {
 public:
  std::array<float, N> data{};
  size_t used_size{};
  std::chrono::time_point<std::chrono::system_clock> time_received;
};

class BeyondEofError : std::exception {
 public:
  BeyondEofError() {};
};

class MPXReader {
 public:
  MPXReader() = default;
  ~MPXReader();
  void init(const Options& options);
  bool eof() const;
  bool hasError() const;
  void fillBuffer();
  MPXBuffer<>& readChunk(int channel);
  float getSamplerate() const;
  int getNumChannels() const;

 private:
  int num_channels_{0};
  sf_count_t chunk_size_{0};
  bool is_eof_{true};
  bool is_error_{false};
  bool feed_thru_{false};
  std::string filename_{""};
  MPXBuffer<> buffer_{};
  MPXBuffer<> buffer_singlechan_{};
  SF_INFO sfinfo_{0, 0, 0, 0, 0, 0};
  SNDFILE* file_{nullptr};
  SNDFILE* outfile_{nullptr};
  sf_count_t num_read_{0};
};

class AsciiBitReader {
 public:
  explicit AsciiBitReader(const Options& options);
  ~AsciiBitReader() = default;
  bool readBit();
  bool eof() const;

 private:
  bool is_eof_{false};
  bool feed_thru_{false};
};

Group readHexGroup(const Options& options);
Group readTEFGroup(const Options& options);

}  // namespace redsea
#endif  // INPUT_H_
