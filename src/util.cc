#include "util.h"

namespace redsea {

// extract len bits from bitstring, starting at starting_at from the right
uint16_t bits (uint16_t bitstring, int starting_at, int len) {
  return ((bitstring >> starting_at) & ((1<<len) - 1));
}

std::string commaJoin(std::vector<std::string> strings) {
  std::string result("");
  for (size_t i=0; i<strings.size(); i++) {
    result += strings[i];
    if (i < strings.size()-1)
      result += ", ";
  }
  return result;
}

} // namespace redsea
