#ifndef TMC_CSV_H_
#define TMC_CSV_H_

#include <cstddef>
#include <cstdint>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace redsea {

struct CSVRow {
  CSVRow(const std::string& line, char delimiter) {
    offsets.reserve(8);
    lengths.reserve(8);
    std::size_t start = 0;
    std::size_t end   = 0;

    std::size_t length_until_line_feed = 0;
    while (length_until_line_feed < line.size() && line[length_until_line_feed] != '\r' &&
           line[length_until_line_feed] != '\n') {
      length_until_line_feed++;
    }

    while (end < length_until_line_feed) {
      end = line.find(delimiter, start);
      if (end == std::string::npos)
        end = length_until_line_feed;
      offsets.push_back(start);
      lengths.push_back(end - start);
      start = end + 1;
    }
    row_string = line;
  }
  std::string at(std::size_t i) const {
    if (i >= offsets.size())
      throw std::out_of_range("Index out of range");
    return row_string.substr(offsets[i], lengths[i]);
  }

  std::vector<std::size_t> offsets;
  std::vector<std::size_t> lengths;
  std::string row_string;
};

struct CSVTable {
  std::map<std::string, std::size_t> titles;
  std::vector<CSVRow> rows;
};

// Read a CSV table from a container of lines (e.g. vector of strings).
/// \param delimiter Element delimiter that splits each line into columns.
template <typename Container>
std::vector<CSVRow> readCSVContainer(const Container& csvdata, char delimiter) {
  std::vector<CSVRow> lines;

  for (const std::string& line : csvdata) {
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

  for (const std::string& line : csvdata) {
    if (is_title_row) {
      CSVRow row{line, delimiter};
      for (std::size_t i = 0; i < row.lengths.size(); i++) {
        table.titles[row.row_string.substr(row.offsets[i], row.lengths[i])] = i;
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

// Find an element by its title and return it as uint16_t.
// @throws exceptions from std::stoi
std::uint16_t get_uint16(const CSVTable& table, const CSVRow& row, const std::string& title);

// Find an element by its title and return it as bool.
bool row_contains(const CSVTable& table, const CSVRow& row, const std::string& title);

}  // namespace redsea

#endif  // TMC_CSV_H_
