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
#include "src/rdsstring.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <numeric>
#include <string>
#include <utility>

#include "ext/iconvpp/iconv.hpp"

#include "src/common.h"

namespace redsea {

namespace {

// EN 50067:1998, Annex E (pp. 73-76)
// plus UCS-2 control codes
std::string getRDSCharString(uint8_t code) {
  static const std::array<std::string, 256> codetable_G0({
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

  return codetable_G0[code];
}

// Length of a UTF-8 character starting at byte_start.
// 0 on error.
size_t charlen(const std::string& str, size_t byte_start) {
  size_t nbyte{byte_start};

  if (nbyte >= str.length())
    return 0;

  while ((str[nbyte] & 0b1100'0000) == 0b1000'0000) {
      nbyte++;

      if (nbyte >= str.length())
        return 0;
  }

  return nbyte - byte_start + 1;
}

std::string decodeUCS2(const std::string& src) {
  static iconvpp::converter converter("UTF-8", "UCS-2");

  std::string dst;
  converter.convert(src, dst);
  return dst;
}

constexpr uint8_t kStringTerminator { 0x0D };
constexpr uint8_t kBlankSpace       { 0x20 };

}  // namespace

RDSString::RDSString(size_t len) : data_(len) {
}

void RDSString::set(size_t pos, uint8_t byte) {
  if (pos >= data_.size())
    return;

  data_.at(pos) = byte;

  if (pos == 0 || (pos == prev_pos_ + 1 && sequential_length_ == pos))
    sequential_length_ = pos + 1;

  if (isComplete()) {
    last_complete_string_ = str();
    last_complete_data_ = getData();
  }

  prev_pos_ = pos;
}

void RDSString::set(size_t pos, uint8_t byte1, uint8_t byte2) {
  set(pos, byte1);
  set(pos + 1, byte2);
}

// Length, in bytes, is exactly the position of the first non-received character
size_t RDSString::getReceivedLength() const {
  return sequential_length_;
}

// Length, in bytes, up to the first string terminator, or the full allocated length
size_t RDSString::getExpectedLength() const {
  const auto terminated_length = std::distance(data_.cbegin(),
      std::find_if(data_.cbegin(), data_.cend(), [](uint8_t b) {
        return b == kStringTerminator;
      })) + 1;

  return std::min(static_cast<size_t>(terminated_length), data_.size());
}

bool RDSString::hasPreviouslyReceivedTerminators() const {
  return std::find_if(data_.cbegin(), data_.cend(), [](uint8_t b) {
        return b == kStringTerminator;
    }) != data_.cend();
}

void RDSString::resize(size_t n) {
  data_.resize(n, kBlankSpace);
}

void RDSString::setEncoding(Encoding encoding) {
  encoding_ = encoding;
}

void RDSString::setDirection(Direction direction) {
  direction_ = direction;
}

std::string RDSString::str() const {
  const auto bytes = getData();

  switch (encoding_) {
    case Encoding::Basic:
      return std::accumulate(bytes.cbegin(), bytes.cend(), std::string(""),
          [](const std::string& s, uint8_t b) {
          return s + getRDSCharString(b); });

    case Encoding::UCS2:
      return decodeUCS2(std::string(reinterpret_cast<const char*>(bytes.data()), bytes.size()));

    case Encoding::UTF8:
      return std::string(reinterpret_cast<const char*>(bytes.data()), bytes.size());
  }

  return "";
}

std::vector<uint8_t> RDSString::getData() const {
  const size_t len{getExpectedLength()};
  std::vector<uint8_t> result(len);
  for (size_t i = 0; i < len; i++)
    result[i] = sequential_length_ > i && data_[i] != kStringTerminator && data_[i] != 0x00 ? data_[i] : kBlankSpace;

  return result;
}

std::string RDSString::getLastCompleteString() const {
  return last_complete_string_;
}

// Overload used in RT+ / eRT+
// Find substring in UTF-8
std::string RDSString::getLastCompleteString(size_t start, size_t len) const {
  // Find byte offset of start position
  size_t byte_start{};
  for (size_t i_char{}; i_char < start; i_char++) {
    const size_t clen{charlen(last_complete_string_, byte_start)};
    if (clen == 0)
      return "";

    byte_start += clen;
  }

  // Find byte offset of end position
  size_t byte_end{byte_start};
  for (size_t i_char = 0; i_char < len; i_char++) {
    const size_t clen{charlen(last_complete_string_, byte_end)};
    if (clen == 0)
      return "";

    byte_end += clen;
  }

  return byte_end < last_complete_string_.length() ?
         last_complete_string_.substr(byte_start, byte_end - byte_start) : "";
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
