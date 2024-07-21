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

// Split a string into substrings by the delimiter character.
std::vector<std::string> splitLine(const std::string& line, char delimiter);

// Read a CSV table from a container of lines (e.g. vector of strings).
/// \param delimiter Element delimiter that splits each line into columns.
template <typename Container>
std::vector<std::vector<std::string>> readCSVContainer(const Container& csvdata, char delimiter) {
  std::vector<std::vector<std::string>> lines;

  std::transform(csvdata.cbegin(), csvdata.cend(), std::back_inserter(lines),
                 [&](const std::string& line) { return splitLine(line, delimiter); });

  return lines;
}

// Read a CSV table from a CSV file.
/// \param delimiter Element delimiter that splits each line into columns.
std::vector<std::vector<std::string>> readCSV(const std::string& filename, char delimiter);

// Read a CSV table from a CSV file. The first line is treated as a title row.
/// \param delimiter Element delimiter that splits each line into columns.
CSVTable readCSVWithTitles(const std::string& filename, char delimiter);

// Read a CSV table from a container of lines (e.g. vector of strings). The first line is treated as
// a title row.
/// \param delimiter Element delimiter that splits each line into columns.
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

// Find an element by its title and return it as a string.
std::string get_string(const CSVTable& table, const CSVRow& row, const std::string& title);

// Find an element by its title and return it as int.
int get_int(const CSVTable& table, const CSVRow& row, const std::string& title);

// Find an element by its title and return it as uint16_t.
uint16_t get_uint16(const CSVTable& table, const CSVRow& row, const std::string& title);

// Find an element by its title and return it as bool.
bool row_contains(const CSVTable& table, const CSVRow& row, const std::string& title);

}  // namespace redsea

#endif  // TMC_CSV_H_
