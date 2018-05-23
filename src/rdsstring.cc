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

#include <string>
#include <utility>

#include "src/common.h"
#include "src/tables.h"

namespace redsea {

RDSChar::RDSChar() : code_(0), codetable_(CodeTable::G0) {
}

RDSChar::RDSChar(uint8_t _code) : code_(_code), codetable_(CodeTable::G0) {
}

void RDSChar::set_codetable(CodeTable codetable) {
  codetable_ = codetable;
}

uint8_t RDSChar::code() const {
  return code_;
}

std::string RDSChar::str() const {
  return RDSCharString(code_, codetable_);
}

RDSString::RDSString(int len) : chars_(len), is_char_sequential_(len),
  prev_pos_(-1), last_complete_string_(str()) {
}

void RDSString::set(int pos, RDSChar chr) {
  if (pos < 0 || pos >= static_cast<int>(chars_.size()))
    return;

  chr.set_codetable(repertoire_at(pos));

  chars_.at(pos) = chr;

  if (pos != prev_pos_ + 1) {
    for (size_t i=0; i < is_char_sequential_.size(); i++)
      is_char_sequential_[i] = false;
  }

  is_char_sequential_.at(pos) = true;

  if (complete()) {
    last_complete_string_ = str();
    last_complete_chars_ = chars();
  }

  prev_pos_ = pos;
}

void RDSString::set(int pos, RDSChar chr1, RDSChar chr2) {
  if (chr1.code() == 0x0F && chr2.code() == 0x0F) {
    set_repertoire(pos, CodeTable::G0);
  } else if (chr1.code() == 0x0E && chr2.code() == 0x0E) {
    set_repertoire(pos, CodeTable::G1);
  } else if (chr1.code() == 0x1B && chr2.code() == 0x6E) {
    set_repertoire(pos, CodeTable::G2);
  } else {
    set(pos, chr1);
    set(pos + 1, chr2);
  }
}

void RDSString::set_repertoire(int pos, CodeTable codetable) {
  if (pos >= 0)
    repertoire_[pos] = codetable;
}

CodeTable RDSString::repertoire_at(int pos) const {
  CodeTable codetable = CodeTable::G0;
  for (std::pair<int, CodeTable> r : repertoire_)
    if (pos >= r.first)
      codetable = r.second;
  return codetable;
}

size_t RDSString::length_received() const {
  size_t result = 0;
  for (size_t i=0; i < is_char_sequential_.size(); i++) {
    if (!is_char_sequential_[i])
      break;
    result = i+1;
  }

  return result;
}

size_t RDSString::length_expected() const {
  size_t result = chars_.size();

  for (size_t i=0; i < chars_.size(); i++) {
    if (chars_[i].code() == 0x0D) {
      result = i;
      break;
    }
  }

  return result;
}

void RDSString::resize(int n) {
  chars_.resize(n, RDSChar(0x20));
}

std::string RDSString::str() const {
  std::string result;
  std::vector<RDSChar> _chars = chars();
  for (size_t pos=0; pos < _chars.size(); pos++) {
    result += _chars[pos].str();
  }

  return result;
}

std::vector<RDSChar> RDSString::chars() const {
  std::vector<RDSChar> result;
  size_t len = length_expected();
  for (size_t i=0; i < len; i++) {
    result.push_back(is_char_sequential_[i] ? chars_[i] : RDSChar(0x20));
  }

  return result;
}

std::string RDSString::last_complete_string() const {
  return last_complete_string_;
}

std::string RDSString::last_complete_string(int start, int len) const {
  std::string result;
  for (int i=start; i < start+len; i++) {
    result += (i < static_cast<int>(last_complete_chars_.size()) ?
        last_complete_chars_[i].str() : " ");
  }

  return result;
}

bool RDSString::has_chars(int start, int len) const {
  return start+len <= static_cast<int>(last_complete_chars_.size());
}

bool RDSString::complete() const {
  return length_received() >= length_expected();
}

void RDSString::clear() {
  for (size_t i=0; i < chars_.size(); i++) {
    is_char_sequential_[i] = false;
  }
  last_complete_string_ = str();
  last_complete_chars_.clear();
  repertoire_.clear();
}

}  // namespace redsea
