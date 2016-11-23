#include "util.h"

#include <iomanip>
#include <sstream>

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

std::string jsonVal(std::string key, std::string value, bool leading_comma) {
  std::string json;
  if (leading_comma)
    json = ",";

  json += "\"" + key + "\":\"" + value + "\"";

  return json;
}

std::string jsonVal(std::string key, int value, bool leading_comma) {
  std::string json;
  if (leading_comma)
    json = ",";

  json += "\"" + key + "\":" + std::to_string(value);

  return json;
}

std::string jsonVal(std::string key, float value, bool leading_comma) {
  std::string json;
  if (leading_comma)
    json = ",";

  json += "\"" + key + "\":" + std::to_string(value);

  return json;
}

std::string jsonVal(std::string key, int value, int num_nybbles,
    bool leading_comma) {
  std::stringstream ss;
  ss.fill('0');
  ss << std::hex << std::setw(num_nybbles) << std::showbase << value;

  return  "\"" + key + "\":\"" + ss.str() + "\"";
}

std::string jsonVal(std::string key, bool value, bool leading_comma) {
  std::string json;
  if (leading_comma)
    json = ",";

  json += "\"" + key + "\":" + (value ? "true" : "false");

  return json;
}

std::string jsonArray(std::string name, std::string contents,
                      bool leading_comma) {
  std::string json;
  if (leading_comma)
    json = ",";

  json += "\"" + name + "\":[" + contents + "]";

  return json;

}

std::string jsonObject(std::string name, std::string contents,
                      bool leading_comma) {
  std::string json;
  if (leading_comma)
    json = ",";

  json += "\"" + name + "\":{" + contents + "}";

  return json;

}

} // namespace redsea
