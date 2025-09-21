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
#include "src/util.hh"

#include <cassert>
#include <cstddef>
#include <cstdint>

namespace redsea {

std::uint16_t getBits(std::uint16_t word, std::size_t starting_at, std::size_t length) {
  assert(starting_at + length <= 16U);
  return static_cast<std::uint16_t>(word >> starting_at) &
         (static_cast<std::uint16_t>(1U << length) - 1U);
}

std::uint32_t getBits(std::uint16_t word1, std::uint16_t word2, std::size_t starting_at,
                      std::size_t length) {
  assert(starting_at + length <= 32U);
  const auto concat =
      static_cast<std::uint32_t>(word2 + (static_cast<std::uint32_t>(word1) << 16U));
  return static_cast<std::uint32_t>(concat >> starting_at) &
         (static_cast<std::uint32_t>(1U << length) - 1U);
}

bool getBool(std::uint16_t word, std::size_t bit_pos) {
  assert(bit_pos < 16);
  return static_cast<bool>(getBits(word, bit_pos, 1));
}

std::uint8_t getUint8(std::uint16_t word, std::size_t bit_pos) {
  assert(bit_pos < 16);
  return static_cast<std::uint8_t>(getBits(word, bit_pos, 8));
}

}  // namespace redsea
