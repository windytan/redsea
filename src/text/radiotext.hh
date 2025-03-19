#ifndef REDSEA_TEXT_RADIOTEXT_HH
#define REDSEA_TEXT_RADIOTEXT_HH

#include <cstddef>
#include <cstdint>
#include <string>

#include "src/text/rdsstring.hh"

namespace redsea {

class RadioText {
 public:
  RadioText() = default;
  bool isABChanged(int new_ab) {
    const bool is = (ab != new_ab);
    ab            = new_ab;
    return is;
  }
  void update(std::size_t pos, std::uint8_t byte1, std::uint8_t byte2) {
    text.set(pos, byte1, byte2);
  }

  struct Plus {
    bool exists{};
    bool cb{};
    std::uint16_t scb{};
    std::uint16_t template_num{};
    bool toggle{};
    bool item_running{};

    struct Tag {
      std::uint16_t content_type{};
      std::uint16_t start{};
      std::uint16_t length{};
    };
  };

  RDSString text{64};
  Plus plus;
  std::string previous_potentially_complete_message;
  int ab{};
};

class ProgramServiceName {
 public:
  ProgramServiceName() = default;
  void update(std::size_t pos, std::uint8_t byte1, std::uint8_t byte2) {
    text.set(pos, byte1, byte2);
  }

  RDSString text{8};
};

class LongPS {
 public:
  LongPS() {
    text.setEncoding(RDSString::Encoding::UTF8);
  }
  void update(std::size_t pos, std::uint8_t byte1, std::uint8_t byte2) {
    text.set(pos, byte1, byte2);
  }

  RDSString text{32};
};

class PTYName {
 public:
  PTYName() : text(8) {}
  bool isABChanged(int new_ab) {
    const bool is = (ab != new_ab);
    ab            = new_ab;
    return is;
  }
  void update(std::size_t pos, std::uint8_t char1, std::uint8_t char2, std::uint8_t char3,
              std::uint8_t char4) {
    text.set(pos, char1, char2);
    text.set(pos + 2, char3, char4);
  }

  RDSString text;
  int ab{0};
};

}  // namespace redsea

#endif  // REDSEA_TEXT_RADIOTEXT_HH
