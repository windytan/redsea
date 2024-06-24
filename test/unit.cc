#include <sstream>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include "../src/block_sync.h"
#include "../src/channel.h"
#include "../src/common.h"
#include "../src/groups.h"
#include "../src/options.h"
#include "test_helpers.h"

TEST_CASE("Decodes basic info") {
  redsea::Options options;

  // YLE X3M (fi) 2016-09-15
  const auto json_lines{hex2json({
    0x6204'0130'966B'594C,
    0x6204'0131'93CD'4520,
    0x6204'0132'E472'5833,
    0x6204'0137'966B'4D20
  }, options, 0x6204)};

  REQUIRE(json_lines.size() == 4);

  for (const auto& group : json_lines) {
    CHECK(group["pi"]                == "0x6204");
    CHECK(group["group"]             == "0A");
    CHECK(json_lines[0]["prog_type"] == "Varied");
    CHECK(group["tp"]                == false);
    CHECK(group["ta"]                == true);
    CHECK(json_lines[0]["is_music"]  == false);
  }

  // https://github.com/windytan/redsea/issues/86
  {
    CHECK(json_lines[0]["di"]["dynamic_pty"]     == false);
    CHECK(json_lines[1]["di"]["compressed"]      == false);
    CHECK(json_lines[2]["di"]["artificial_head"] == false);
    CHECK(json_lines[3]["di"]["stereo"]          == true);
  }

  CHECK(json_lines[3]["ps"] == "YLE X3M ");
}

TEST_CASE("Decodes callsign") {
  redsea::Options options;

  SECTION("RBDS station") {
    options.rbds = true;

    const auto json_lines{hex2json({
      0x5521'2000'0D00'0000
    }, options, 0x5521)};

    CHECK(json_lines.back()["callsign"] == "WAER");
  }

  SECTION("No callsign for non-RBDS station") {
    const auto json_lines{hex2json({
      0x5521'2000'0D00'0000
    }, options, 0x5521)};

    CHECK_FALSE(json_lines.back().contains("callsign"));
  }
}

// https://github.com/windytan/redsea/wiki/Some-RadioText-research
TEST_CASE("Decodes radiotext") {
  redsea::Options options;

  SECTION("String length method A: Terminated using 0x0D") {
    // JACK 96.9 (ca) 2019-05-05
    const auto json_lines{hex2json({
      0xC954'24F0'4A41'434B,  // "JACK"
      0xC954'24F1'2039'362E,  // " 96."
      0xC954'24F2'390D'0000   // "9\r  "
    }, options, 0xC954)};

    REQUIRE(json_lines.size() == 3);
    CHECK(json_lines.back()["radiotext"] == "JACK 96.9");
  }

  SECTION("String length method B: Padded to 64 characters") {
    // Radio Grün-Weiß (at) 2021-07-18
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

    REQUIRE(json_lines.size() == 16);
    CHECK(json_lines.back()["radiotext"] == "FANCY - Bolero");
  }

  SECTION("String length method C: Random-length string with no terminator") {
    // Antenne Kärnten (at) 2021-07-26
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

    REQUIRE(json_lines.size() == 13);
    CHECK(json_lines.back()["radiotext"] == "Robbie Williams - Feel");
  }

  SECTION("Non-ascii character") {
    // YLE Vega (fi) 2016-09-15
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

    REQUIRE(json_lines.size() == 16);
    CHECK(json_lines.back()["radiotext"] == "Vega Kväll");
  }

  SECTION("Partial") {
    options.show_partial = true;

    // Antenne Kärnten (at) 2021-07-26
    const auto json_lines{hex2json({
      0xA540'2540'526F'6262,  // "Robb"
      0xA540'2541'6965'2057,  // "ie W"
      0xA540'2542'696C'6C69,  // "illi"
      0xA540'2543'616D'7320,  // "ams "
      0xA540'2544'2D20'4665   // "- Fe"
    }, options, 0xA540)};

    REQUIRE(json_lines.size() == 5);
    REQUIRE(json_lines.back().contains("partial_radiotext"));
    CHECK(json_lines.back()["partial_radiotext"] == "Robbie Williams - Fe"
                                                    "                                            ");
  }
}

TEST_CASE("Decodes Long PS") {
  redsea::Options options;

  SECTION("Space-padded") {
    // The Breeze Gold Coast 100.6 (au) 2024-05-17
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

    REQUIRE(json_lines.back().contains("long_ps"));
    CHECK(json_lines.back()["long_ps"] == "Breeze 100.6 Gold Coast");
  }

  SECTION("String-terminated, non-ascii character") {
    // Järviradio (fi)
    const auto json_lines{hex2json({
      0x6255'F520'4AC3'A452,
      0x6255'F521'5649'5241,
      0x6255'F522'4449'4F0D
    }, options, 0x6255)};

    REQUIRE(json_lines.back().contains("long_ps"));
    CHECK(json_lines.back()["long_ps"] == "JäRVIRADIO");  // sic
  }
}

TEST_CASE("Decodes RadioText Plus") {
    redsea::Options options;

    SECTION("Containing non-ascii characters") {
      // Antenne 2016-09-17
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

      REQUIRE(json_lines.back()["radiotext_plus"]["tags"].size()           == 2);
      CHECK(json_lines.back()["radiotext_plus"]["tags"][0]["content-type"] == "item.artist");
      CHECK(json_lines.back()["radiotext_plus"]["tags"][0]["data"]         == "Christina Stürmer");
      CHECK(json_lines.back()["radiotext_plus"]["tags"][1]["content-type"] == "item.title");
      CHECK(json_lines.back()["radiotext_plus"]["tags"][1]["data"]         == "Ein Teil von mir");
    }
}

