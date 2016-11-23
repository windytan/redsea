#ifndef UTIL_H_
#define UTIL_H_

#include <cstdint>
#include <string>
#include <vector>

namespace redsea {

uint16_t bits (uint16_t word, int starting_at, int len);

std::string join(std::vector<std::string> strings, std::string);
std::string join(std::vector<uint16_t> strings, std::string);

std::string hexString(int value, int numybbles);

} // namespace redsea
#endif // UTIL_H_
