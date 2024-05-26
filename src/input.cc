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
 * @throws if attempting to read beyond EOF
 */
void MPXReader::init(const Options& options) {
  num_channels_ = options.num_channels;
  feed_thru_ = options.feed_thru;
  filename_ = options.sndfilename;

  if (options.input_type != InputType::MPX_stdin &&
      options.input_type != InputType::MPX_sndfile)
    return;

  if (options.input_type == InputType::MPX_stdin) {
    sfinfo_.channels = 1;
    sfinfo_.format = SF_FORMAT_RAW | SF_FORMAT_PCM_16;
    sfinfo_.samplerate = static_cast<int>(options.samplerate + .5f);
    sfinfo_.frames = 0;
    file_ = sf_open_fd(fileno(stdin), SFM_READ, &sfinfo_, SF_TRUE);
    if (feed_thru_)
      outfile_ = sf_open_fd(fileno(stdout), SFM_WRITE, &sfinfo_, SF_TRUE);
  } else if (options.input_type == InputType::MPX_sndfile) {
    file_ = sf_open(options.sndfilename.c_str(), SFM_READ, &sfinfo_);
    num_channels_ = sfinfo_.channels;
  }

  if (!file_) {
    if (sf_error(file_) == 26 || options.input_type == InputType::MPX_stdin)
      throw BeyondEofError();

    std::cerr << "error: failed to open file: " <<
              sf_error_number(sf_error(file_)) << '\n';
    is_error_ = true;
  } else if (sfinfo_.samplerate < kMinimumSampleRate_Hz) {
    std::cerr << "error: sample rate is " << sfinfo_.samplerate << ", must be " << kMinimumSampleRate_Hz
              << " Hz or higher\n";
    is_error_ = true;
  } else {
    chunk_size_ = (kInputChunkSize / num_channels_) * num_channels_;

    is_eof_ = (num_channels_ >= static_cast<int>(buffer_.data.size()));
  }
}

MPXReader::~MPXReader() {
  sf_close(file_);

  if (feed_thru_)
    sf_close(outfile_);
}

bool MPXReader::eof() const {
  return is_eof_;
}

/*
 * Fill the internal buffer with fresh samples. This should be called before
 * the first channel is processed via ReadChunk().
 *
 */
void MPXReader::fillBuffer() {
  num_read_ = sf_read_float(file_, buffer_.data.data(), chunk_size_);

  buffer_.time_received = std::chrono::system_clock::now();

  if (num_read_ < chunk_size_)
    is_eof_ = true;

  buffer_.used_size = static_cast<size_t>(num_read_);

  if (feed_thru_)
    sf_write_float(outfile_, buffer_.data.data(), num_read_);
}

// @throws logic_error if channel is out-of-bounds
MPXBuffer<>& MPXReader::readChunk(int channel) {
  if (channel < 0 || channel >= num_channels_) {
    throw std::logic_error("Tried to access channel " + std::to_string(channel) + " of " +
                           std::to_string(num_channels_) + "-channel signal");
  }

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

float MPXReader::getSamplerate() const {
  return sfinfo_.samplerate;
}

int MPXReader::getNumChannels() const {
  return num_channels_;
}

bool MPXReader::hasError() const {
  return is_error_;
}

/*
 * An AsciiBitReader reads an unsynchronized serial bitstream as '0' and '1'
 * characters via stdin.
 *
 */
AsciiBitReader::AsciiBitReader(const Options& options) :
    feed_thru_(options.feed_thru) {
}

bool AsciiBitReader::readBit() {
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
Group readHexGroup(const Options& options) {
  Group group;
  group.disableOffsets();

  bool group_complete = false;

  while (!(group_complete || std::cin.eof())) {
    std::string line;
    std::getline(std::cin, line);
    if (options.feed_thru)
      std::cout << line << '\n';

    if (line.length() < 16)
      continue;

    for (eBlockNumber block_num : {BLOCK1, BLOCK2, BLOCK3, BLOCK4}) {
      Block block;
      bool block_still_valid = true;

      int which_nibble = 0;
      while (which_nibble < 4) {
        if (line.length() < 1) {
          group_complete = true;
          break;
        }

        const std::string single = line.substr(0, 1);

        if (single != " ") {
          try {
            const int nval = std::stoi(std::string(single), nullptr, 16);
            block.data = static_cast<uint16_t>((block.data << 4) + nval);
          } catch (std::exception&) {
            block_still_valid = false;
          }
          which_nibble++;
        }
        line = line.substr(1);
      }

      if (block_still_valid) {
        block.is_received = true;
        group.setBlock(block_num, block);
      }

      if (block_num == BLOCK4)
        group_complete = true;
    }
  }

  if (options.timestamp)
    group.setTime(std::chrono::system_clock::now());

  return group;
}

// Read one group in the TEF6686 output format
Group readTEFGroup(const Options& options) {
  Group group;
  group.disableOffsets();

  while (!std::cin.eof()) {
    std::string line;
    std::getline(std::cin, line);
    if (options.feed_thru)
      std::cout << line << '\n';

    if (line.substr(0, 1) == "P") {
      Block block1;
      try {
        line = line.substr(1);
        const int64_t data = std::stol(line, nullptr, 16);
        block1.data = data & 0xFFFF;
        block1.is_received = true;
      } catch (std::exception& e) {
      }
      group.setBlock(BLOCK1, block1);
    } else if (line.substr(0, 1) == "R") {
      int64_t data{0};
      uint16_t rdsErr{0xFF};
      try {
        line = line.substr(1);
        data = std::stol(line, nullptr, 16);
        rdsErr = (data & 0xFF);
      } catch (std::exception& e) {
      }
      for (const auto blockNum : { BLOCK2, BLOCK3, BLOCK4 }) {
        Block block;
        block.data = (data >> 8) >> (16 * (3 - blockNum));
        block.is_received = (rdsErr >> (2 * (3 - blockNum))) == 0;
        group.setBlock(blockNum, block);
      }
      break;
    }
  }

  return group;
}

}  // namespace redsea
