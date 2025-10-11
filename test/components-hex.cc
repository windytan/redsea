// Redsea tests: Component tests for hex input
// All different kinds of messages we can receive should go here

#include <cstdio>
#include <sstream>
#include <string>
#include <vector>

#include <unistd.h>

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include "src/io/input.hh"
#include "test_helpers.hh"

TEST_CASE("Basic info") {
  redsea::Options options;

  SECTION("Using Group 0A") {
    // YLE X3M (fi) 2016-09-15
    // clang-format off
    const auto json_lines{hex2json({
      0x6204'0130'966B'594C,
      0x6204'0131'93CD'4520,
      0x6204'0132'E472'5833,
      0x6204'0137'966B'4D20
    }, options, 0x6204)};
    // clang-format on

    REQUIRE(json_lines.size() == 4);

    for (const auto& group : json_lines) {
      CHECK(group["pi"] == "0x6204");
      CHECK(group["group"] == "0A");
      CHECK(json_lines[0]["prog_type"] == "Varied");
      CHECK(group["tp"] == false);
      CHECK(group["ta"] == true);
      CHECK(json_lines[0]["is_music"] == false);
    }

    // https://github.com/windytan/redsea/issues/86
    {
      CHECK(json_lines[0]["di"]["dynamic_pty"] == false);
      CHECK(json_lines[1]["di"]["compressed"] == false);
      CHECK(json_lines[2]["di"]["artificial_head"] == false);
      CHECK(json_lines[3]["di"]["stereo"] == true);
    }

    CHECK(json_lines[3]["ps"] == "YLE X3M ");
  }

  SECTION("Using Group 0B") {
    // Radio Krka (si)
    const auto json_lines{hex2json({0x9423'0800'0000'2020, 0x9423'0801'0000'4B52,
                                    0x9423'0802'0000'4B41, 0x9423'0807'0000'2020},
                                   options, 0x9423)};

    CHECK(json_lines.back()["pi"] == "0x9423");
    CHECK(json_lines.back()["ps"] == "  KRKA  ");
  }

  SECTION("Using Group 15B") {
    // Дорожное 2017-07-03
    // clang-format off
    const auto json_lines{hex2json({
      0x7827'F928'7827'F928
    }, options, 0x7827)};
    // clang-format on

    CHECK(json_lines[0]["group"] == "15B");
    CHECK(json_lines[0]["prog_type"] == "Varied");
    CHECK(json_lines[0]["tp"] == false);
  }

  SECTION("Using Group 15B (Block 2 lost)") {
    // Дорожное 2017-07-03
    // clang-format off
    const auto json_lines{hex2json({
      0x7827'F928'7827'F928
    }, options, 0x7827, DeleteOneBlock::Block2)};
    // clang-format on

    CHECK(json_lines[0]["group"] == "15B");
    CHECK(json_lines[0]["prog_type"] == "Varied");
    CHECK(json_lines[0]["tp"] == false);
  }
}

TEST_CASE("PTY name") {
  redsea::Options options;
  // walczakp/rds-spy-logs/Poland/3ABC - 2019-05-04 22-36-23.spy
  // clang-format off
  const auto json_lines{hex2json({
    0x3ABC'A750'4352'492E,
    0x3ABC'A751'434E'0D0D
  }, options, 0x3ABC)};
  // clang-format on

  CHECK(json_lines.size() == 2);
  CHECK(json_lines.at(1)["pty_name"] == "CRI.CN ");
}

