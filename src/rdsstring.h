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

#include <cstdint>
#include <string>
#include <vector>

namespace redsea {

// An RDSString can be RadioText, Program Service name, or Enhanced RadioText.
class RDSString {
 public:
  enum class Encoding { Basic, UCS2, UTF8 };
  enum class Direction { LTR, RTL };

  explicit RDSString(size_t len = 8);
  void set(size_t pos, uint8_t byte);
  void set(size_t pos, uint8_t byte1, uint8_t byte2);
  size_t getReceivedLength() const;
  size_t getExpectedLength() const;
  std::vector<uint8_t> getData() const;
  std::string str() const;
  const std::string& getLastCompleteString() const;
  std::string getLastCompleteString(size_t start, size_t len) const;
  bool isComplete() const;
  bool hasPreviouslyReceivedTerminators() const;
  void clear();
  void resize(size_t n);
  void setEncoding(Encoding encoding);
  void setDirection(Direction direction);

 private:
  Encoding encoding_{Encoding::Basic};
  Direction direction_{Direction::LTR};
  // Raw bytes.
  std::vector<uint8_t> data_;
  // Raw bytes.
  std::vector<uint8_t> last_complete_data_;
  size_t prev_pos_{};
  size_t sequential_length_{};
  // Decoded string.
  std::string last_complete_string_;
};

}  // namespace redsea
#endif  // RDSSTRING_H_
