#include "src/util/csv.hh"

#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

namespace redsea {

namespace {
std::size_t findTitleIndex(const CSVTable& table, const std::string& title) {
  for (std::size_t i = 0; i < table.titles.size(); i++) {
    if (table.titles[i] == title)
      return i;
  }
  throw std::out_of_range("CSVTable::findTitleIndex: title not found");
}
}  // namespace

CSVRow::CSVRow(const std::string_view& line, char delimiter) : row_string(line) {
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
}

std::string CSVRow::at(std::size_t i) const {
  if (i >= offsets.size())
    throw std::out_of_range("CSVRow::at: index out of range");

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

std::string get_string(const CSVTable& table, const CSVRow& row, const std::string& title) {
  return row.at(findTitleIndex(table, title));
}

int get_int(const CSVTable& table, const CSVRow& row, const std::string& title) {
  return std::stoi(row.at(findTitleIndex(table, title)));
}

int get_int(const CSVRow& row, std::size_t index) {
  return std::stoi(row.at(index));
}

std::uint16_t get_uint16(const CSVTable& table, const CSVRow& row, const std::string& title) {
  return static_cast<std::uint16_t>(get_int(table, row, title));
}

std::uint16_t get_uint16(const CSVRow& row, std::size_t index) {
  return static_cast<std::uint16_t>(std::stoi(row.at(index)));
}

bool row_contains(const CSVTable& table, const CSVRow& row, const std::string& title) {
  return !row.at(findTitleIndex(table, title)).empty();
}

}  // namespace redsea
