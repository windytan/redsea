#include "src/text/radiotext.hh"

#include <cstddef>
#include <cstdint>

#include "src/text/rdsstring.hh"

namespace redsea {

bool RadioText::isABChanged(int new_ab) {
  const bool is = (ab != new_ab);
  ab            = new_ab;
  return is;
}

void RadioText::update(std::size_t pos, std::uint8_t byte1, std::uint8_t byte2) {
  text.set(pos, byte1, byte2);
}

void ProgramServiceName::update(std::size_t pos, std::uint8_t byte1, std::uint8_t byte2) {
  text.set(pos, byte1, byte2);
}

LongPS::LongPS() {
  text.setEncoding(RDSString::Encoding::UTF8);
}
void LongPS::update(std::size_t pos, std::uint8_t byte1, std::uint8_t byte2) {
  text.set(pos, byte1, byte2);
}

PTYName::PTYName() : text(8) {}
bool PTYName::isABChanged(int new_ab) {
  const bool is = (ab != new_ab);
  ab            = new_ab;
  return is;
}

void PTYName::update(std::size_t pos, std::uint8_t char1, std::uint8_t char2, std::uint8_t char3,
                     std::uint8_t char4) {
  text.set(pos, char1, char2);
  text.set(pos + 2, char3, char4);
}

}  // namespace redsea
