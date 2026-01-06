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
#include "src/text/rdsstring.hh"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <numeric>
#include <string>
#include <string_view>
#include <vector>

#include "ext/iconvpp/iconv.hpp"

namespace redsea {

namespace {

// EN 50067:1998, Annex E (pp. 73-76)
// plus UCS-2 control codes
std::string_view getRDSCharString(std::uint8_t code) {
  // clang-format off
  constexpr std::array<std::string_view, 256> codetable_G0({
    " ", " ", " ", " ", " ", " ", " ", " ", " ", " ", "\n"," ", " ", "\r"," ", " ",
    " ", " ", " ", " ", " ", " ", " ", " ", " ", " ", " ", " ", " ", " ", " ", "\u00AD",
    " ", "!", "\"","#", "¤", "%", "&", "'", "(", ")", "*", "+", ",", "-", ".", "/",
    "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", ":", ";", "<", "=", ">", "?",
    "@", "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N", "O",
    "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z", "[", "\\","]", "―", "_",
    "‖", "a", "b", "c", "d", "e", "f", "g", "h", "i", "j", "k", "l", "m", "n", "o",
    "p", "q", "r", "s", "t", "u", "v", "w", "x", "y", "z", "{", "|", "}", "¯", " ",
    "á", "à", "é", "è", "í", "ì", "ó", "ò", "ú", "ù", "Ñ", "Ç", "Ş", "β", "¡", "Ĳ",
    "â", "ä", "ê", "ë", "î", "ï", "ô", "ö", "û", "ü", "ñ", "ç", "ş", "ǧ", "ı", "ĳ",
    "ª", "α", "©", "‰", "Ǧ", "ě", "ň", "ő", "π", "€", "£", "$", "←", "↑", "→", "↓",
    "º", "¹", "²", "³", "±", "İ", "ń", "ű", "µ", "¿", "÷", "°", "¼", "½", "¾", "§",
    "Á", "À", "É", "È", "Í", "Ì", "Ó", "Ò", "Ú", "Ù", "Ř", "Č", "Š", "Ž", "Ð", "Ŀ",
    "Â", "Ä", "Ê", "Ë", "Î", "Ï", "Ô", "Ö", "Û", "Ü", "ř", "č", "š", "ž", "đ", "ŀ",
    "Ã", "Å", "Æ", "Œ", "ŷ", "Ý", "Õ", "Ø", "Þ", "Ŋ", "Ŕ", "Ć", "Ś", "Ź", "Ŧ", "ð",
    "ã", "å", "æ", "œ", "ŵ", "ý", "õ", "ø", "þ", "ŋ", "ŕ", "ć", "ś", "ź", "ŧ", " "});
  // clang-format on

  return codetable_G0[code];
}

// \throws Conversion errors from iconvpp
std::string decodeUCS2(const std::string& src) {
  static iconvpp::converter converter("UTF-8", "UCS-2");

  std::string dst;
  converter.convert(src, dst);
  return dst;
}

// Length of a UTF-8 character starting at byte_start.
// Returns 1 on error (treat as single byte) to allow continuing.
// Returns 0 if already at or past end of string.
inline std::size_t charlen(const std::string& str, std::size_t byte_start) {
  if (byte_start >= str.length())
    return 0;

  const std::uint8_t b = static_cast<std::uint8_t>(str[byte_start]);

  // ASCII
  if ((b & 0x80U) == 0x00U)
    return 1;

  std::size_t len = 0;
  if ((b & 0xE0U) == 0xC0U)
    len = 2;
  else if ((b & 0xF0U) == 0xE0U)
    len = 3;
  else if ((b & 0xF8U) == 0xF0U)
    len = 4;
  else
    return 1; // Invalid start byte

  if (byte_start + len > str.length())
    return 1; // Incomplete sequence

  // Verify continuation bytes
  for (std::size_t i = 1; i < len; ++i) {
    if ((static_cast<std::uint8_t>(str[byte_start + i]) & 0xC0U) != 0x80U)
      return 1; // Invalid sequence
  }

  return len;
}

constexpr std::uint8_t kStringTerminator{0x0D};
constexpr std::uint8_t kBlankSpace{0x20};

}  // namespace

// Sanitize a UTF-8 string by replacing invalid/incomplete sequences with '?'.
// This ensures the output is always valid UTF-8 for JSON serialization.
std::string sanitizeUtf8(const std::string& src) {
  std::string result;
  result.reserve(src.size());

  std::size_t i = 0;
  while (i < src.size()) {
    const std::uint8_t b = static_cast<std::uint8_t>(src[i]);

    // Determine expected sequence length from first byte
    std::size_t seq_len = 0;
    if ((b & 0x80U) == 0x00U) {
      // 1-byte ASCII: 0xxxxxxx
      seq_len = 1;
    } else if ((b & 0xE0U) == 0xC0U) {
      // 2-byte sequence: 110xxxxx
      seq_len = 2;
    } else if ((b & 0xF0U) == 0xE0U) {
      // 3-byte sequence: 1110xxxx
      seq_len = 3;
    } else if ((b & 0xF8U) == 0xF0U) {
      // 4-byte sequence: 11110xxx
      seq_len = 4;
    } else {
      // Invalid start byte (continuation byte or 0xF8+)
      result += '?';
      i++;
      continue;
    }

    // Check if we have enough bytes remaining
    if (i + seq_len > src.size()) {
      // Incomplete sequence at end of string
      result += '?';
      break;
    }

    // Validate continuation bytes (must be 10xxxxxx)
    bool valid = true;
    for (std::size_t j = 1; j < seq_len; j++) {
      const std::uint8_t cb = static_cast<std::uint8_t>(src[i + j]);
      if ((cb & 0xC0U) != 0x80U) {
        valid = false;
        break;
      }
    }

    if (valid) {
      // Valid sequence - copy it
      result.append(src, i, seq_len);
      i += seq_len;
    } else {
      // Invalid sequence - replace first byte and continue
      result += '?';
      i++;
    }
  }

  return result;
}

RDSString::RDSString(std::size_t len) : data_(len) {}

void RDSString::set(std::size_t pos, std::uint8_t byte) {
  if (pos >= data_.size())
    return;

  data_.at(pos) = byte;

  if (pos == 0 || (pos == prev_pos_ + 1 && sequential_length_ == pos))
    sequential_length_ = pos + 1;

  if (isComplete()) {
    try {
      last_complete_string_ = str();
    } catch (...) {
      clear();
      return;
    }

    last_complete_data_ = getData();
  }

  prev_pos_ = pos;
}

void RDSString::set(std::size_t pos, std::uint8_t byte1, std::uint8_t byte2) {
  set(pos, byte1);
  set(pos + 1, byte2);
}

// Length, in bytes, is exactly the position of the first non-received character
std::size_t RDSString::getReceivedLength() const {
  return sequential_length_;
}

// Length, in bytes, up to the first string terminator, or the full allocated length
std::size_t RDSString::getExpectedLength() const {
  const auto terminated_length =
      std::distance(data_.cbegin(),
                    std::find_if(data_.cbegin(), data_.cend(),
                                 [](std::uint8_t b) { return b == kStringTerminator; })) +
      1;

  return std::min(static_cast<std::size_t>(terminated_length), data_.size());
}

bool RDSString::hasPreviouslyReceivedTerminators() const {
  return std::find_if(data_.cbegin(), data_.cend(),
                      [](std::uint8_t b) { return b == kStringTerminator; }) != data_.cend();
}

void RDSString::resize(std::size_t n) {
  data_.resize(n, kBlankSpace);
}

void RDSString::setEncoding(Encoding encoding) {
  encoding_ = encoding;
}

void RDSString::setDirection(Direction direction) {
  direction_ = direction;
}

// \throws Conversion errors from iconvpp when text is UCS2
std::string RDSString::str() const {
  const auto bytes = getData();

  switch (encoding_) {
    case Encoding::Basic:
      return std::accumulate(bytes.cbegin(), bytes.cend(), std::string(""),
                             [](const std::string& s, std::uint8_t b) {
                               return s + std::string{getRDSCharString(b)};
                             });

    case Encoding::UCS2:
      return decodeUCS2(std::string(reinterpret_cast<const char*>(bytes.data()), bytes.size()));

    case Encoding::UTF8:
      return std::string(reinterpret_cast<const char*>(bytes.data()), bytes.size());
  }

  return "";
}

std::vector<std::uint8_t> RDSString::getData() const {
  const std::size_t len{getExpectedLength()};
  std::vector<std::uint8_t> result(len);
  for (std::size_t i = 0; i < len; i++) {
    result[i] = sequential_length_ > i && data_[i] != kStringTerminator && data_[i] != 0x00
                    ? data_[i]
                    : kBlankSpace;
  }

  return result;
}

const std::string& RDSString::getLastCompleteString() const {
  return last_complete_string_;
}

// Overload used in RT+ / eRT+
// Find substring in UTF-8
std::string RDSString::getLastCompleteString(std::size_t start, std::size_t len) const {
  // Find byte offset of start position
  std::size_t byte_start{};
  for (std::size_t i_char{}; i_char < start; i_char++) {
    const std::size_t clen{charlen(last_complete_string_, byte_start)};
    if (clen == 0)
      return "";

    byte_start += clen;
  }

  // Find byte offset of end position
  std::size_t byte_end{byte_start};
  for (std::size_t i_char = 0; i_char < len; i_char++) {
    const std::size_t clen{charlen(last_complete_string_, byte_end)};
    if (clen == 0)
      return "";

    byte_end += clen;
  }

  return byte_end <= last_complete_string_.length()
             ? last_complete_string_.substr(byte_start, byte_end - byte_start)
             : "";
}

bool RDSString::isComplete() const {
  return getReceivedLength() >= getExpectedLength();
}

void RDSString::clear() {
  sequential_length_ = 0;
  last_complete_string_.clear();
  last_complete_data_.clear();
}

}  // namespace redsea
