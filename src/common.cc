/*
 * Copyright (c) OH2EIQ
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

#include "src/common.h"

namespace redsea {

std::string getTimePointString(
    const std::chrono::time_point<std::chrono::system_clock>& timepoint,
    const std::string& format) {

  // This is done to ensure we get truncation and not rounding to integer seconds
  const auto seconds_since_epoch(
      std::chrono::duration_cast<std::chrono::seconds>(timepoint.time_since_epoch()));
  const std::time_t t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::time_point(seconds_since_epoch));

  std::string format_with_fractional(format);
  const std::size_t found = format_with_fractional.find("%f");
  if (found != std::string::npos) {
    const int ms         = (std::chrono::duration_cast<std::chrono::milliseconds>(timepoint.time_since_epoch() -
                            seconds_since_epoch)).count();
    const int hundredths = (ms / 10)  % 10;
    const int tenths     = (ms / 100) % 10;

    format_with_fractional.replace(found, 2, std::to_string(tenths) + std::to_string(hundredths));
  }

  char buffer[64];
  if (std::strftime(buffer, sizeof(buffer), format_with_fractional.c_str(), std::localtime(&t)) == 0) {
    return "(format error)";
  }

  return std::string(buffer);
}

}  // namespace redsea
