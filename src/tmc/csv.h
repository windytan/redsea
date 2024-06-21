#ifndef TMC_CSV_H_
#define TMC_CSV_H_

#include <algorithm>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace redsea {

using CSVRow = std::vector<std::string>;

struct CSVTable {
  std::map<std::string, size_t> titles;
  std::vector<CSVRow> rows;
};

std::vector<std::string> splitLine(const std::string& line, char delimiter);

template <typename Container>
std::vector<std::vector<std::string>> readCSVContainer(const Container& csvdata, char delimiter) {
  std::vector<std::vector<std::string>> lines;

  std::transform(csvdata.cbegin(), csvdata.cend(), std::back_inserter(lines),
                 [&](const std::string& line) { return splitLine(line, delimiter); });

  return lines;
}

std::vector<std::vector<std::string>> readCSV(const std::string& filename, char delimiter);
CSVTable readCSVWithTitles(const std::string& filename, char delimiter);

template <typename Container>
CSVTable readCSVContainerWithTitles(const Container& csvdata, char delimiter) {
  CSVTable table;

  bool is_title_row = true;

  for (std::string line : csvdata) {
    line.erase(std::remove(line.begin(), line.end(), '\r'), line.end());
    line.erase(std::remove(line.begin(), line.end(), '\n'), line.end());

    const std::vector<std::string> fields = splitLine(line, delimiter);
    if (is_title_row) {
      for (size_t i = 0; i < fields.size(); i++) table.titles[fields[i]] = i;
      is_title_row = false;
    } else {
      if (fields.size() <= table.titles.size())
        table.rows.push_back(fields);
    }
  }

  return table;
}

std::string get_string(const CSVTable& table, const CSVRow& row, const std::string& title);
int get_int(const CSVTable& table, const CSVRow& row, const std::string& title);
uint16_t get_uint16(const CSVTable& table, const CSVRow& row, const std::string& title);
bool row_contains(const CSVTable& table, const CSVRow& row, const std::string& title);

}  // namespace redsea

#endif  // TMC_CSV_H_
