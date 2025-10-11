#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include "src/tmc/tmc.hh"
#include "test_helpers.hh"

// Note: We don't actually know if any of these TMC are correctly decoded, but they do look kind of
// sensible, which is better than nothing?

// In theory, someone could try and obtain the Denmark location table and check that the locations
// make sense in the context of these messages.

TEST_CASE("TMC") {
  redsea::Options options;

  SECTION("System info") {
    // DR P4 København (da) 2019-05-04
    // walczakp/rds-spy-logs/Denmark/9602 - 2019-05-04 17-55-01.spy
    // clang-format off
    const auto json_lines{hex2json({
      0x9602'3410'0267'CD46,
      0x9602'3410'5B49'CD46},
    options, 0x9602)};
    // clang-format on

    REQUIRE(json_lines.size() == 2);
    CHECK(json_lines.at(0)["open_data_app"]["oda_group"] == "8A");
    CHECK(json_lines.at(0)["open_data_app"]["app_name"] == "RDS-TMC: ALERT-C");
    CHECK(json_lines.at(0)["tmc"]["system_info"]["is_encrypted"] == false);
    CHECK(json_lines.at(0)["tmc"]["system_info"]["location_table"] == 9);
    CHECK(json_lines.at(1)["tmc"]["system_info"]["service_id"] == 45);
    CHECK(json_lines.at(1)["tmc"]["system_info"]["gap"] == 5);
    CHECK(json_lines.at(1)["tmc"]["system_info"]["ltcc"] == 9);
  }

  SECTION("Message 1") {
    // DR P4 København (da) 2019-05-04
    // walczakp/rds-spy-logs/Denmark/9602 - 2019-05-04 17-55-01.spy
    // clang-format off
    const auto json_lines{hex2json({
      0x9602'3410'0267'CD46,

      0x9602'8405'C852'2550,
      0x9602'8405'48F4'0000},
    options, 0x9602)};
    // clang-format on

    REQUIRE(json_lines.size() == 3);
    REQUIRE(json_lines.at(2)["tmc"].contains("message"));
    CHECK(listEquals(json_lines.at(2)["tmc"]["message"]["event_codes"], {82}));
    CHECK(json_lines.at(2)["tmc"]["message"]["update_class"] == 32);
    // Not an unlikely message to see in the summer, so could be correct :)
    CHECK(json_lines.at(2)["tmc"]["message"]["description"] ==
          "Roadworks. Heavy traffic has to be expected.");
    CHECK(json_lines.at(2)["tmc"]["message"]["location"] == 9552);
    CHECK(json_lines.at(2)["tmc"]["message"]["direction"] == "single");
    CHECK(json_lines.at(2)["tmc"]["message"]["extent"] == "-1");
    // The message was received in May, so "mid-July" is plausible
    CHECK(json_lines.at(2)["tmc"]["message"]["until"] == "mid-July");
    CHECK(json_lines.at(2)["tmc"]["message"]["urgency"] == "none");
  }

  SECTION("Message 2: Speed limit") {
    // DR P4 København (da) 2019-05-04
    // walczakp/rds-spy-logs/Denmark/9602 - 2019-05-04 17-55-01.spy
    // clang-format off
    const auto json_lines{hex2json({
      0x9602'3410'0267'CD46,

      0x9602'8406'D2BD'06DB,
      0x9602'8406'4384'7E00},
    options, 0x9602)};
    // clang-format on

    REQUIRE(json_lines.at(2)["tmc"].contains("message"));
    CHECK(listEquals(json_lines.at(2)["tmc"]["message"]["event_codes"], {701}));
    CHECK(json_lines.at(2)["tmc"]["message"]["update_class"] == 11);
    CHECK(json_lines.at(2)["tmc"]["message"]["description"] == "Roadworks.");
    // If 1755 is on a highway then 80 km/h makes sense
    CHECK(json_lines.at(2)["tmc"]["message"]["speed_limit"] == "80 km/h");
    CHECK(json_lines.at(2)["tmc"]["message"]["location"] == 1755);
    // Note: Kind of weird that it's one-way only, but again, could be true for a highway
    CHECK(json_lines.at(2)["tmc"]["message"]["direction"] == "single");
    CHECK(json_lines.at(2)["tmc"]["message"]["extent"] == "-2");
    // A bit longer roadworks but it could work
    CHECK(json_lines.at(2)["tmc"]["message"]["until"] == "mid-November");
    CHECK(json_lines.at(2)["tmc"]["message"]["urgency"] == "none");
  }

  SECTION("Message 3: Multi-event") {
    // Radio-K (at) 2021-07-26
    // walczakp/rds-spy-logs/Austria/A502_-_2021-07-26_19-26-33.spy
    // clang-format off
    const auto json_lines{hex2json({
      0xA502'3410'0064'CD46,

      0xA502'8405'C201'7BEB,
      0xA502'8405'415D'2C8C},
    options, 0xA502)};
    // clang-format on

    REQUIRE(json_lines.size() == 3);
    REQUIRE(json_lines.at(2)["tmc"].contains("message"));
    CHECK(listEquals(json_lines.at(2)["tmc"]["message"]["event_codes"], {513, 803}));
    CHECK(json_lines.at(2)["tmc"]["message"]["update_class"] == 5);
    // Very normal message to see in the summer -> could be correct
    CHECK(json_lines.at(2)["tmc"]["message"]["description"] ==
          "Single alternate line traffic. Construction work.");
    CHECK(json_lines.at(2)["tmc"]["message"]["location"] == 31723);
    CHECK(json_lines.at(2)["tmc"]["message"]["direction"] == "single");
    CHECK(json_lines.at(2)["tmc"]["message"]["extent"] == "-0");
    CHECK(json_lines.at(2)["tmc"]["message"]["urgency"] == "none");
  }

  SECTION("Message 4: Multi-event with quantifier") {
    // Ö1 (at) 2017-12-27
    // rds-spy-logs/Austria/A203 - 2017-12-27 18-22-00 AT LA OE1 92.1.rds
    // clang-format off
    const auto json_lines{hex2json({
      0xA201'3010'0064'CD46,

      0xA201'8003'C641'8097,
      0xA201'8003'441F'4865},
    options, 0xA201)};
    // clang-format on

    REQUIRE(json_lines.size() == 3);
    REQUIRE(json_lines.at(2)["tmc"].contains("message"));
    CHECK(json_lines.at(2)["tmc"]["message"]["description"] ==
          "Delays of up to 15 minutes. Stationary traffic.");
  }
}
