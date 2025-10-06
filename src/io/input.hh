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
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <string>

#include <sndfile.h>

#include "src/constants.hh"
#include "src/groups.hh"

namespace redsea {

struct Options;

// Read this many samples at a time
constexpr std::size_t kInputChunkSize = 8192;

// MPX buffer size allows expansion due to resampling
constexpr auto kBufferSize = static_cast<std::size_t>(kInputChunkSize * kMaxResampleRatio) + 1;

class MPXBuffer {
 public:
  std::array<float, kBufferSize> data{};
  std::size_t used_size{};
  std::chrono::time_point<std::chrono::system_clock> time_received;
};

class BeyondEofError : std::exception {
 public:
  BeyondEofError() = default;
};

class MPXReader {
 public:
  MPXReader()                             = default;
  MPXReader(const MPXReader&)             = delete;
  MPXReader& operator=(const MPXReader&)  = delete;
  MPXReader(MPXReader&& other)            = delete;
  MPXReader& operator=(MPXReader&& other) = delete;
  ~MPXReader();
  void init(const Options& options);
  bool eof() const;
  void fillBuffer();
  MPXBuffer& readChunk(std::uint32_t channel);
  float getSamplerate() const;
  std::uint32_t getNumChannels() const;

 private:
  std::uint32_t num_channels_{};
  sf_count_t chunk_size_{};
  bool is_eof_{true};
  bool feed_thru_{false};
  std::string filename_;
  MPXBuffer buffer_{};
  MPXBuffer buffer_singlechan_{};
  SF_INFO sfinfo_{0, 0, 0, 0, 0, 0};
  SNDFILE* file_{nullptr};
  SNDFILE* outfile_{nullptr};
  // How many samples was read, before dividing into channels
  sf_count_t num_read_{};
};

class AsciiBitReader {
 public:
  explicit AsciiBitReader(const Options& options);
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
