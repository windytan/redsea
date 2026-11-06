#pragma once

#include <cctype>
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <string>
#include <string_view>
#include <type_traits>

#include "src/util/maybe.hh"

namespace redsea {

// \brief Parse a possibly SI-suffixed number out of a string.
template <typename T>
Maybe<T> parseSI(std::string_view str) {
  static_assert(std::is_same_v<T, float> || std::is_same_v<T, std::int32_t>);
  if (str.empty()) {
    return {};
  }

  std::string buf(str);
  char* cstr   = buf.data();
  errno        = 0;
  char* endptr = cstr;
  T value;
  if constexpr (std::is_same_v<T, float>)
    value = std::strtof(cstr, &endptr);
  else
    value = std::strtol(cstr, &endptr, 10);

  if (errno == ERANGE) {
    // Value out of range
    return {};
  }

  T factor = 1;
  if (std::tolower(*endptr) == 'k') {
    factor = 1000;
  } else if (std::toupper(*endptr) == 'M') {
    factor = 1'000'000;
  } else if (*endptr != '\0') {
    // Extra characters
    return {};
  }

  if (factor > 1) {
    const T max_safe = std::numeric_limits<T>::max() / factor;
    const T min_safe = std::numeric_limits<T>::lowest() / factor;
    if (value > max_safe || value < min_safe) {
      return {};
    }
  }

  return Maybe<T>{value * factor};
}

}  // namespace redsea
