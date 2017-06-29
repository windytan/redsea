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
#include "src/ascii_in.h"

#include <iostream>
#include <stdexcept>
#include <string>

#include "src/groups.h"
#include "src/util.h"

namespace redsea {

AsciiBits::AsciiBits(const Options& options) :
    is_eof_(false), feed_thru_(options.feed_thru) {
}

AsciiBits::~AsciiBits() {
}

int AsciiBits::NextBit() {
  int result = 0;
  while (result != '0' && result != '1' && result != EOF) {
    result = getchar();
    if (feed_thru_)
      putchar(result);
  }

  if (result == EOF) {
    is_eof_ = true;
    return 0;
  }

  return (result == '1');
}

bool AsciiBits::eof() const {
  return is_eof_;
}

Group NextGroupRSpy(const Options& options) {
  Group group;
  group.disable_offsets();

  bool finished = false;

  while (!(finished || std::cin.eof())) {
    std::string line;
    std::getline(std::cin, line);
    if (options.feed_thru)
      std::cout << line << std::endl;

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