TEST_CASE("PIN & SLC (Group 1)") {
  redsea::Options options;

  SECTION("PIN, SLC variants 0 & 3") {
    // YLE Yksi (fi) 2016-09-15
    // NOTE: PIN has disappeared from the RDS standard in 2021. Nowadays these bits are RFU.
    // clang-format off
    const auto json_lines{hex2json({
      0x6201'10E0'00E1'7C54,
      0x6201'10E0'3027'7C54
    }, options, 0x6201)};
    // clang-format on

    CHECK(json_lines.size() == 2);
    CHECK(json_lines.at(0)["prog_item_number"] == 31828);
    CHECK(json_lines.at(0)["prog_item_started"]["day"] == 15);
    CHECK(json_lines.at(0)["prog_item_started"]["time"] == "17:20");
    CHECK(json_lines.at(0)["country"] == "fi");
    CHECK(json_lines.at(1)["language"] == "Finnish");
  }

  SECTION("SLC variant 6") {
    // RTL 102.5 (it) 2019-05-04
    // walczakp/rds-spy-logs/Italy/5218 - 2019-05-04 22-24-42.spy
    const auto json_lines{hex2json({0x5218'1520'6DAB'0000}, options, 0X5218)};

    CHECK(json_lines.size() == 1);
    CHECK(json_lines.at(0)["slc_broadcaster_bits"] == "0x5AB");
  }
}

TEST_CASE("Callsign") {
  redsea::Options options;

  SECTION("RBDS station (USA)") {
    options.rbds = true;

    // 98.5 KFOX (KUFX) (us) 2020-08-19
    // walczakp/rds-spy-logs/USA/4569 - 2020-08-19 20-45-06.spy
    const auto json_lines{hex2json({0x4569'00C8'CDCD'416E}, options, 0x4569)};

    CHECK(json_lines.back()["callsign"] == "KUFX");
  }

  SECTION("RBDS station + TMC: Uncertain callsign") {
    options.rbds = true;

    // walczakp/rds-spy-logs/USA/16C6 - 2019-05-04 21-43-25.spy
    const auto json_lines{hex2json({0x16C6'00EA'E0CD'6F77}, options, 0x16C6)};

    CHECK(json_lines.back()["callsign_uncertain"] == "KCOS");
  }

  SECTION("RBDS station (Canada)") {
    options.rbds = true;

    // CBC Radio 2 (ca)
    // walczakp/rds-spy-logs/Canada/B203 - 2019-05-05 09-33-12.spy
    const auto json_lines{hex2json({0xB203'21C1'5553'4943}, options, 0xB203)};

    CHECK(json_lines.back()["callsign"] == "CBC English - Radio Two");
  }

  SECTION("No callsign for non-RBDS station") {
    options.rbds = false;

    const auto json_lines{hex2json({0x4569'00C8'CDCD'416E}, options, 0x4569)};

    CHECK_FALSE(json_lines.back().contains("callsign"));
  }
}

// The concept of string length in RadioText is only important for a logging receiver like redsea.
// Stations have chosen various methods to communicate this length that we have outlined here:
// https://github.com/windytan/redsea/wiki/Some-RadioText-research
TEST_CASE("Radiotext") {
  redsea::Options options;

  SECTION("String length method A: Terminated using 0x0D") {
    options.rbds = true;

    // JACK 96.9 (ca) 2019-05-05
    // clang-format off
    const auto json_lines{hex2json({
      0xC954'24F0'4A41'434B,  // "JACK"
      0xC954'24F1'2039'362E,  // " 96."
      0xC954'24F2'390D'0000   // "9\r  "
    }, options, 0xC954)};
    // clang-format on

    REQUIRE(json_lines.size() == 3);
    CHECK(json_lines.back()["radiotext"] == "JACK 96.9");

    // Other lines shouldn't have RadioText
    for (auto prev_line = std::begin(json_lines); prev_line <= std::prev(json_lines.end(), 2);
         prev_line++)
      REQUIRE_FALSE(prev_line->contains("radiotext"));
  }

  SECTION("String length method B: Padded to 64 characters") {
    // Radio Grün-Weiß (at) 2021-07-18
    // clang-format off
    const auto json_lines{hex2json({
      0xA959'2410'4641'4E43,  // "FANC"
      0xA959'2411'5920'2D20,  // "Y - "
      0xA959'2412'426F'6C65,  // "Bole"
      0xA959'2413'726F'2020,  // "ro  "
      0xA959'2414'2020'2020,  // "    "
      0xA959'2415'2020'2020,  // ...
      0xA959'2416'2020'2020, 0xA959'2417'2020'2020,
      0xA959'2418'2020'2020, 0xA959'2419'2020'2020,
      0xA959'241A'2020'2020, 0xA959'241B'2020'2020,
      0xA959'241C'2020'2020, 0xA959'241D'2020'2020,
      0xA959'241E'2020'2020, 0xA959'241F'2020'2020
    }, options, 0xA959)};
    // clang-format on

    REQUIRE(json_lines.size() == 16);
    CHECK(json_lines.back()["radiotext"] == "FANCY - Bolero");

    // Other lines shouldn't have RadioText
    for (auto prev_line = std::begin(json_lines); prev_line < std::prev(json_lines.end(), 1);
         prev_line++)
      REQUIRE_FALSE(prev_line->contains("radiotext"));
  }

  SECTION("Short string with Method B") {
    options.rbds = true;

    // AMP (ca) 2019-05-03
    // walczakp/rds-spy-logs/Canada/CD59 - 2019-05-03 23-56-06.spy
    const auto json_lines{hex2json(
        {0xCD59'2120'414D'5020, 0xCD59'2121'2020'2020, 0xCD59'2122'2020'2020, 0xCD59'2123'2020'2020,
         0xCD59'2124'2020'2020, 0xCD59'2125'2020'2020, 0xCD59'2126'2020'2020, 0xCD59'2127'2020'2020,
         0xCD59'2128'2020'2020, 0xCD59'2129'2020'2020, 0xCD59'212A'2020'2020, 0xCD59'212B'2020'2020,
         0xCD59'212C'2020'2020, 0xCD59'212D'2020'2020, 0xCD59'212E'2020'2020,
         0xCD59'212F'2020'2020},
        options, 0xCD59)};

    REQUIRE(json_lines.size() == 16);
    CHECK(json_lines.back()["radiotext"] == "AMP");

    // Other lines shouldn't have RadioText
    for (auto prev_line = std::begin(json_lines); prev_line < std::prev(json_lines.end(), 1);
         prev_line++)
      REQUIRE_FALSE(prev_line->contains("radiotext"));
  }

  SECTION("String length method B using Group 2B") {
    // Radio Krka (si)
    const auto json_lines{hex2json(
        {0x9423'2800'0000'5052, 0x9423'2801'0000'494A, 0x9423'2802'0000'4554, 0x9423'2803'0000'4E4F,
         0x9423'2804'0000'2050, 0x9423'2805'0000'4F53, 0x9423'2806'0000'4C55, 0x9423'2807'0000'5341,
         0x9423'2808'0000'4E4A, 0x9423'2809'0000'4520, 0x9423'280A'0000'5241, 0x9423'280B'0000'4449,
         0x9423'280C'0000'4120, 0x9423'280D'0000'4B52, 0x9423'280E'0000'4B41,
         0x9423'280F'0000'2020},
        options, 0x9423)};

    REQUIRE(json_lines.size() == 16);
    CHECK(json_lines.back()["radiotext"] == "PRIJETNO POSLUSANJE RADIA KRKA");

    // Other lines shouldn't have RadioText
    for (auto prev_line = std::begin(json_lines); prev_line < std::prev(json_lines.end(), 1);
         prev_line++)
      REQUIRE_FALSE(prev_line->contains("radiotext"));
  }

  SECTION("String length method C: Random-length string with no terminator") {
    // Antenne Kärnten (at) 2021-07-26
    // clang-format off
    const auto json_lines{hex2json({
      0xA540'2540'526F'6262,  // "Robb"  // REPEAT 1
      0xA540'2541'6965'2057,  // "ie W"
      0xA540'2542'696C'6C69,  // "illi"
      0xA540'2543'616D'7320,  // "ams "
      0xA540'2544'2D20'4665,  // "- Fe"
      0xA540'2545'656C'2020,  // "el  "
      0xA540'2540'526F'6262,             // REPEAT 2
      0xA540'2541'6965'2057,
      0xA540'2542'696C'6C69,
      0xA540'2543'616D'7320,
      0xA540'2544'2D20'4665,
      0xA540'2545'656C'2020,
      0xA540'2540'526F'6262,             // REPEAT 3 starts - length confirmed
    }, options, 0xA540)};
    // clang-format on

    REQUIRE(json_lines.size() == 13);
    CHECK(json_lines.back()["radiotext"] == "Robbie Williams - Feel");

    // Other lines shouldn't have RadioText
    for (auto prev_line = std::begin(json_lines); prev_line < std::prev(json_lines.end(), 1);
         prev_line++)
      REQUIRE_FALSE(prev_line->contains("radiotext"));
  }

  SECTION("Non-ASCII character from 'basic character set'") {
    // YLE Vega (fi) 2016-09-15
    // clang-format off
    const auto json_lines{hex2json({
      0x6205'2440'5665'6761,  // "Vega"
      0x6205'2441'204B'7691,  // " kvä"
      0x6205'2442'6C6C'2020,  // "ll  "
      0x6205'2443'2020'2020,  // "    "
      0x6205'2444'2020'2020,  // ...
      0x6205'2445'2020'2020, 0x6205'2446'2020'2020,
      0x6205'2447'2020'2020, 0x6205'2448'2020'2020,
      0x6205'2449'2020'2020, 0x6205'244A'2020'2020,
      0x6205'244B'2020'2020, 0x6205'244C'2020'2020,
      0x6205'244D'2020'2020, 0x6205'244E'2020'2020,
      0x6205'244F'2020'2020
    }, options, 0x6205)};
    // clang-format on

    REQUIRE(json_lines.size() == 16);
    CHECK(json_lines.back()["radiotext"] == "Vega Kväll");
  }

  SECTION("Partial") {
    options.show_partial = true;

    // Antenne Kärnten (at) 2021-07-26
    // clang-format off
    const auto json_lines{hex2json({
      0xA540'2540'526F'6262,  // "Robb"
      0xA540'2541'6965'2057,  // "ie W"
      0xA540'2542'696C'6C69,  // "illi"
      0xA540'2543'616D'7320,  // "ams "
      0xA540'2544'2D20'4665   // "- Fe"
    }, options, 0xA540)};
    // clang-format on

    REQUIRE(json_lines.size() == 5);
    REQUIRE(json_lines.back().contains("partial_radiotext"));
    CHECK(json_lines.back()["partial_radiotext"] ==
          "Robbie Williams - Fe"
          "                                            ");
    CHECK(json_lines.back()["rt_ab"] == "A");

    // All lines should have RadioText
    for (auto line : json_lines) REQUIRE(line.contains("partial_radiotext"));
  }
}

// TODO Below test do NOT pass
TEST_CASE("Radiotext failing corner cases", "[!mayfail]") {
  redsea::Options options;

  SECTION("TODO: String length method A received out-of-order") {
    options.rbds = true;

    // JACK 96.9 (ca) 2019-05-05
    // clang-format off
    const auto json_lines{hex2json({
      0xC954'24F2'390D'0000,   // "9\r  "
      0xC954'24F0'4A41'434B,  // "JACK"
      0xC954'24F1'2039'362E  // " 96."
    }, options, 0xC954)};
    // clang-format on

    REQUIRE(json_lines.size() == 3);
    REQUIRE(json_lines.back().contains("radiotext"));
    CHECK(json_lines.back()["radiotext"] == "JACK 96.9");

    // Other lines shouldn't have RadioText
    for (auto prev_line = std::begin(json_lines); prev_line != std::prev(json_lines.end(), 2);
         prev_line++)
      REQUIRE_FALSE(prev_line->contains("radiotext"));
  }

  // https://github.com/windytan/redsea/issues/118
  SECTION("TODO: String length hybrid method A+B: Terminated *and* padded") {
    // Radio Austria (at) 2024
    const auto json_lines{hex2json(
        {0xA3E0'2550'5375'7065, 0xA3E0'2551'7273'7461, 0xA3E0'2552'7273'2026, 0xA3E0'2553'2053'7570,
         0xA3E0'2554'6572'6869, 0xA3E0'2555'7473'0D20, 0xA3E0'2556'2020'2020, 0xA3E0'2557'2020'2020,
         0xA3E0'2558'2020'2020, 0xA3E0'2559'2020'2020, 0xA3E0'255A'2020'2020, 0xA3E0'255B'2020'2020,
         0xA3E0'255C'2020'2020, 0xA3E0'255D'2020'2020, 0xA3E0'255E'2020'2020,
         0xA3E0'255F'2020'2020},
        options, 0xA3E0)};

    REQUIRE(json_lines.size() == 16);
    REQUIRE(json_lines.back().contains("radiotext"));
    CHECK(json_lines.back()["radiotext"] == "Superstars & Superhits");

    // Other lines shouldn't have RadioText
    for (auto prev_line = std::begin(json_lines); prev_line < std::prev(json_lines.end(), 1);
         prev_line++)
      REQUIRE_FALSE(prev_line->contains("radiotext"));
  }
}

TEST_CASE("RDS2 Enhanced RadioText") {
  redsea::Options options;

  // Järviradio (fi)
  // clang-format off
  const auto json_lines{hex2json({
    // eRT ODA identifier
    0x6255'3538'0001'6552,
    // Text data
    0x6255'C520'4AC3'A472,
    0x6255'C521'7669'7261,
    0x6255'C522'6469'6F20,
    0x6255'C523'5244'5332,
    0x6255'C524'2045'5254,
    0x6255'C525'0D0D'0D0D,
  }, options, 0x6255)};
  // clang-format on

  CHECK(json_lines.back()["enhanced_radiotext"] == "Järviradio RDS2 ERT");
}

TEST_CASE("Enhanced RadioText: invalid multibyte chars") {
  redsea::Options options;

  // Järviradio (fi)
  // clang-format off
  CHECK_NOTHROW(hex2json({
    // eRT ODA identifier (with mistake)
    0x6255'3538'000A'6552,
    // Text data
    0x6255'C520'4AC3'A472,
    0x6255'C521'7669'7261,
    0x6255'C522'6469'6F20,
    0x6255'C523'5244'5332,
    0x6255'C524'2045'5254,
    0x6255'C525'0D0D'0D0D,
  }, options, 0x6255));
  // clang-format on
}

TEST_CASE("RadioText Plus") {
  redsea::Options options;

  // Some encoders forget that RT+ length field means _additional_ length, so we need to rtrim
  SECTION("Off-by-one encoder bug workaround") {
    // clang-format off
    const auto json_lines{hex2json({
      // RT+ ODA identifier
      0x53C5'3558'0000'4BD7,
      // RT+
      0x53C5'C548'8020'0A6A,
      // RT message
      0x53C5'2550'4649'4F52,
      0x53C5'2551'454C'4C41,
      0x53C5'2552'204D'414E,
      0x53C5'2553'4E4F'4941,
      0x53C5'2554'202D'2047,
      0x53C5'2555'4C49'2041,
      0x53C5'2556'4D41'4E54,
      0x53C5'2557'4920'2020,
      0x53C5'2558'2020'2020, 0x53C5'2559'2020'2020, 0x53C5'255A'2020'2020,
      0x53C5'255B'2020'2020, 0x53C5'255C'2020'2020, 0x53C5'255D'2020'2020,
      0x53C5'255E'2020'2020, 0x53C5'255F'2020'2020,
      // RT+ (second one)
      0x53C5'C548'8020'0A6A,
    }, options, 0x53C5)};
    // clang-format on

    REQUIRE(!json_lines.empty());
    std::printf("%s\n", json_lines.back().dump().c_str());
    REQUIRE(json_lines.back().contains("radiotext_plus"));
    REQUIRE(json_lines.back()["radiotext_plus"]["tags"].size() == 2);
    CHECK(json_lines.back()["radiotext_plus"]["tags"][0]["content-type"] == "item.artist");
    CHECK(json_lines.back()["radiotext_plus"]["tags"][0]["data"] == "FIORELLA MANNOIA");
    CHECK(json_lines.back()["radiotext_plus"]["tags"][1]["content-type"] == "item.title");
    CHECK(json_lines.back()["radiotext_plus"]["tags"][1]["data"] == "GLI AMANTI");
  }

  // Should count the number of letters and not UTF8-converted bytes
  SECTION("Containing non-ASCII characters") {
    // Antenne 2016-09-17
    // clang-format off
    const auto json_lines{hex2json({
      // RT+ ODA identifier
      0xD318'3558'0000'4BD7,
      // RT+ (we need two of these to confirm)
      0xD318'C558'8D20'0DCF,
      // RT message
      0xD318'2540'6A65'747A, 0xD318'2541'7420'6175,
      0xD318'2542'6620'414E, 0xD318'2543'5445'4E4E,
      0xD318'2544'4520'4241, 0xD318'2545'5945'524E,
      0xD318'2546'3A20'4368, 0xD318'2547'7269'7374,
      0xD318'2548'696E'6120, 0xD318'2549'5374'9972,
      0xD318'254A'6D65'7220, 0xD318'254B'2D20'4569,
      0xD318'254C'6E20'5465, 0xD318'254D'696C'2076,
      0xD318'254E'6F6E'206D, 0xD318'254F'6972'2020,
      // RT+ (second one)
      0xD318'C558'8D20'0DCF
    }, options, 0xD318)};
    // clang-format on

    REQUIRE(json_lines.back()["radiotext_plus"]["tags"].size() == 2);
    CHECK(json_lines.back()["radiotext_plus"]["tags"][0]["content-type"] == "item.artist");
    CHECK(json_lines.back()["radiotext_plus"]["tags"][0]["data"] == "Christina Stürmer");
    CHECK(json_lines.back()["radiotext_plus"]["tags"][1]["content-type"] == "item.title");
    CHECK(json_lines.back()["radiotext_plus"]["tags"][1]["data"] == "Ein Teil von mir");
  }
}

TEST_CASE("RDS2 Long PS") {
  redsea::Options options;

  SECTION("Space-padded") {
    // The Breeze Gold Coast 100.6 (au) 2024-05-17
    // clang-format off
    const auto json_lines{hex2json({
      0x49B1'F180'4272'6565,
      0x49B1'F181'7A65'2031,
      0x49B1'F182'3030'2E36,
      0x49B1'F183'2047'6F6C,
      0x49B1'F184'6420'436F,
      0x49B1'F185'6173'7400,
      0x49B1'F186'0000'0000,
      0x49B1'F187'0000'0000
    }, options, 0x49B1)};
    // clang-format on

    REQUIRE(json_lines.back().contains("long_ps"));
    CHECK(json_lines.back()["long_ps"] == "Breeze 100.6 Gold Coast");
  }

  SECTION("String-terminated, contains non-ASCII UTF-8 character") {
    // Järviradio (fi)
    // clang-format off
    const auto json_lines{hex2json({
      0x6255'F520'4AC3'A452,
      0x6255'F521'5649'5241,
      0x6255'F522'4449'4F0D
    }, options, 0x6255)};
    // clang-format on

    REQUIRE(json_lines.back().contains("long_ps"));
    CHECK(json_lines.back()["long_ps"] == "JäRVIRADIO");  // sic
  }
}

TEST_CASE("Alternative frequencies") {
  redsea::Options options;

  SECTION("Method A") {
    // YLE Yksi (fi) 2016-09-15
    // clang-format off
    const auto json_lines{hex2json({
      0x6201'00F7'E704'5349,
      0x6201'00F0'2217'594C,
      0x6201'00F1'1139'4520,
      0x6201'00F2'0A14'594B
    }, options, 0x6201)};
    // clang-format on

    REQUIRE(json_lines.size() == 4);
    REQUIRE(json_lines.back().contains("alt_frequencies_a"));
    CHECK(listEquals(json_lines.back()["alt_frequencies_a"],
                     {87'900, 90'900, 89'800, 89'200, 93'200, 88'500, 89'500}));
  }

  SECTION("Method B") {
    // YLE Helsinki (fi) 2016-09-15
    // clang-format off
    const auto json_lines{hex2json({
      0x6403'0447'F741'4920, 0x6403'0440'415F'594C, 0x6403'0441'4441'4520, 0x6403'0442'5541'484B,
      0x6403'0447'1C41'4920, 0x6403'0440'6841'594C, 0x6403'0441'5E41'4520, 0x6403'0442'414B'484B,
      0x6403'0447'4156'4920, 0x6403'0440'CB41'594C, 0x6403'0441'B741'4520, 0x6403'0442'4174'484B
    }, options, 0x6403)};
    // clang-format on

    REQUIRE(json_lines.size() == 12);
    REQUIRE(json_lines.back().contains("alt_frequencies_b"));

    // https://web.archive.org/web/20160622055936/http://yle.fi/uutiset/taajuudet/6009222
    CHECK(json_lines.back()["alt_frequencies_b"]["tuned_frequency"] == 94'000);
    CHECK(listEquals(json_lines.back()["alt_frequencies_b"]["same_programme"],
                     {97'000, 90'300, 95'000, 96'100, 99'100}));
    CHECK(listEquals(json_lines.back()["alt_frequencies_b"]["regional_variants"],
                     {94'300, 96'000, 97'900, 96'900, 107'800, 105'800}));
  }

  SECTION("Method B with --show-partial") {
    options.show_partial = true;

    // YLE Helsinki (fi) 2016-09-15
    // clang-format off
    const auto json_lines{hex2json({
      0x6403'0447'F741'4920, 0x6403'0440'415F'594C, 0x6403'0441'4441'4520, 0x6403'0442'5541'484B
    }, options, 0x6403)};
    // clang-format on

    REQUIRE(json_lines.size() == 4);
    REQUIRE(json_lines[0].contains("partial_alt_frequencies"));
    CHECK(listEquals(json_lines[0]["partial_alt_frequencies"], {94'000}));
    REQUIRE(json_lines[1].contains("partial_alt_frequencies"));
  }
}

TEST_CASE("Clock-time and date") {
  redsea::Options options;

  SECTION("During DST") {
    // BR-KLASSIK (de) 2017-04-04
    const auto json_lines{hex2json({0xD314'41C1'C3EF'5AC4}, options, 0xD314)};

    REQUIRE(json_lines.size() == 1);
    REQUIRE(json_lines.back().contains("clock_time"));
    CHECK(json_lines.back()["clock_time"] == "2017-04-04T23:43:00+02:00");
  }

  SECTION("Outside of DST") {
    // 104.6RTL (de) 2018-11-01
    // walczakp/rds-spy-logs/Germany/D42A - 2018-11-01 14-17-16 DE BER RTL104_6.rds
    const auto json_lines{hex2json({0xD42A'4541'C86E'D482}, options, 0xD42A)};

    REQUIRE(json_lines.size() == 1);
    REQUIRE(json_lines.back().contains("clock_time"));
    CHECK(json_lines.back()["clock_time"] == "2018-11-01T14:18:00+01:00");
  }

  SECTION("Negative UTC offset") {
    options.rbds = true;

    // 98.5 KFOX (KUFX) (us) 2020-08-19
    // walczakp/rds-spy-logs/USA/4569 - 2020-08-19 20-45-06.spy
    const auto json_lines{hex2json({0x4569'40DD'CD92'3BAE}, options, 0x4569)};

    REQUIRE(json_lines.size() == 1);
    REQUIRE(json_lines.back().contains("clock_time"));
    CHECK(json_lines.back()["clock_time"] == "2020-08-19T20:46:00-07:00");
  }

  SECTION("Zero UTC offset") {
    // Vikerraadio (ee) 2016-07-18 (though ee is not actually UTC+0)
    const auto json_lines{hex2json({0x22E1'4581'C1E7'4280}, options, 0x22E1)};

    REQUIRE(json_lines.size() == 1);
    REQUIRE(json_lines.back().contains("clock_time"));
    CHECK(json_lines.back()["clock_time"] == "2016-07-18T20:10:00Z");
  }

  SECTION("Across local midnight") {
    // https://github.com/windytan/redsea/issues/83
    // clang-format off
    const auto json_lines{hex2json({
      0xF201'441D'D299'5EC4,
      0xF201'441D'D299'6004
    }, options, 0xF201)};
    // clang-format on

    REQUIRE(json_lines.size() == 2);
    REQUIRE(json_lines[0].contains("clock_time"));
    REQUIRE(json_lines[1].contains("clock_time"));
    CHECK(json_lines[0]["clock_time"] == "2022-05-25T23:59:00+02:00");
    CHECK(json_lines[1]["clock_time"] == "2022-05-26T00:00:00+02:00");
  }

  SECTION("Across UTC midnight") {
    // https://github.com/windytan/redsea/issues/83
    // clang-format off
    const auto json_lines{hex2json({
      0xF201'441D'D299'7EC4,
      0xF201'441D'D29A'0004
    }, options, 0xF201)};
    // clang-format on

    REQUIRE(json_lines.size() == 2);
    REQUIRE(json_lines[0].contains("clock_time"));
    REQUIRE(json_lines[1].contains("clock_time"));
    CHECK(json_lines[0]["clock_time"] == "2022-05-26T01:59:00+02:00");
    CHECK(json_lines[1]["clock_time"] == "2022-05-26T02:00:00+02:00");
  }

  SECTION("Invalid MJD is handled cleanly") {
    // Integer underflow fixed in 1.0.0
    const auto json_lines{hex2json(
        {
            0xD314'41C0'7530'5AC4  // MJD = 15000
        },
        options, 0xD314)};

    REQUIRE(json_lines.size() == 1);
    CHECK_FALSE(json_lines.back().contains("clock_time"));
  }
}

// TDC is a rarely seen feature. The TRDS4001 encoder is known to fill the fields
// with its version string and some unknown binary data. We can at least test that this
// version string is found somewhere in the data.
TEST_CASE("Transparent data channels") {
  redsea::Options options;

  // Radio 10 (nl) 2019-05-04
  // walczakp/rds-spy-logs/Netherlands/83D2 - 2019-05-04 23-00-53.spy
  const auto json_lines{hex2json(
      {0x83D2'5540'00C8'006D, 0x83D2'5541'FF00'0000, 0x83D2'5542'00E2'00E3, 0x83D2'5543'00C8'00E0,
       0x83D2'5544'00DE'00D8, 0x83D2'5545'00DF'00E4, 0x83D2'5546'5452'4453, 0x83D2'5547'3430'3031,
       0x83D2'5548'2052'656C, 0x83D2'5549'6561'7365, 0x83D2'554A'2030'3230, 0x83D2'554B'3130'3930,
       0x83D2'554C'3020'3136, 0x83D2'554D'2F30'362F, 0x83D2'554E'3230'3033, 0x83D2'554F'202D'2052,
       0x83D2'5550'5652'2045, 0x83D2'5551'6C65'7474, 0x83D2'5552'726F'6E69, 0x83D2'5553'6361'2053,
       0x83D2'5554'7061'0037, 0x83D2'5555'0020'2037, 0x83D2'5556'0020'2037, 0x83D2'5557'0020'2020,
       0x83D2'5558'2020'2020, 0x83D2'5559'2020'2020, 0x83D2'555A'2020'2020, 0x83D2'555B'2020'2020,
       0x83D2'555C'2020'2020, 0x83D2'555D'2020'2020, 0x83D2'555E'2020'2020, 0x83D2'555F'2053'20AC},
      options, 0x83D2)};

  REQUIRE(json_lines.size() == 32);
  CHECK(json_lines.back()["transparent_data"].contains("full_text"));

  const auto full_text =
      json_lines.back()["transparent_data"]["full_text"].template get<std::string>();
  CHECK(full_text.find("TRDS4001 Release 02010900 16/06/2003 - RVR Elettronica") !=
        std::string::npos);
}

// In-house applications are just that, only defined in-house. Currently we print them
// as integers.
TEST_CASE("In-house applications") {
  redsea::Options options;

  // BR-KLASSIK (de) 2017-04-04
  // clang-format off
  const auto json_lines{hex2json({
    0xD314'61C0'AFFE'AFFE,
    0xD314'61C1'D100'0A19,
    0xD314'61C2'0000'0B01,
    0xD314'61C3'2005'2015,
    0xD314'61DF'0000'D314},
  options, 0xD314)};
  // clang-format on

  REQUIRE(json_lines.size() == 5);
  CHECK(listEquals(json_lines.at(0)["in_house_data"], {0x00, 0xAFFE, 0xAFFE}));
  CHECK(listEquals(json_lines.at(1)["in_house_data"], {0x01, 0xD100, 0x0A19}));
  CHECK(listEquals(json_lines.at(2)["in_house_data"], {0x02, 0x0000, 0x0B01}));
  CHECK(listEquals(json_lines.at(3)["in_house_data"], {0x03, 0x2005, 0x2015}));
  CHECK(listEquals(json_lines.at(4)["in_house_data"], {0x1F, 0x0000, 0xD314}));
}

TEST_CASE("EON") {
  redsea::Options options;

  SECTION("Using 14A groups") {
    // YLE X (fi) 2016-09-15
    // clang-format off
    const auto json_lines{hex2json({
      0x6202'E150'594C'6203,
      0x6202'E151'4553'6203,
      0x6202'E152'554F'6203,
      0x6202'E153'4D49'6203,
      0x6202'E155'2C41'6203,
      0x6202'E15C'0000'6203,
      0x6202'E15D'4800'6203,
      0x6202'E15E'7C83'6203},
    options, 0x6202)};
    // clang-format on

    REQUIRE(json_lines.size() == 8);
    CHECK(json_lines.at(3)["pi"] == "0x6202");

    // Refers to YLE Suomi 94.0 MHz
    CHECK(json_lines.at(3)["other_network"]["pi"] == "0x6203");
    CHECK(json_lines.at(3)["other_network"]["ps"] == "YLESUOMI");
    CHECK(json_lines.at(4)["other_network"]["kilohertz"] == 94'000);
    CHECK(json_lines.at(5)["other_network"]["has_linkage"] == false);
    CHECK(json_lines.at(5)["other_network"]["tp"] == true);
    CHECK(json_lines.at(6)["other_network"]["prog_type"] == "Varied");
    CHECK(json_lines.at(6)["other_network"]["ta"] == false);
    CHECK(json_lines.at(7)["other_network"]["prog_item_number"] == 31875);
    CHECK(json_lines.at(7)["other_network"]["prog_item_started"]["day"] == 15);
    CHECK(json_lines.at(7)["other_network"]["prog_item_started"]["time"] == "18:03");
  }

  SECTION("Using 14B groups") {
    // Deutschlandfunk Kultur (de) 2016-12-25
    const auto json_lines{hex2json({0xD220'EA90'D220'D313}, options, 0xD220)};

    CHECK(json_lines.back()["pi"] == "0xD220");

    // Refers to Bayern 3
    CHECK(json_lines.back()["other_network"]["pi"] == "0xD313");
    CHECK(json_lines.back()["other_network"]["tp"] == true);
    CHECK(json_lines.back()["other_network"]["ta"] == false);
  }

  SECTION("Alt frequencies") {
    // Radio Gioconda (it)
    const auto json_lines{
        hex2json({0x53C5'E554'E2AD'53C6, 0x53C5'E554'C2CD'53C6}, options, 0x53C5)};

    REQUIRE(json_lines.size() == 2);
    CHECK(listEquals(json_lines.back()["other_network"]["alt_frequencies"], {104'800, 106'900}));
  }
}

TEST_CASE("DAB cross-referencing") {
  redsea::Options options;
  // BBC Radio 4 (gb) 2015-09-27
  // walczakp/rds-spy-logs/UK/C204 - 2015-09-27 23-35-46 UK NRW BBC4.rds
  const auto json_lines{hex2json({0xC204'3138'0000'0093, 0xC204'C124'3717'CE15}, options, 0xC204)};

  REQUIRE(json_lines.size() == 2);

  // Source: https://www.bbc.co.uk/programmes/articles/98FthRzhxJ4z0fXYJnsvlM/about-radio-4
  CHECK(json_lines.back()["dab"]["channel"] == "12B");
  CHECK(json_lines.back()["dab"]["kilohertz"] == 225'648);
}

TEST_CASE("Unspecified ODA") {
  redsea::Options options;

  // WDR 5 (de) 2019-05-05
  // walczakp/rds-spy-logs/Germany/D395 - 2019-05-05 09-46-23.spy
  const auto json_lines{hex2json({0xD395'B065'279A'0020}, options, 0xD395)};

  REQUIRE(json_lines.size() == 1);
  CHECK(json_lines.at(0)["group"] == "11A");
  REQUIRE(json_lines.at(0).contains("unknown_oda"));
  CHECK(json_lines.at(0)["unknown_oda"]["raw_data"] == "05 279A 0020");
}

TEST_CASE("Block error rate (BLER) reporting") {
  redsea::Options options;

  SECTION("Disabled") {
    options.bler = false;
    // clang-format off
    const auto json_lines{hex2json({
      0x7827'F928'7827'F928
    }, options, 0x7827, DeleteOneBlock::Block2)};
    // clang-format on

    REQUIRE(!json_lines.empty());
    REQUIRE(!json_lines.back().contains("bler"));
  }

  SECTION("Enabled") {
    options.bler = true;

    constexpr int num_erroneous_blocks = 1;

    // clang-format off
    const auto json_lines{hex2json({
      0x7827'F928'7827'F928
    }, options, 0x7827, DeleteOneBlock::Block2)};
    // clang-format on

    REQUIRE(!json_lines.empty());
    REQUIRE(json_lines.back().contains("bler"));
    // 1 block out of kNumBlerAverageGroups was missing
    CHECK(json_lines.back()["bler"] ==
          num_erroneous_blocks * 100 / (4 * redsea::kNumBlerAverageGroups));
  }
}

TEST_CASE("Invalid data") {
  redsea::Options options;

  SECTION("Invalid UTF-8 is handled cleanly") {
    // clang-format off
    REQUIRE_NOTHROW(hex2json({
      0xE24D'4401'D02F'1942,
      0xE24D'E400'E24D'0000,
      0xE24D'F400'E20D'FC20
    }, options, 0xE24D));
    // clang-format on
  }
}

TEST_CASE("Rx time for hex input") {
  redsea::Options options;

  SECTION("Disabled") {
    options.timestamp = false;

    // clang-format off
    const auto json_lines{hex2json({
      0x7827'F928'7827'F928
    }, options, 0x7827)};
    // clang-format on

    REQUIRE(!json_lines.empty());
    REQUIRE(!json_lines.back().contains("rx_time"));
  }

  SECTION("Enabled") {
    options.timestamp = true;

    // clang-format off
    const auto json_lines{hex2json({
      0x7827'F928'7827'F928
    }, options, 0x7827)};
    // clang-format on

    REQUIRE(!json_lines.empty());
    REQUIRE(json_lines.back().contains("rx_time"));
    // There's not much else we can confidently test here without mocking system_clock
  }
}

TEST_CASE("ASCII hex input format") {
  redsea::Options options;
  std::stringstream ss;

  // This format should be compatible with the .spy format used by RDS Spy.
  // But note that redsea does not decode the timestamp information.
  // The specification can be found on page 23 at:
  // https://rdsspy.com/download/mainapp/rdsspy.pdf

  SECTION("Simple") {
    ss << "7827 F928 7827 F928\n" << "6255 3538 0001 6552\n";

    const auto group1 = redsea::readHexGroup(options, ss);
    const auto group2 = redsea::readHexGroup(options, ss);

    CHECK(group1.asHex() == "7827 F928 7827 F928");
    CHECK(group2.asHex() == "6255 3538 0001 6552");
  }

  SECTION("No spaces") {
    ss << "7827F9287827F928\n" << "6255353800016552\n";

    const auto group1 = redsea::readHexGroup(options, ss);
    const auto group2 = redsea::readHexGroup(options, ss);

    CHECK(group1.asHex() == "7827 F928 7827 F928");
    CHECK(group2.asHex() == "6255 3538 0001 6552");
  }

  SECTION("Title line, comments, mixed case, empty lines, odd spacing") {
    ss << "<hex data>\n7827 f928 78 27 F928 First group\n"
       << "\n"
       << "6255 3538 0001 6552 Second group\n";

    const auto group1 = redsea::readHexGroup(options, ss);
    const auto group2 = redsea::readHexGroup(options, ss);

    CHECK(group1.asHex() == "7827 F928 7827 F928");
    CHECK(group2.asHex() == "6255 3538 0001 6552");
  }

  SECTION("RDS2 data streams") {
    ss << "E24D 4401 D02F 1942\n" << "#S1 E24D E400 E24D 0000\n" << "#S2 E24D F400 E20D FC20\n";

    const auto group1 = redsea::readHexGroup(options, ss);
    const auto group2 = redsea::readHexGroup(options, ss);
    const auto group3 = redsea::readHexGroup(options, ss);

    CHECK(group1.getDataStream() == 0U);
    CHECK(group2.getDataStream() == 1U);
    CHECK(group3.getDataStream() == 2U);
    CHECK(group1.asHex() == "E24D 4401 D02F 1942");
    CHECK(group2.asHex() == "E24D E400 E24D 0000");
    CHECK(group3.asHex() == "E24D F400 E20D FC20");
  }

  SECTION("Missing block") {
    ss << "7827 ---- 7827 F928\n";

    const auto group1 = redsea::readHexGroup(options, ss);

    CHECK(group1.asHex() == "7827 ---- 7827 F928");
  }

  SECTION("Line too short") {
    ss << "6255 3538 000\n";

    const auto group1 = redsea::readHexGroup(options, ss);

    // Lines should be at least 16 characters
    CHECK(group1.isEmpty());
  }

  SECTION("Last blocks cut short") {
    ss << "7827 F928 7827 F92\n";
    ss << "6255    3538 000\n";

    const auto group1 = redsea::readHexGroup(options, ss);
    const auto group2 = redsea::readHexGroup(options, ss);

    // The only option is to throw away the entire block
    CHECK(group1.asHex() == "7827 F928 7827 ----");
    // Spacing makes the input over 16 characters -> still .spy compatible
    CHECK(group2.asHex() == "6255 3538 ---- ----");
  }
}

TEST_CASE("Hex output format") {
  redsea::Options options;
  options.show_raw = true;

  // clang-format off
  const auto json_lines{hex2json({
    0x7827'F928'7827'F928
  }, options, 0x7827, DeleteOneBlock::Block2)};
  // clang-format on

  CHECK(json_lines.back()["raw_data"] == "7827 ---- 7827 F928");
};
