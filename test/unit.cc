#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include "../src/block_sync.h"
#include "../src/channel.h"
#include "../src/common.h"
#include "../src/groups.h"
#include "../src/options.h"
#include "../src/tmc/csv.h"
#include "test_helpers.h"

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

TEST_CASE("Programme Item Number etc.") {
  redsea::Options options;
  // YLE Yksi (fi) 2016-09-15
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

// https://github.com/windytan/redsea/wiki/Some-RadioText-research
TEST_CASE("Radiotext") {
  redsea::Options options;

  SECTION("String length method A: Terminated using 0x0D") {
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
  }

  SECTION("Using Group 2B") {
    // Radio Krka (si)
    const auto json_lines{hex2json(
        {0x9423'2800'0000'5052, 0x9423'2801'0000'494A, 0x9423'2802'0000'4554, 0x9423'2803'0000'4E4F,
         0x9423'2804'0000'2050, 0x9423'2805'0000'4F53, 0x9423'2806'0000'4C55, 0x9423'2807'0000'5341,
         0x9423'2808'0000'4E4A, 0x9423'2809'0000'4520, 0x9423'280A'0000'5241, 0x9423'280B'0000'4449,
         0x9423'280C'0000'4120, 0x9423'280D'0000'4B52, 0x9423'280E'0000'4B41,
         0x9423'280F'0000'2020},
        options, 0x9423)};

    CHECK(json_lines.back()["radiotext"] == "PRIJETNO POSLUSANJE RADIA KRKA");
  }
}

TEST_CASE("Enhanced RadioText") {
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

TEST_CASE("RadioText Plus") {
  redsea::Options options;

  SECTION("Containing non-ascii characters") {
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

TEST_CASE("Long PS") {
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
      0x6403'0447'F741'4920,
      0x6403'0440'415F'594C,
      0x6403'0441'4441'4520,
      0x6403'0442'5541'484B,
      0x6403'0447'1C41'4920,
      0x6403'0440'6841'594C,
      0x6403'0441'5E41'4520,
      0x6403'0442'414B'484B,
      0x6403'0447'4156'4920,
      0x6403'0440'CB41'594C,
      0x6403'0441'B741'4520,
      0x6403'0442'4174'484B
    }, options, 0x6403)};
    // clang-format on

    REQUIRE(json_lines.size() == 12);
    REQUIRE(json_lines.back().contains("alt_frequencies_b"));
    CHECK(json_lines.back()["alt_frequencies_b"]["tuned_frequency"] == 94'000);
    CHECK(listEquals(json_lines.back()["alt_frequencies_b"]["same_programme"],
                     {97'000, 90'300, 95'000, 96'100, 99'100}));
    CHECK(listEquals(json_lines.back()["alt_frequencies_b"]["regional_variants"],
                     {94'300, 96'000, 97'900, 96'900, 107'800, 105'800}));
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

  SECTION("With a negative UTC offset") {
    // 98.5 KFOX (KUFX) (us) 2020-08-19
    // walczakp/rds-spy-logs/USA/4569 - 2020-08-19 20-45-06.spy
    const auto json_lines{hex2json({0x4569'40DD'CD92'3BAE}, options, 0x4569)};

    REQUIRE(json_lines.size() == 1);
    REQUIRE(json_lines.back().contains("clock_time"));
    CHECK(json_lines.back()["clock_time"] == "2020-08-19T20:46:00-07:00");
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

// TDS is a rarely seen feature. The TRDS4001 encoder is known to fill the fields
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
      0x6202'E140'594C'6205,
      0x6202'E141'4520'6205,
      0x6202'E142'5645'6205,
      0x6202'E143'4741'6205,
      0x6202'E145'2C88'6205},
    options, 0x6202)};
    // clang-format on

    REQUIRE(json_lines.size() == 5);
    CHECK(json_lines.at(3)["pi"] == "0x6202");

    // Refers to YLE Vega 101.1 MHz
    CHECK(json_lines.at(3)["other_network"]["pi"] == "0x6205");
    CHECK(json_lines.at(3)["other_network"]["ps"] == "YLE VEGA");
    CHECK(json_lines.at(4)["other_network"]["kilohertz"] == 101100);
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
}

TEST_CASE("DAB cross-referencing") {
  redsea::Options options;
  // BBC Radio 4 (gb) 2015-09-27
  // walczakp/rds-spy-logs/UK/C204 - 2015-09-27 23-35-46 UK NRW BBC4.rds
  const auto json_lines{hex2json({0xC204'3138'0000'0093, 0xC204'C124'3717'CE15}, options, 0xC204)};

  REQUIRE(json_lines.size() == 2);

  // Source: https://www.bbc.co.uk/programmes/articles/98FthRzhxJ4z0fXYJnsvlM/about-radio-4
  CHECK(json_lines.back()["dab"]["channel"] == "12B");
  CHECK(json_lines.back()["dab"]["kilohertz"] == 225648);
}

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
    "00100010111000010111001100"
    "00100101100000111100111110"
    "00100000011001011011010011"
    "01101001001000000110111110"};
  // clang-format on

  SECTION("Detects error-free group") {
    const std::string test_data{correct_group + correct_group};
    const auto groups{asciibin2groups(test_data, options)};

    CHECK(groups.back().getNumErrors() == 0);
  }

  SECTION("Detects long error burst") {
    std::string broken_group{correct_group};
    flipAsciiBit(broken_group, 1);
    flipAsciiBit(broken_group, 2);
    flipAsciiBit(broken_group, 9);
    flipAsciiBit(broken_group, 10);

    const std::string test_data{correct_group + correct_group + broken_group};
    const auto groups{asciibin2groups(test_data, options)};

    CHECK(groups.back().getNumErrors() == 1);
  }

  SECTION("Corrects double bit flip") {
    std::string broken_group{correct_group};
    flipAsciiBit(broken_group, 1);
    flipAsciiBit(broken_group, 2);

    const std::string test_data{correct_group + correct_group + broken_group};
    const auto groups{asciibin2groups(test_data, options)};

    CHECK(groups.back().getNumErrors() == 1);
    CHECK(groups.back().has(redsea::BLOCK1));
    CHECK(groups.back().get(redsea::BLOCK1) == 0x22E1);
  }

  SECTION("Rejects triple bit flip") {
    std::string broken_group{correct_group};
    flipAsciiBit(broken_group, 1);
    flipAsciiBit(broken_group, 2);
    flipAsciiBit(broken_group, 3);

    const std::string test_data{correct_group + correct_group + broken_group};
    const auto groups{asciibin2groups(test_data, options)};

    CHECK(groups.back().getNumErrors() == 1);
    CHECK_FALSE(groups.back().has(redsea::BLOCK1));
    CHECK(groups.back().get(redsea::BLOCK1) == 0x0000);  // "----"
  }

  SECTION("Rejects double bit flip if FEC is disabled") {
    options.use_fec = false;

    std::string broken_group{correct_group};
    flipAsciiBit(broken_group, 1);
    flipAsciiBit(broken_group, 2);

    const std::string test_data{correct_group + correct_group + broken_group};
    const auto groups{asciibin2groups(test_data, options)};

    CHECK(groups.back().getNumErrors() == 1);
    CHECK_FALSE(groups.back().has(redsea::BLOCK1));
    CHECK(groups.back().get(redsea::BLOCK1) == 0x0000);  // "----"
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

TEST_CASE("CSV reader") {
  const std::string testfilename{"testfile.csv"};

  std::ofstream out(testfilename);
  out << "num;a;b;c\n0;-16;+8;7\nzero;minus 16;plus 8;seitsemän";
  out.close();

  SECTION("Simple read without titles") {
    auto csv = redsea::readCSV(testfilename, ';');

    CHECK(csv.size() == 3);
    CHECK(csv.at(1).at(3) == "7");
  }

  SECTION("Get values by column title") {
    auto csv = redsea::readCSVWithTitles(testfilename, ';');

    CHECK(csv.rows.size() == 2);
    CHECK(redsea::get_int(csv, csv.rows.at(0), "a") == -16);
    CHECK(redsea::get_int(csv, csv.rows.at(0), "b") == 8);
    CHECK(redsea::get_uint16(csv, csv.rows.at(0), "c") == 7);

    CHECK(redsea::get_string(csv, csv.rows.at(1), "c") == "seitsemän");
  }
}
