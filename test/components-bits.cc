// Redsea tests: Component tests for bit-level functionality

#include <string>

#include <catch2/catch_test_macros.hpp>

#include "../src/block_sync.hh"
#include "../src/options.hh"
#include "test_helpers.hh"

TEST_CASE("PI search") {
  redsea::Options options;

  SECTION("Accepts new PI from three repeats") {
    // Vikerraadio (ee)
    // clang-format off
    const auto json_lines{asciibin2json({
                                                       "001"
      "1110110110111010011100010101001000010100001110000010"
      "0010001011100001011100110000100101100000111100111110"
      "0010000001100101101101001101101001001000000110111110"
      "0010001011100001011100110000000101100010010011100000"
      "1010011010110011111010010101010011010011000101010101"
      "0010001011100001011100110000100101100001001010101000"
      "0111001101100001010000011001100001000011010111000111"
      "001000"
    }, options)};
    // clang-format on

    REQUIRE(json_lines.size() == 1);
    CHECK(json_lines[0]["pi"] == "0x22E1");
  }

  SECTION("Ignores phantom sync caused by data-mimicking") {
    // Noise that shouldn't even sync
    // It also happens to look like two repeats of PI 0x40AF
    // clang-format off
    const auto groups{asciibin2groups({
      "1100001001000011110110110010101010011101101100110001010011111011"
      "1110001001000001100101000011111110101011001100100011010111001100"
      "0100010001001110001101001001000000011011001010100000001011110001"
      "1100110001010011000010111010101000101000001001000101100110000110"
      "0001000000101011111000100001000110111101011000010110000010011101"
      "0010111010001101001010011011100100000011000101010000101100101010"
      "0100100110000101110000010101101011011100000100100010010010110100"
      "0001010010100010010100000010101101100010011100001000101111110011"
      "0001001000100100111110100000100110110011110110000111010100000000"
    }, options)};
    // clang-format on

    CHECK(groups.empty());
  }
}

TEST_CASE("Error detection and correction") {
  redsea::Options options;
  // clang-format off
  const std::string correct_group{
    "0010001011100001" "0111001100"
    "0010010110000011" "1100111110"
    "0010000001100101" "1011010011"
    "0110100100100000" "0110111110"};
  // clang-format on

  SECTION("Detects error-free group") {
    const std::string test_data{correct_group + correct_group};
    const auto groups{asciibin2groups(test_data, options)};

    CHECK(groups.back().getNumErrors() == 0);
  }

  SECTION("Detects long error burst") {
    const std::string broken_group = [&]() {
      std::string broken = correct_group;
      flipAsciiBit(broken, 1);
      flipAsciiBit(broken, 2);
      flipAsciiBit(broken, 9);
      flipAsciiBit(broken, 10);
      return broken;
    }();

    const std::string test_data{correct_group + correct_group + broken_group};
    const auto groups{asciibin2groups(test_data, options)};

    CHECK(groups.back().getNumErrors() == 1);
  }

  SECTION("Corrects double bit flip") {
    const std::string broken_group = [&]() {
      std::string broken = correct_group;
      flipAsciiBit(broken, 1);
      flipAsciiBit(broken, 2);
      return broken;
    }();

    const std::string test_data{correct_group + correct_group + broken_group};
    const auto groups{asciibin2groups(test_data, options)};

    CHECK(groups.back().getNumErrors() == 1);
    CHECK(groups.back().has(redsea::BLOCK1));
    CHECK(groups.back().get(redsea::BLOCK1) == 0x22E1);
  }

  SECTION("Rejects triple bit flip") {
    const std::string broken_group = [&]() {
      std::string broken = correct_group;
      flipAsciiBit(broken, 1);
      flipAsciiBit(broken, 2);
      flipAsciiBit(broken, 3);
      return broken;
    }();

    const std::string test_data{correct_group + correct_group + broken_group};
    const auto groups{asciibin2groups(test_data, options)};

    CHECK(groups.back().getNumErrors() == 1);
    CHECK_FALSE(groups.back().has(redsea::BLOCK1));
    CHECK(groups.back().get(redsea::BLOCK1) == 0x0000);  // "----"
  }

  SECTION("Rejects double bit flip if FEC is disabled") {
    options.use_fec = false;

    const std::string broken_group = [&]() {
      std::string broken = correct_group;
      flipAsciiBit(broken, 1);
      flipAsciiBit(broken, 2);
      return broken;
    }();

    const std::string test_data{correct_group + correct_group + broken_group};
    const auto groups{asciibin2groups(test_data, options)};

    CHECK(groups.back().getNumErrors() == 1);
    CHECK_FALSE(groups.back().has(redsea::BLOCK1));
    CHECK(groups.back().get(redsea::BLOCK1) == 0x0000);  // "----"
  }
}
