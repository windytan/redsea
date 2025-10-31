#ifndef TMC_CSV_H_
#define TMC_CSV_H_

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace redsea {

struct CSVRow {
  CSVRow(const std::string_view& line, char delimiter);
  [[nodiscard]] std::string at(std::size_t i) const;

  std::vector<std::size_t> offsets;
  std::vector<std::size_t> lengths;
  std::string row_string;
};

struct CSVTable {
  std::vector<std::string> titles;
  std::vector<CSVRow> rows;
};

// Read a CSV table from a container of lines (e.g. vector of strings).
/// \param delimiter Element delimiter that splits each line into columns.
template <typename Container>
std::vector<CSVRow> readCSVContainer(const Container& csvdata, char delimiter) {
  std::vector<CSVRow> lines;

  for (const auto& line : csvdata) {
    lines.emplace_back(line, delimiter);
  }

  return lines;
}

// Read a CSV table from a CSV file.
/// \param delimiter Element delimiter that splits each line into columns.
std::vector<CSVRow> readCSV(const std::string& filename, char delimiter);

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

  for (const auto& line : csvdata) {
    if (is_title_row) {
      CSVRow row{line, delimiter};
      for (std::size_t i = 0; i < row.lengths.size(); i++) {
        table.titles.emplace_back(row.row_string.substr(row.offsets[i], row.lengths[i]));
      }
      is_title_row = false;
    } else {
      table.rows.emplace_back(line, delimiter);
    }
  }

  return table;
}

// Find an element by its title and return it as a string.
std::string get_string(const CSVTable& table, const CSVRow& row, const std::string& title);

// Find an element by its title and return it as int.
// @throws exceptions from std::stoi
int get_int(const CSVTable& table, const CSVRow& row, const std::string& title);

int get_int(const CSVRow& row, std::size_t index);

// Find an element by its title and return it as uint16_t.
// @throws exceptions from std::stoi
std::uint16_t get_uint16(const CSVTable& table, const CSVRow& row, const std::string& title);

std::uint16_t get_uint16(const CSVRow& row, std::size_t index);

// Does the row contain a field with this title?
bool row_contains(const CSVTable& table, const CSVRow& row, const std::string& title);

}  // namespace redsea

#endif  // TMC_CSV_H_
