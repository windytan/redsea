#include "src/text/stringutil.hh"

#include <array>
#include <chrono>
#include <cstddef>
#include <ctime>
#include <string>
#include <vector>

namespace redsea {

// \brief Format hours, minutes as HH:MM
std::string getHoursMinutesString(int hour, int minute) {
  return std::to_string(hour).insert(0, 2 - std::to_string(hour).length(), '0') + ":" +
         std::to_string(minute).insert(0, 2 - std::to_string(minute).length(), '0');
}

// \brief Format a system clock timestamp
std::string getTimePointString(const std::chrono::time_point<std::chrono::system_clock>& timepoint,
                               const std::string& format) {
  // This is done to ensure we get truncation and not rounding to integer seconds
  const auto seconds_since_epoch(
      std::chrono::duration_cast<std::chrono::seconds>(timepoint.time_since_epoch()));
  const std::time_t t = std::chrono::system_clock::to_time_t(
      std::chrono::system_clock::time_point(seconds_since_epoch));

  std::string format_with_fractional(format);
  const std::size_t found = format_with_fractional.find("%f");
  if (found != std::string::npos) {
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                        timepoint.time_since_epoch() - seconds_since_epoch)
                        .count();
    const auto hundredths = (ms / 10) % 10;
    const auto tenths     = (ms / 100) % 10;

    format_with_fractional.replace(found, 2, std::to_string(tenths) + std::to_string(hundredths));
  }

  std::array<char, 64> buffer{};
  if (std::strftime(buffer.data(), buffer.size(), format_with_fractional.c_str(),
                    std::localtime(&t)) == 0) {
    return "(format error)";
  }

  return {buffer.data()};
}

// \brief Join strings with a delimiter
std::string join(const std::vector<std::string>& strings, const std::string& d) {
  if (strings.empty())
    return "";

  std::string result;
  for (size_t i = 0; i < strings.size(); i++) {
    result += strings[i];
    if (i < strings.size() - 1)
      result += d;
  }
  return result;
}

std::string rtrim(std::string s) {
  return s.erase(s.find_last_not_of(' ') + 1);
}

std::string getHexString(int value, int n) {
  std::string result;
  result.resize(n);
  for (int i = 0; i < n; i++) {
    const auto nybble =
        (static_cast<unsigned int>(value) >> static_cast<unsigned int>(4 * (n - 1 - i)) & 0x0FU);
    if (nybble < 10U)
      result[i] = static_cast<char>('0' + nybble);
    else
      result[i] = static_cast<char>('A' + (nybble - 10U));
  }

  return result;
}

std::string getHexString(std::uint32_t value, int n) {
  return getHexString(static_cast<int>(value), n);
}

std::string getPrefixedHexString(int value, int n) {
  return "0x" + getHexString(value, n);
}

}  // namespace redsea
