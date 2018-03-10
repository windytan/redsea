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
#include "src/input.h"

#include <cassert>
#include <iostream>
#include <stdexcept>
#include <string>

#include "src/groups.h"
#include "src/util.h"

namespace redsea {

/*
 * An MPXReader deals with reading an FM multiplex signal from an audio file or
 * raw PCM via stdin, separating it into channels and converting to chunks of
 * floating-point samples.
 *
 */
MPXReader::MPXReader(const Options& options) :
    input_type_(options.input_type),
    feed_thru_(options.feed_thru),
    sfinfo_({0, 0, 0, 0, 0, 0}) {
  is_eof_ = false;

  if (options.input_type == INPUT_MPX_STDIN ||
      options.input_type == INPUT_MPX_SNDFILE) {

    if (options.input_type == INPUT_MPX_STDIN) {
      sfinfo_.channels = 1;
      sfinfo_.format = SF_FORMAT_RAW | SF_FORMAT_PCM_16;
      sfinfo_.samplerate = options.samplerate;
      sfinfo_.frames = 0;
      file_ = sf_open_fd(fileno(stdin), SFM_READ, &sfinfo_, SF_TRUE);

      outfile_ = sf_open_fd(fileno(stdout), SFM_WRITE, &sfinfo_, SF_TRUE);
    } else if (options.input_type == INPUT_MPX_SNDFILE) {
      file_ = sf_open(options.sndfilename.c_str(), SFM_READ, &sfinfo_);
    }

    if (file_ == nullptr) {
      int err = sf_error (file_) ;
      std::cerr << "error: failed to open file: " << sf_error_number(err);
      is_eof_ = true;
    } else if (sfinfo_.samplerate < 128000.f) {
      std::cerr << "error: sample rate must be 128000 Hz or higher" << '\n';
      is_eof_ = true;
    } else {
      assert(sfinfo_.channels < static_cast<int>(buffer_.size()));
      used_buffer_size_ =
          (buffer_.size() / sfinfo_.channels) * sfinfo_.channels;
    }
  }
}

MPXReader::~MPXReader() {
  sf_close(file_);
}

bool MPXReader::eof() const {
  return is_eof_;
}

std::vector<float> MPXReader::ReadChunk() {
  std::vector<float> chunk;
  if (is_eof_)
    return chunk;

  sf_count_t num_read =
      sf_read_float(file_, buffer_.data(), used_buffer_size_);
  if (num_read < static_cast<long long>(used_buffer_size_))
    is_eof_ = true;

  if (sfinfo_.channels == 1) {
    chunk = std::vector<float>(buffer_.begin(), buffer_.end());
  } else {
    chunk = std::vector<float>(num_read / sfinfo_.channels);
    for (size_t i = 0; i < chunk.size(); i++)
      chunk[i] = buffer_[i * sfinfo_.channels];
  }
  return chunk;
}

float MPXReader::samplerate() const {
  return sfinfo_.samplerate;
}

AsciiBitReader::AsciiBitReader(const Options& options) :
    is_eof_(false), feed_thru_(options.feed_thru) {
}

AsciiBitReader::~AsciiBitReader() {
}

bool AsciiBitReader::NextBit() {
  int chr = 0;
  while (chr != '0' && chr != '1' && chr != EOF) {
    chr = getchar();
    if (feed_thru_)
      putchar(chr);
  }

  if (chr == EOF)
    is_eof_ = true;

  return (chr == '1');
}

bool AsciiBitReader::eof() const {
  return is_eof_;
}

/*
 * Read a single line containing an RDS group in the RDS Spy hex format.
 *
 */
Group ReadNextHexGroup(const Options& options) {
  Group group;
  group.disable_offsets();

  bool finished = false;

  while (!(finished || std::cin.eof())) {
    std::string line;
    std::getline(std::cin, line);
    if (options.feed_thru)
      std::cout << line << '\n';

    if (line.length() < 16)
      continue;

    for (eBlockNumber block_num : {BLOCK1, BLOCK2, BLOCK3, BLOCK4}) {
      uint16_t block_data = 0;
      bool block_still_valid = true;

      int nyb = 0;
      while (nyb < 4) {
        if (line.length() < 1) {
          finished = true;
          break;
        }

        std::string single = line.substr(0, 1);

        if (single != " ") {
          try {
            int nval = std::stoi(std::string(single), nullptr, 16);
            block_data = (block_data << 4) + nval;
          } catch (std::invalid_argument) {
            block_still_valid = false;
          }
          nyb++;
        }
        line = line.substr(1);
      }

      if (block_still_valid)
        group.set(block_num, block_data);

      if (block_num == BLOCK4)
        finished = true;
    }
  }

  if (options.timestamp)
    group.set_time(std::chrono::system_clock::now());

  return group;
}

}  // namespace redsea
