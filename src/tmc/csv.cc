#include "src/tmc/csv.hh"

#include <cstdint>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace redsea {

std::vector<std::string> splitLine(const std::string& line, char delimiter) {
  std::stringstream ss(line);
  std::vector<std::string> result;

  while (ss.good()) {
    std::string val;
    std::getline(ss, val, delimiter);
    result.push_back(val);
  }

  return result;
}

std::vector<std::vector<std::string>> readCSV(const std::string& filename, char delimiter) {
  std::vector<std::vector<std::string>> lines;

  std::ifstream in(filename);
  if (!in.is_open())
    return lines;

  for (std::string line; std::getline(in, line);) {
    line.erase(std::remove(line.begin(), line.end(), '\r'), line.end());
    line.erase(std::remove(line.begin(), line.end(), '\n'), line.end());

    lines.emplace_back(splitLine(line, delimiter));
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
  return row.at(table.titles.at(title));
}

int get_int(const CSVTable& table, const CSVRow& row, const std::string& title) {
  return std::stoi(row.at(table.titles.at(title)));
}

std::uint16_t get_uint16(const CSVTable& table, const CSVRow& row, const std::string& title) {
  return static_cast<std::uint16_t>(get_int(table, row, title));
}

bool row_contains(const CSVTable& table, const CSVRow& row, const std::string& title) {
  return !row.at(table.titles.at(title)).empty();
}

}  // namespace redsea