TEST_CASE("Decodes alternative frequencies") {
  redsea::Options options;

  SECTION("Method A") {
    // YLE Yksi (fi) 2016-09-15
    const auto json_lines{hex2json({
      0x6201'00F7'E704'5349,
      0x6201'00F0'2217'594C,
      0x6201'00F1'1139'4520,
      0x6201'00F2'0A14'594B
    }, options, 0x6201)};

    REQUIRE(json_lines.size() == 4);
    REQUIRE(json_lines.back().contains("alt_frequencies_a"));
    CHECK(listEquals(json_lines.back()["alt_frequencies_a"],
          {87'900, 90'900, 89'800, 89'200, 93'200, 88'500, 89'500}));
  }

  SECTION("Method B") {
    // YLE Helsinki (fi) 2016-09-15
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

    REQUIRE(json_lines.size() == 12);
    REQUIRE(json_lines.back().contains("alt_frequencies_b"));
    CHECK(json_lines.back()["alt_frequencies_b"]["tuned_frequency"] == 94'000);
    CHECK(listEquals(json_lines.back()["alt_frequencies_b"]["same_programme"],
            {97'000, 90'300, 95'000, 96'100, 99'100}));
    CHECK(listEquals(json_lines.back()["alt_frequencies_b"]["regional_variants"],
            {94'300, 96'000, 97'900, 96'900, 107'800, 105'800}));
  }
}

TEST_CASE("Decodes clock-time and date") {
  redsea::Options options;

  SECTION("During DST") {
    // BR-KLASS (de) 2017-04-04
    const auto json_lines{hex2json({
      0xD314'41C1'C3EF'5AC4
    }, options, 0xD314)};

    REQUIRE(json_lines.size() == 1);
    REQUIRE(json_lines.back().contains("clock_time"));
    CHECK(json_lines.back()["clock_time"] == "2017-04-04T23:43:00+02:00");
  }

  SECTION("Outside of DST") {
    // 104.6RTL (de) 2018-11-01
    // walczakp/rds-spy-logs/Germany/D42A - 2018-11-01 14-17-16 DE BER RTL104_6.rds
    const auto json_lines{hex2json({
      0xD42A'4541'C86E'D482
    }, options, 0xD42A)};

    REQUIRE(json_lines.size() == 1);
    REQUIRE(json_lines.back().contains("clock_time"));
    CHECK(json_lines.back()["clock_time"] == "2018-11-01T14:18:00+01:00");
  }

  SECTION("With a negative UTC offset") {
    // 98.5 KFOX (KUFX) (us) 2020-08-19
    // walczakp/rds-spy-logs/USA/4569 - 2020-08-19 20-45-06.spy
    const auto json_lines{hex2json({
      0x4569'40DD'CD92'3BAE
    }, options, 0x4569)};

    REQUIRE(json_lines.size() == 1);
    REQUIRE(json_lines.back().contains("clock_time"));
    CHECK(json_lines.back()["clock_time"] == "2020-08-19T20:46:00-07:00");
  }

  SECTION("Across local midnight") {
    // https://github.com/windytan/redsea/issues/83
    const auto json_lines{hex2json({
      0xF201'441D'D299'5EC4,
      0xF201'441D'D299'6004
    }, options, 0xF201)};

    REQUIRE(json_lines.size() == 2);
    REQUIRE(json_lines[0].contains("clock_time"));
    REQUIRE(json_lines[1].contains("clock_time"));
    CHECK(json_lines[0]["clock_time"] == "2022-05-25T23:59:00+02:00");
    CHECK(json_lines[1]["clock_time"] == "2022-05-26T00:00:00+02:00");
  }

  SECTION("Across UTC midnight") {
    // https://github.com/windytan/redsea/issues/83
    const auto json_lines{hex2json({
      0xF201'441D'D299'7EC4,
      0xF201'441D'D29A'0004
    }, options, 0xF201)};

    REQUIRE(json_lines.size() == 2);
    REQUIRE(json_lines[0].contains("clock_time"));
    REQUIRE(json_lines[1].contains("clock_time"));
    CHECK(json_lines[0]["clock_time"] == "2022-05-26T01:59:00+02:00");
    CHECK(json_lines[1]["clock_time"] == "2022-05-26T02:00:00+02:00");
  }
}

TEST_CASE("PI search") {
  redsea::Options options;

  SECTION("Accepts new PI from three repeats") {
    // Vikerraadio (ee)
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

    REQUIRE(json_lines.size() == 1);
    CHECK(json_lines[0]["pi"] == "0x22E1");
  }

  SECTION("Ignores phantom sync caused by data-mimicking") {
    // Noise that shouldn't even sync
    // It also happens to look like two repeats of PI 0x40AF
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

    CHECK(groups.empty());
  }
}

TEST_CASE("Error detection and correction") {
  redsea::Options options;
  const std::string correct_group{
    "00100010111000010111001100"
    "00100101100000111100111110"
    "00100000011001011011010011"
    "01101001001000000110111110"};

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

  SECTION("FEC can be disabled") {
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
