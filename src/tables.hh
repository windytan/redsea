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
#ifndef TABLES_H_
#define TABLES_H_

#include <cstdint>
#include <string>

namespace redsea {

std::string getPTYNameString(std::uint16_t pty);
std::string getPTYNameStringRBDS(std::uint16_t pty);
std::string getCountryString(std::uint16_t cc, std::uint16_t ecc);
std::string getLanguageString(std::uint16_t code);
std::string getAppNameString(std::uint16_t aid);
std::string getRTPlusContentTypeString(std::uint32_t content_type);
std::string getDICodeString(std::uint16_t di);
std::string getCallsignFromPI(std::uint16_t pi);

}  // namespace redsea
#endif  // TABLES_H_
