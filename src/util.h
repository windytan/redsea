#ifndef UTIL_H_
#define UTIL_H_

#include <cstdint>
#include <string>
#include <vector>

namespace redsea {

uint16_t bits (uint16_t bitstring, int starting_at, int len);

std::string commaJoin(std::vector<std::string> strings);
std::string commaJoin(std::vector<uint16_t> strings);

} // namespace redsea
#endif // UTIL_H_
