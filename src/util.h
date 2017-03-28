#ifndef UTIL_H_
#define UTIL_H_

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace redsea {

uint16_t Bits(uint16_t word, int starting_at, int len);

std::string Join(std::vector<std::string> strings, std::string);
std::string Join(std::vector<uint16_t> strings, std::string);

std::string HexString(int value, int numybbles);

class CSVRow {
 public:
  CSVRow(std::map<std::string, int> titles, std::vector<std::string> values);
  std::string at(std::string title) const;

 private:
  std::map<std::string, int> titles_;
  std::vector<std::string> values_;
};

std::vector<std::string> splitline(std::string line, char delimiter);
std::vector<std::vector<std::string>> ReadCSV(std::vector<std::string> csvdata,
                                              char delimiter);
std::vector<std::vector<std::string>> ReadCSV(std::string filename,
                                              char delimiter);
std::vector<CSVRow> ReadCSVWithTitles(std::string filename, char delimiter);
std::vector<CSVRow> ReadCSVWithTitles(std::vector<std::string> csvdata,
                                      char delimiter);

class CarrierFrequency {
 public:
  explicit CarrierFrequency(uint16_t code, bool is_lf_mf = false);
  bool valid() const;
  int kHz() const;
  std::string str() const;
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
