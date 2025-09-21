#ifndef TEXT_STRINGUTIL_HH_
#define TEXT_STRINGUTIL_HH_

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

namespace redsea {

std::string getHoursMinutesString(int hour, int minute);
std::string getTimePointString(const std::chrono::time_point<std::chrono::system_clock>& timepoint,
                               const std::string& format);

std::string join(const std::vector<std::string>& strings, const std::string& d);

std::string getHexString(int value, int n);
std::string getHexString(std::uint32_t value, int n);
std::string getPrefixedHexString(int value, int n);

std::string rtrim(std::string s);

}  // namespace redsea

#endif  // TEXT_STRINGUTIL_HH_
