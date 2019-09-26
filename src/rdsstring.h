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
#ifndef RDSSTRING_H_
#define RDSSTRING_H_

#include <map>
#include <string>
#include <vector>

#include "src/common.h"

namespace redsea {

class RDSChar {
 public:
  RDSChar() = default;
  explicit RDSChar(uint16_t _code) : code_(_code) {};

  uint16_t getCode() const    { return code_; };
  bool isSequential() const   { return is_sequential_; };
  void setSequential(bool is) { is_sequential_ = is; }

 private:
  uint16_t code_      { 0 };
  bool is_sequential_ { false };
};

class RDSString {
 public:
  explicit RDSString(size_t len = 8);
  void set(size_t pos, RDSChar chr);
  void set(size_t pos, RDSChar chr1, RDSChar chr2);
  size_t getReceivedLength() const;
  size_t getExpectedLength() const;
  std::vector<RDSChar> getChars() const;
  std::string str() const;
  std::string getLastCompleteString() const;
  std::string getLastCompleteString(size_t start, size_t len) const;
  bool hasChars(size_t start, size_t len) const;
  bool isComplete() const;
  void clear();
  void resize(size_t n);

 private:
  std::vector<RDSChar> chars_;
  std::vector<RDSChar> last_complete_chars_;
  size_t prev_pos_ { 0 };
  std::string last_complete_string_;
};

}  // namespace redsea
#endif  // RDSSTRING_H_
