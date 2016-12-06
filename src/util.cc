#include "src/util.h"

#include <fstream>
#include <iomanip>
#include <sstream>

namespace redsea {

// extract len bits from word, starting at starting_at from the right
uint16_t bits(uint16_t word, int starting_at, int len) {
  return ((word >> starting_at) & ((1 << len) - 1));
}

std::string join(std::vector<std::string> strings, std::string d) {
  std::string result("");
  for (size_t i=0; i < strings.size(); i++) {
    result += strings[i];
    if (i < strings.size()-1)
      result += d;
  }
  return result;
}

std::string join(std::vector<std::uint16_t> nums, std::string d) {
  std::string result("");
  for (size_t i=0; i < nums.size(); i++) {
    result += std::to_string(nums[i]);
    if (i < nums.size()-1)
      result += d;
  }
  return result;
}

std::string hexString(int value, int numnybbles) {
  std::stringstream ss;

  ss.fill('0');
  ss.setf(std::ios_base::uppercase);

  ss << std::hex << std::setw(numnybbles) << value;

  return ss.str();
}

// 3.2.1.6
CarrierFrequency::CarrierFrequency(uint16_t code, bool is_lf_mf) :
    code_(code), is_lf_mf_(is_lf_mf) {
}

bool CarrierFrequency::isValid() const {
  return ((is_lf_mf_ && code_ >= 1 && code_ <= 135) ||
         (!is_lf_mf_ && code_ >= 1 && code_ <= 204));
}

int CarrierFrequency::getKhz() const {
  int khz = 0;
  if (isValid()) {
    if (!is_lf_mf_)
      khz = 87500 + 100 * code_;
    else if (code_ <= 15)
      khz = 144 + 9 * code_;
    else
      khz = 522 + (9 * (code_ - 15));
  }

  return khz;
}

std::string CarrierFrequency::getString() const {
  float num = (is_lf_mf_ ? getKhz() : getKhz() / 1000.0f);
  std::stringstream ss;
  ss.precision(is_lf_mf_ ? 0 : 1);
  if (isValid())
    ss << std::fixed << num << (is_lf_mf_ ? " kHz" : " MHz");
  else
    ss << "N/A";
  return ss.str();
};

bool operator== (const CarrierFrequency &f1,
                 const CarrierFrequency &f2) {
  return (f1.getKhz() == f2.getKhz());
}

bool operator< (const CarrierFrequency &f1,
                const CarrierFrequency &f2) {
  return (f1.getKhz() < f2.getKhz());
}

std::vector<std::string> splitline(std::string line, char delimiter) {
  std::stringstream ss(line);
  std::vector<std::string> result;

  while (ss.good()) {
    std::string val;
    std::getline(ss, val, delimiter);
    result.push_back(val);
  }

  return result;
}

std::vector<std::vector<std::string>> readCSV(std::vector<std::string> csvdata,
                                              char delimiter) {
  std::vector<std::vector<std::string>> lines;

  for (std::string line : csvdata)
    lines.push_back(splitline(line, delimiter));

  return lines;
}

std::vector<std::vector<std::string>> readCSV(std::string filename,
                                              char delimiter,
                                              size_t numfields) {
  std::vector<std::vector<std::string>> lines;

  std::ifstream in(filename);
  if (!in.is_open())
    return lines;

  for (std::string line; std::getline(in, line); ) {
    if (!in.good())
      break;

    std::vector<std::string> fields = splitline(line, delimiter);
    if (fields.size() == numfields)
      lines.push_back(fields);
  }

  in.close();

  return lines;
}


}  // namespace redsea
