#ifndef UTIL_H_
#define UTIL_H_

#include <cstdint>
#include <string>
#include <vector>

namespace redsea {

uint16_t bits (uint16_t word, int starting_at, int len);

std::string join(std::vector<std::string> strings, std::string);
std::string join(std::vector<uint16_t> strings, std::string);

std::string jsonVal(std::string key, std::string value,
    bool leading_comma = true);
std::string jsonVal(std::string key, bool value, bool leading_comma = true);
std::string jsonVal(std::string key, int value, bool leading_comma = true);
std::string jsonVal(std::string key, int value, int num_nybbles,
    bool leading_comma = true);
std::string jsonVal(std::string key, float value, bool leading_comma = true);
std::string jsonArray(std::string name, std::string contents,
                      bool leading_comma=true);
std::string jsonObject(std::string name, std::string contents,
                      bool leading_comma=true);

} // namespace redsea
#endif // UTIL_H_
