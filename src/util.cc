#include "util.h"

namespace redsea {

// extract len bits from word, starting at starting_at from the right
uint16_t bits (uint16_t word, int starting_at, int len) {
  return ((word >> starting_at) & ((1<<len) - 1));
}

std::string join(std::vector<std::string> strings, std::string d) {
  std::string result("");
  for (size_t i=0; i<strings.size(); i++) {
    result += strings[i];
    if (i < strings.size()-1)
      result += d;
  }
  return result;
}

std::string join(std::vector<std::uint16_t> nums, std::string d) {
  std::string result("");
  for (size_t i=0; i<nums.size(); i++) {
    result += std::to_string(nums[i]);
    if (i < nums.size()-1)
      result += d;
  }
  return result;
}

} // namespace redsea
