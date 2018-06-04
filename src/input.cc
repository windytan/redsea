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
    num_channels_(options.num_channels),
    is_eof_(true),
    feed_thru_(options.feed_thru),
    sfinfo_({0, 0, 0, 0, 0, 0}),
    file_(nullptr) {

  if (options.input_type != InputType::MPX_stdin &&
      options.input_type != InputType::MPX_sndfile)
    return;

  if (options.input_type == InputType::MPX_stdin) {
    sfinfo_.channels = 1;
    sfinfo_.format = SF_FORMAT_RAW | SF_FORMAT_PCM_16;
    sfinfo_.samplerate = options.samplerate;
    sfinfo_.frames = 0;
    file_ = sf_open_fd(fileno(stdin), SFM_READ, &sfinfo_, SF_TRUE);
    outfile_ = sf_open_fd(fileno(stdout), SFM_WRITE, &sfinfo_, SF_TRUE);
  } else if (options.input_type == InputType::MPX_sndfile) {
    file_ = sf_open(options.sndfilename.c_str(), SFM_READ, &sfinfo_);
    num_channels_ = sfinfo_.channels;
  }

  chunk_size_ = (kInputChunkSize / num_channels_) * num_channels_;

  if (!file_) {
    std::cerr << "error: failed to open file: " <<
              sf_error_number(sf_error(file_)) << '\n';
    exit(EXIT_FAILURE);
  } else if (sfinfo_.samplerate < kMinimumSampleRate_Hz) {
    std::cerr << "error: sample rate must be " << kMinimumSampleRate_Hz
              << " Hz or higher\n";
    exit(EXIT_FAILURE);
  } else {
    assert(num_channels_ < static_cast<int>(buffer_.data.size()));
    is_eof_ = false;
  }
}

MPXReader::~MPXReader() {
  sf_close(file_);
}

bool MPXReader::eof() const {
  return is_eof_;
}

/*
 * Fill the internal buffer with fresh samples. This should be called before
 * the first channel is processed via ReadChunk().
 *
 */
void MPXReader::FillBuffer() {
  num_read_ = sf_read_float(file_, buffer_.data.data(), chunk_size_);
  if (num_read_ < chunk_size_)
    is_eof_ = true;

  buffer_.used_size = num_read_;

  if (feed_thru_)
    sf_write_float(outfile_, buffer_.data.data(), num_read_);
}

MPXBuffer<>& MPXReader::ReadChunk(int channel) {
  assert(channel >= 0 && channel < num_channels_);

  if (is_eof_)
    return buffer_;

  if (num_channels_ == 1) {
    return buffer_;
  } else {
    buffer_singlechan_.used_size = buffer_.used_size / num_channels_;
    for (size_t i = 0; i < buffer_singlechan_.used_size; i++)
      buffer_singlechan_.data[i] = buffer_.data[i * num_channels_ + channel];

    return buffer_singlechan_;
  }
}

float MPXReader::samplerate() const {
  return sfinfo_.samplerate;
}

int MPXReader::num_channels() const {
  return num_channels_;
}

/*
 * An AsciiBitReader reads an unsynchronized serial bitstream as '0' and '1'
 * characters via stdin.
 *
 */
AsciiBitReader::AsciiBitReader(const Options& options) :
    is_eof_(false), feed_thru_(options.feed_thru) {
}

AsciiBitReader::~AsciiBitReader() {
}

bool AsciiBitReader::ReadNextBit() {
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
