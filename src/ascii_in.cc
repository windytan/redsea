#include "ascii_in.h"

#include <iostream>

namespace redsea {

AsciiBits::AsciiBits() : is_eof_(false) {

}

AsciiBits::~AsciiBits() {

}

int AsciiBits::getNextBit() {
  int result = 0;
  while (result != '0' && result != '1' && result != EOF)
    result = getchar();

  if (result == EOF) {
    is_eof_ = true;
    return 0;
  }

  return (result == '1');

}

bool AsciiBits::isEOF() const {
  return is_eof_;
}

std::vector<uint16_t> getNextGroupRSpy() {
  std::vector<uint16_t> result;

  bool finished = false;

  while (! (finished || std::cin.eof())) {
    std::string line;
    std::getline(std::cin, line);
    if (line.length() < 16)
      continue;

    for (int nblok=0; nblok<4; nblok++) {
      uint16_t bval=0;
      int nyb=0;

      while (nyb < 4) {

        if (line.length() < 1) {
          finished = true;
          break;
        }

        std::string single = line.substr(0,1);

        if (single.compare(" ") != 0) {
          try {
            int nval = std::stoi(std::string(single), nullptr, 16);
            bval = (bval << 4) + nval;
            nyb++;
          } catch (std::invalid_argument) {
            finished = true;
            break;
          }
        }
        line = line.substr(1);
      }

      if (finished)
        break;

      result.push_back(bval);

      if (nblok==3)
        finished = true;
    }
  }

  return result;

}


} // namespace redsea
