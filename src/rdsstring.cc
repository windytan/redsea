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
#include <numeric>
#include <string>
#include <utility>

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

constexpr uint8_t kStringTerminator { 0x0D };

}  // namespace

RDSString::RDSString(size_t len) : chars_(len),
    last_complete_string_(str()) {
}

void RDSString::set(size_t pos, RDSChar chr) {
  if (pos >= chars_.size())
    return;

  chars_.at(pos) = chr;

  if (pos != prev_pos_ + 1)
    for (RDSChar& c : chars_)
      c.is_sequential = false;

  chars_.at(pos).is_sequential = true;

  if (isComplete()) {
    last_complete_string_ = str();
    last_complete_chars_ = getChars();
  }

  prev_pos_ = pos;
}

void RDSString::set(size_t pos, RDSChar chr1, RDSChar chr2) {
  set(pos, chr1);
  set(pos + 1, chr2);
}

// Length is exactly the position of the first non-received character
size_t RDSString::getReceivedLength() const {
  return size_t(std::distance(chars_.cbegin(),
      std::find_if_not(chars_.cbegin(), chars_.cend(), [](const RDSChar& chr) {
        return chr.is_sequential;
      })));
}

// Length up to the first string terminator, or the full allocated length
size_t RDSString::getExpectedLength() const {
  auto terminated_length = std::distance(chars_.cbegin(),
      std::find_if(chars_.cbegin(), chars_.cend(), [](const RDSChar& chr) {
        return chr.code == kStringTerminator;
      })) + 1;

  return std::min(static_cast<size_t>(terminated_length), chars_.size());
}

bool RDSString::hasPreviouslyReceivedTerminators() const {
  return std::find_if(chars_.cbegin(), chars_.cend(), [](const RDSChar& chr) {
        return chr.code == kStringTerminator;
    }) != chars_.cend();
}

void RDSString::resize(size_t n) {
  chars_.resize(n, RDSChar(0x20));
}

std::string RDSString::str() const {
  auto characters = getChars();
  return std::accumulate(characters.cbegin(), characters.cend(), std::string(""),
      [](const std::string& s, const RDSChar& chr) {
      return s + getRDSCharString(chr.code); });
}

std::vector<RDSChar> RDSString::getChars() const {
  size_t len = getExpectedLength();
  std::vector<RDSChar> result(len);
  for (size_t i = 0; i < len; i++)
    result[i] = chars_[i].is_sequential && chars_[i].code != kStringTerminator ? chars_[i] : RDSChar(0x20);

  return result;
}

std::string RDSString::getLastCompleteString() const {
  return last_complete_string_;
}

// Used in RT+
std::string RDSString::getLastCompleteString(size_t start, size_t len) const {
  std::string result;
  for (size_t i = start; i < start + len; i++)
    result += (i < last_complete_chars_.size() ?
        getRDSCharString(last_complete_chars_[i].code) : " ");

  return result;
}

bool RDSString::hasChars(size_t start, size_t len) const {
  return start + len <= last_complete_chars_.size();
}

bool RDSString::isComplete() const {
  return getReceivedLength() >= getExpectedLength();
}

void RDSString::clear() {
  for (RDSChar& c : chars_)
    c.is_sequential = false;
  last_complete_string_ = str();
  last_complete_chars_.clear();
}

}  // namespace redsea
