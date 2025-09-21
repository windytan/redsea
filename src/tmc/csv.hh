#ifndef TMC_CSV_H_
#define TMC_CSV_H_

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace redsea {

struct CSVRow {
  CSVRow(const std::string& line, char delimiter);
  [[nodiscard]] std::string at(std::size_t i) const;

  std::vector<std::size_t> offsets;
  std::vector<std::size_t> lengths;
  std::string row_string;
};

struct CSVTable {
  std::vector<std::string> titles;
  std::vector<CSVRow> rows;
};

// Read a CSV table from a container.
/// \param delimiter Element delimiter that splits each line into columns.
std::vector<CSVRow> readCSVContainer(const std::vector<std::string>& csvdata, char delimiter);

// Read a CSV table from a CSV file.
/// \param delimiter Element delimiter that splits each line into columns.
std::vector<CSVRow> readCSV(const std::string& filename, char delimiter);

// Read a CSV table from a CSV file. The first line is treated as a title row.
/// \param delimiter Element delimiter that splits each line into columns.
CSVTable readCSVWithTitles(const std::string& filename, char delimiter);

// Read a CSV table from a container of lines (e.g. vector of strings). The first line is treated as
// a title row.
/// \param delimiter Element delimiter that splits each line into columns.
CSVTable readCSVContainerWithTitles(const std::vector<std::string>& csvdata, char delimiter);

// Find an element by its title and return it as a string.
// @throws std::out_of_range if title not found, or row too short
std::string get_string(const CSVTable& table, const CSVRow& row, const std::string& title);

// Find an element by its title and return it as int.
// @throws exceptions from std::stoi
// @throws std::out_of_range if title not found, or row too short
int get_int(const CSVTable& table, const CSVRow& row, const std::string& title);

// Find an element by its title and return it as uint16_t.
// @throws exceptions from std::stoi
// @throws std::out_of_range if title not found, or row too short
std::uint16_t get_uint16(const CSVTable& table, const CSVRow& row, const std::string& title);

// Find out if an element by this title exists.
bool row_contains(const CSVTable& table, const CSVRow& row, const std::string& title);

}  // namespace redsea

#endif  // TMC_CSV_H_
