// Redsea tests: Unit tests

#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include "src/rft/base64.hh"
#include "src/rft/crc.hh"
#include "src/simplemap.hh"
#include "src/text/stringutil.hh"
#include "src/tmc/csv.hh"
#include "src/util.hh"

TEST_CASE("Bitfield extraction") {
  constexpr std::uint16_t block1{0b0001'0010'0011'0100};
  constexpr std::uint16_t block2{0b0101'0110'0111'1000};

  SECTION("Single block") {
    // clang-format off
    CHECK(redsea::getBits(block1, 0, 4) ==             0b0100);

    CHECK(redsea::getBits(block1, 4, 5) ==      0b0'0011);
    CHECK(redsea::getBits(block1, 4, 6) ==     0b10'0011);
    CHECK(redsea::getBits(block1, 4, 8) ==   0b0010'0011);
    CHECK(redsea::getBits(block1, 4, 9) == 0b1'0010'0011);

    CHECK(redsea::getBits(block1, 5, 5) ==     0b10'001);
    CHECK(redsea::getBits(block1, 5, 8) == 0b1'0010'001);

    CHECK(redsea::getBool(block1, 12)   == true);
    // clang-format on
  }

  SECTION("Concatenation of two blocks") {
    // clang-format off
    CHECK(redsea::getBits(block1, block2, 0, 4)                        == 0b1000);

    CHECK(redsea::getBits(block1, block2, 4, 5)  ==                0b0'0111);
    CHECK(redsea::getBits(block1, block2, 4, 6)  ==               0b10'0111);
    CHECK(redsea::getBits(block1, block2, 4, 8)  ==             0b0110'0111);
    CHECK(redsea::getBits(block1, block2, 4, 9)  ==           0b1'0110'0111);

    CHECK(redsea::getBits(block1, block2, 8, 12) ==   0b0100'0101'0110);
    CHECK(redsea::getBits(block1, block2, 9, 12) == 0b1'0100'0101'011);
    // clang-format on
  }
}

TEST_CASE("CSV reader") {
  char testfilename[]  = "/tmp/redsea-test-XXXXXX";
  const int testfilefd = ::mkstemp(testfilename);
  REQUIRE(testfilefd != -1);

  std::ofstream out(testfilename);
  // Test string has:
  // - title line
  // - empty column
  // - empty line
  // - signed, non-signed integers
  // - string with UTF-8 characters
  // - both CRLF and LF line feeds
  // - last line does not have a line feed
  out << "num;a;empty;b;c\r\n0;-16;;+8;7\n\nzero;minus 16;;plus 8;seitsemän";
  out.close();

  SECTION("Simple read without titles") {
    const auto csv = redsea::readCSV(testfilename, ';');

    CHECK(csv.size() == 4);
    CHECK(csv.at(1).lengths.size() == 5);
    CHECK(csv.at(1).at(2) == "");
    CHECK(csv.at(1).at(4) == "7");
  }

  SECTION("Get values by column title") {
    const auto csv = redsea::readCSVWithTitles(testfilename, ';');

    CHECK(csv.rows.size() == 3);
    CHECK(redsea::get_int(csv, csv.rows.at(0), "a") == -16);
    CHECK(redsea::get_int(csv, csv.rows.at(0), "b") == 8);
    CHECK(redsea::get_uint16(csv, csv.rows.at(0), "c") == 7);

    REQUIRE_THROWS_AS(redsea::get_string(csv, csv.rows.at(1), "a"), std::out_of_range);

    CHECK(redsea::get_string(csv, csv.rows.at(2), "empty") == "");
    CHECK(redsea::get_string(csv, csv.rows.at(2), "c") == "seitsemän");
  }
}

TEST_CASE("Clock-time formatting") {
  SECTION("From data (Hours + minutes)") {
    const auto str = redsea::getHoursMinutesString(1, 1);
    CHECK(str == "01:01");
  }

  SECTION("From system clock (yyyy-mm-dd)") {
    const std::chrono::time_point<std::chrono::system_clock> time_point(
        std::chrono::milliseconds(0));

    const auto time_string = redsea::getTimePointString(time_point, "%Y-%m-%d");
    // We can't say much about the string but at least it should be 10 characters, right?
    CHECK(time_string.length() == 10);
  }

  SECTION("From system clock (hh:mm:ss.ss)") {
    const std::chrono::time_point<std::chrono::system_clock> time_point(
        std::chrono::milliseconds(0));
    const auto time_string = redsea::getTimePointString(time_point, "%H:%M:%S.%f");
    CHECK(time_string.length() == 11);
    CHECK(time_string.substr(7, 4) == "0.00");
  }
}

TEST_CASE("Base64 encoding") {
  const std::string test_string1{"light wor"};
  const std::string encoded1 = redsea::asBase64(test_string1.c_str(), test_string1.size());
  CHECK(encoded1 == "bGlnaHQgd29y");

  const std::string test_string2{"light wo"};
  const std::string encoded2 = redsea::asBase64(test_string2.c_str(), test_string2.size());
  CHECK(encoded2 == "bGlnaHQgd28=");

  const std::string test_string3{"light w"};
  const std::string encoded3 = redsea::asBase64(test_string3.c_str(), test_string3.size());
  CHECK(encoded3 == "bGlnaHQgdw==");

  const std::string test_string4{};
  const std::string encoded4 = redsea::asBase64(test_string4.c_str(), test_string4.size());
  CHECK(encoded4 == "");
}

TEST_CASE("CRC16") {
  // Pg. 84 + padding to test the address offset
  const std::vector<std::uint8_t> test_bytes{
      0x00, 0x32, 0x44, 0x31, 0x31, 0x31, 0x32, 0x33, 0x34, 0x30, 0x31, 0x30,
      0x31, 0x30, 0x35, 0x41, 0x42, 0x43, 0x44, 0x31, 0x32, 0x33, 0x46, 0x30,
      0x58, 0x58, 0x58, 0x58, 0x31, 0x31, 0x30, 0x36, 0x39, 0x32, 0x31, 0x32,
      0x34, 0x39, 0x31, 0x30, 0x30, 0x30, 0x33, 0x32, 0x30, 0x30, 0x36, 0x36};
  const std::uint16_t expected_crc = 0x9723;
  const std::uint16_t crc = redsea::crc16_ccitt(test_bytes.data(), 1, test_bytes.size() - 1);
  CHECK(crc == expected_crc);

  const std::uint16_t wrong_crc = redsea::crc16_ccitt(test_bytes.data(), 0, test_bytes.size() - 1);
  CHECK(wrong_crc != expected_crc);
}

TEST_CASE("Round-up division") {
  CHECK(redsea::divideRoundingUp(5, 2) == 3);
  CHECK(redsea::divideRoundingUp(4, 2) == 2);
  CHECK(redsea::divideRoundingUp(3, 2) == 2);
  CHECK(redsea::divideRoundingUp(2, 2) == 1);
  CHECK(redsea::divideRoundingUp(1, 2) == 1);
  CHECK(redsea::divideRoundingUp(0, 2) == 0);
}

TEST_CASE("SimpleMap") {
  SimpleMap<int, std::string> map{
      {1, "one"  },
      {2, "two"  },
      {3, "three"},
  };

  CHECK(map.contains(1));
  CHECK_FALSE(map.contains(4));

  CHECK(map.at(2) == "two");

  REQUIRE_THROWS_AS(map.at(4), std::out_of_range);

  map.insert(4, "four");
  CHECK(map.at(4) == "four");

  map.insert(2, "deux");
  CHECK(map.at(2) == "deux");

  SimpleMap<int, std::pair<int, int>> complex_map;
  complex_map.insert(1, {10, 20});
  CHECK(complex_map.at(1).first == 10);

  complex_map.at(1).second = 5;
  CHECK(complex_map.at(1).second == 5);
}
