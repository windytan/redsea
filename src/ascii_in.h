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
#ifndef ASCII_IN_H_
#define ASCII_IN_H_

#include <cstdint>
#include <vector>

#include "src/groups.h"

namespace redsea {

class AsciiBits {
 public:
  explicit AsciiBits(bool feed_thru = false);
  ~AsciiBits();
  int NextBit();
  bool eof() const;

 private:
  bool is_eof_;
  bool feed_thru_;
};

Group NextGroupRSpy(bool feed_thru = false);

}  // namespace redsea
#endif // ASCII_IN_H_
