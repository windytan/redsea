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
#ifndef DATA_H_
#define DATA_H_

#include <string>

#include "src/common.h"

namespace redsea {

std::string RDSCharString(uint8_t code, eCodeTable codetable = G0);
std::string PTYNameString(int pty, bool is_rbds = false);
std::string CountryString(uint16_t pi, uint16_t ecc);
std::string LanguageString(uint16_t code);
std::string AppNameString(uint16_t aid);
std::string RTPlusContentTypeString(uint16_t content_type);
std::string DICodeString(uint16_t di);

}  // namespace redsea
#endif  // DATA_H_
