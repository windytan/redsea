// Redsea tests: Unit tests

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <string>
#include <variant>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "../src/rft.hh"
#include "../src/util/base64.hh"
#include "../src/util/csv.hh"
#include "../src/util/tree.hh"
#include "../src/util/util.hh"

TEST_CASE("Bitfield extraction") {
  constexpr std::uint16_t block1{0b0001'0010'0011'0100};
  constexpr std::uint16_t block2{0b0101'0110'0111'1000};

  SECTION("Single block") {
    // clang-format off
    CHECK(redsea::getBits<4>(block1, 0) ==             0b0100);

    CHECK(redsea::getBits<5>(block1, 4) ==      0b0'0011);
    CHECK(redsea::getBits<6>(block1, 4) ==     0b10'0011);
    CHECK(redsea::getBits<8>(block1, 4) ==   0b0010'0011);
    CHECK(redsea::getBits<9>(block1, 4) == 0b1'0010'0011);

    CHECK(redsea::getBits<5>(block1, 5) ==     0b10'001);
    CHECK(redsea::getBits<8>(block1, 5) == 0b1'0010'001);

    CHECK(redsea::getBool(block1, 12)   == true);
    // clang-format on
  }

  SECTION("Concatenation of two blocks") {
    // clang-format off
    CHECK(redsea::getBits<4>(block1, block2, 0)                        == 0b1000);

    CHECK(redsea::getBits<5>(block1, block2, 4)  ==                0b0'0111);
    CHECK(redsea::getBits<6>(block1, block2, 4)  ==               0b10'0111);
    CHECK(redsea::getBits<8>(block1, block2, 4)  ==             0b0110'0111);
    CHECK(redsea::getBits<9>(block1, block2, 4)  ==           0b1'0110'0111);

    CHECK(redsea::getBits<12>(block1, block2, 8) ==   0b0100'0101'0110);
    CHECK(redsea::getBits<12>(block1, block2, 9) == 0b1'0100'0101'011);
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

TEST_CASE("ObjectTree") {
  redsea::ObjectTree tree;

  SECTION("String") {
    CHECK(tree.empty());

    tree["string1"] = "value";
    tree["string2"] = "another value";

    CHECK(!tree.empty());
    CHECK(std::holds_alternative<std::string>(tree["string1"].get()));
    CHECK(std::get<std::string>(tree["string1"].get()) == "value");
    CHECK(std::holds_alternative<std::string>(tree["string2"].get()));
    CHECK(std::get<std::string>(tree["string2"].get()) == "another value");
  }

  SECTION("Integer") {
    CHECK(!tree.contains("number"));

    tree["number"] = 42;

    CHECK(tree.contains("number"));
    CHECK(std::holds_alternative<int>(tree["number"].get()));
    CHECK(std::get<int>(tree["number"].get()) == 42);
  }

  SECTION("Bool") {
    tree["bool"] = true;

    CHECK(std::holds_alternative<bool>(tree["bool"].get()));
    CHECK(std::get<bool>(tree["bool"].get()) == true);
  }

  SECTION("Double") {
    tree["double"] = 3.14;

    CHECK(std::holds_alternative<double>(tree["double"].get()));
    CHECK(std::get<double>(tree["double"].get()) == 3.14);
  }

  SECTION("Array") {
    tree["array"].push_back("first");
    tree["array"].push_back("second");

    CHECK(std::holds_alternative<redsea::ObjectTree::array_t>(tree["array"].get()));
    CHECK(std::get<redsea::ObjectTree::array_t>(tree["array"].get()).size() == 2);

    // Change an already existing array element
    tree["array"][0] = "0";
    const auto str =
        std::get<std::string>(std::get<redsea::ObjectTree::array_t>(tree["array"].get())[0].get());
    CHECK(str == "0");

    // Array resizes automatically
    tree["array"][4] = "4";
    CHECK(std::get<redsea::ObjectTree::array_t>(tree["array"].get()).size() == 5);

    // operator[] creates an array automatically
    tree["newarray"][2] = "2";
    CHECK(std::get<redsea::ObjectTree::array_t>(tree["newarray"].get()).size() == 3);
  }

  SECTION("Subtree") {
    tree["object"]["key1"] = "val1";
    tree["object"]["key2"] = 2;

    CHECK(std::holds_alternative<redsea::ObjectTree::object_t>(tree["object"].get()));
    CHECK(std::get<redsea::ObjectTree::object_t>(tree["object"].get()).size() == 2);
  }
}
