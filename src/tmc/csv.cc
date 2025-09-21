#include "src/tmc/csv.hh"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace redsea {

namespace {

int find_column_index(const std::vector<std::string>& headers, const std::string& name) {
  auto it = std::find(headers.begin(), headers.end(), name);
  if (it == headers.end())
    throw std::runtime_error("Column not found: " + name);
  return static_cast<int>(std::distance(headers.begin(), it));
}

}  // namespace

CSVRow::CSVRow(const std::string& line, char delimiter) {
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

std::string CSVRow::at(std::size_t i) const {
  if (i >= offsets.size())
    throw std::out_of_range("Index out of range");
  return row_string.substr(offsets[i], lengths[i]);
}

std::vector<CSVRow> readCSV(const std::string& filename, char delimiter) {
  std::vector<CSVRow> lines;

  std::ifstream in(filename);
  if (!in.is_open())
    return lines;

  for (std::string line; std::getline(in, line);) {
    lines.emplace_back(line, delimiter);
  }

  in.close();

  return lines;
}

CSVTable readCSVWithTitles(const std::string& filename, char delimiter) {
  std::vector<std::string> lines;

  std::ifstream in(filename);
  if (in.is_open()) {
    for (std::string line; std::getline(in, line);) {
      lines.push_back(line);
    }

    in.close();
  }

  return readCSVContainerWithTitles(lines, delimiter);
}

std::vector<CSVRow> readCSVContainer(const std::vector<std::string>& csvdata, char delimiter) {
  std::vector<CSVRow> lines;
  lines.reserve(csvdata.size());

  for (const std::string& line : csvdata) {
    lines.emplace_back(line, delimiter);
  }

  return lines;
}

CSVTable readCSVContainerWithTitles(const std::vector<std::string>& csvdata, char delimiter) {
  CSVTable table;
  table.rows.reserve(csvdata.size() - 1);

  bool is_title_row = true;

  for (const std::string& line : csvdata) {
    if (is_title_row) {
      CSVRow row{line, delimiter};
      table.titles.resize(row.lengths.size());
      for (std::size_t i = 0; i < row.lengths.size(); i++) {
        table.titles[i] = row.row_string.substr(row.offsets[i], row.lengths[i]);
      }
      is_title_row = false;
    } else {
      table.rows.emplace_back(line, delimiter);
    }
  }

  return table;
}

std::string get_string(const CSVTable& table, const CSVRow& row, const std::string& title) {
  return row.at(find_column_index(table.titles, title));
}

int get_int(const CSVTable& table, const CSVRow& row, const std::string& title) {
  return std::stoi(row.at(find_column_index(table.titles, title)));
}

std::uint16_t get_uint16(const CSVTable& table, const CSVRow& row, const std::string& title) {
  return static_cast<std::uint16_t>(get_int(table, row, title));
}

bool row_contains(const CSVTable& table, const CSVRow& row, const std::string& title) {
  return !row.at(find_column_index(table.titles, title)).empty();
}

}  // namespace redsea
