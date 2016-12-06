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

std::vector<std::string> splitline(std::string line, char delimiter);
std::vector<std::vector<std::string>> readCSV(std::vector<std::string> csvdata,
                                              char delimiter);
std::vector<std::vector<std::string>> readCSV(std::string filename,
                                              char delimiter, size_t numfields);

class CarrierFrequency {
 public:
  CarrierFrequency(uint16_t code, bool is_lf_mf=false);
  bool isValid() const;
  int getKhz() const;
  std::string getString() const;
  friend bool operator== (const CarrierFrequency &f1,
                          const CarrierFrequency &f2);
  friend bool operator< (const CarrierFrequency &f1,
                         const CarrierFrequency &f2);

 private:
  uint16_t code_;
  bool is_lf_mf_;
};

}  // namespace redsea
#endif  // UTIL_H_
