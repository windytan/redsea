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
#include "src/io/input.hh"

// For fileno
#include <stdio.h>

#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <tuple>

#include <sndfile.h>

#include "src/constants.hh"
#include "src/groups.hh"
#include "src/options.hh"

namespace redsea {

/**
 * An MPXReader deals with reading an FM multiplex signal from an audio file or
 * raw PCM via stdin, separating it into channels and converting to chunks of
 * floating-point samples.
 * @throws BeyondEofError if there is nothing to read
 * @throws std::runtime_error for sndfile errors
 */
void MPXReader::init(const Options& options) {
  num_channels_ = options.num_channels;
  feed_thru_    = options.feed_thru;
  filename_     = options.sndfilename;

  switch (options.input_type) {
    case InputType::MPX_stdin: {
      sfinfo_.channels   = 1;
      sfinfo_.format     = SF_FORMAT_RAW | SF_FORMAT_PCM_16;
      sfinfo_.samplerate = static_cast<int>(std::lround(options.samplerate));
      sfinfo_.frames     = 0;
      file_              = sf_open_fd(::fileno(stdin), SFM_READ, &sfinfo_, SF_TRUE);
      if (feed_thru_)
        outfile_ = sf_open_fd(::fileno(stdout), SFM_WRITE, &sfinfo_, SF_TRUE);

      break;
    }
    case InputType::MPX_sndfile: {
      file_         = sf_open(options.sndfilename.c_str(), SFM_READ, &sfinfo_);
      num_channels_ = static_cast<std::uint32_t>(sfinfo_.channels);

      if (options.is_rate_defined)
        std::cerr << "warning: ignoring sample rate parameter" << std::endl;
      if (options.is_num_channels_defined)
        std::cerr << "warning: ignoring number of channels parameter" << std::endl;
      break;
    }
    default: return;
  }

  if (!file_) {
    if (sf_error(file_) == 26 || options.input_type == InputType::MPX_stdin)
      throw BeyondEofError();

    throw std::runtime_error(sf_error_number(sf_error(file_)));
  } else if (sfinfo_.samplerate < static_cast<int>(kMinimumSampleRate_Hz)) {
    throw std::runtime_error(
        "sample rate is " + std::to_string(sfinfo_.samplerate) + " Hz, must be " +
        std::to_string(static_cast<int>(kMinimumSampleRate_Hz)) + " Hz or higher");
  } else if (options.streams && sfinfo_.samplerate < 171000) {
    throw std::runtime_error("RDS2 data streams require a sample rate of 171 kHz or higher");
  } else if (sfinfo_.samplerate > static_cast<int>(kMaximumSampleRate_Hz)) {
    throw std::runtime_error("sample rate is " + std::to_string(sfinfo_.samplerate) +
                             " Hz, must be " + "no higher than " +
                             std::to_string(static_cast<int>(kMaximumSampleRate_Hz)) + " Hz");
  } else {
    chunk_size_ = static_cast<sf_count_t>((kInputChunkSize / num_channels_) * num_channels_);

    is_eof_ = (num_channels_ >= buffer_.data.size());
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

// @brief Read a chunk of samples on the specified PCM channel.
// @note Remember to first call fillBuffer().
// @throws logic_error if channel is out-of-bounds
MPXBuffer& MPXReader::readChunk(std::uint32_t channel) {
  if (channel >= num_channels_) {
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
  return static_cast<float>(sfinfo_.samplerate);
}

std::uint32_t MPXReader::getNumChannels() const {
  return num_channels_;
}

/*
 * An AsciiBitReader reads an unsynchronized serial bitstream as '0' and '1'
 * characters via stdin.
 *
 */
AsciiBitReader::AsciiBitReader(const Options& options) : feed_thru_(options.feed_thru) {}

bool AsciiBitReader::readBit() {
  int chr = 0;
  while (chr != '0' && chr != '1' && chr != EOF) {
    chr = std::getchar();
    if (feed_thru_)
      std::ignore = std::putchar(chr);
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

    // RDS Spy format marks the RDS2 data stream number like this
    int n_stream = 0;
    if (line.length() >= 20 && (line.substr(0, 4) == "#S1 " || line.substr(0, 4) == "#S2 " ||
                                line.substr(0, 4) == "#S3 ")) {
      n_stream = std::stoi(line.substr(2, 1));
      line     = line.substr(4);
    }
    group.setDataStream(n_stream);

    for (const eBlockNumber block_num : {BLOCK1, BLOCK2, BLOCK3, BLOCK4}) {
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
            block.data     = static_cast<std::uint16_t>((block.data << 4U) + nval);
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

  if (options.timestamp) {
    group.setRxTime(std::chrono::system_clock::now());
  }

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
      // Lines starting with 'P' contain the PI code
      // e.g. PA540
      Block block1;
      try {
        line               = line.substr(1);
        block1.data        = static_cast<std::uint16_t>(std::stol(line, nullptr, 16));
        block1.is_received = true;
      } catch (const std::exception&) {
        continue;
      }
      group.setBlock(BLOCK1, block1);
    } else if (line.substr(0, 1) == "R" && line.length() >= 15) {
      // 'R' lines contain the rest of the blocks + errors
      // e.g. R0549000000000F (R + 3*4 nybbles + 2 nybbles = 14 nybbles = 56 bits)
      // The 'errors' mark whether a block had errors: 00110000 for Block1, 00001100 for Block 2,
      // ...
      // https://github.com/PE5PVB/TEF6686_ESP32/blob/dfa56f9dbe5dbf8bf32b4f1631abe64d552a25dd/src/rds.cpp#L406
      try {
        std::array<Block, 3> blocks{};
        blocks[0].data = std::stol(line.substr(1, 4), nullptr, 16);
        blocks[1].data = std::stol(line.substr(5, 4), nullptr, 16);
        blocks[2].data = std::stol(line.substr(9, 4), nullptr, 16);

        const auto rds_err = static_cast<std::uint32_t>(std::stol(line.substr(13, 2), nullptr, 16));
        blocks[0].is_received = (static_cast<std::uint32_t>(rds_err >> 4U) & 0xFFU) == 0;
        blocks[1].is_received = (static_cast<std::uint32_t>(rds_err >> 2U) & 0xFFU) == 0;
        blocks[2].is_received = (static_cast<std::uint32_t>(rds_err >> 0U) & 0xFFU) == 0;

        group.setBlock(BLOCK2, blocks[0]);
        group.setBlock(BLOCK3, blocks[1]);
        group.setBlock(BLOCK4, blocks[2]);
      } catch (const std::exception&) {
        break;
      }
      break;
    }
  }

  return group;
}

}  // namespace redsea
