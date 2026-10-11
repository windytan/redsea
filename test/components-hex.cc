// Redsea tests: Component tests for hex input
// All different kinds of messages we can receive should go here

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
    const auto json_lines = hex2json({
      0x6204'0130'966B'594C,
      0x6204'0131'93CD'4520,
      0x6204'0132'E472'5833,
      0x6204'0137'966B'4D20
    }, options, 0x6204);
    // clang-format on

    REQUIRE(json_lines.size() == 4);

    for (const auto& group : json_lines) {
      REQUIRE(group.is_object());
      CHECK(group["pi"] == "0x6204");
      CHECK(group["group"] == "0A");
      CHECK(group["prog_type"] == "Varied");
      CHECK(group["tp"] == false);
      CHECK(group["ta"] == true);
      CHECK(group["is_music"] == false);
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
    const auto json_lines = hex2json({0x9423'0800'0000'2020, 0x9423'0801'0000'4B52,
                                      0x9423'0802'0000'4B41, 0x9423'0807'0000'2020},
                                     options, 0x9423);

    REQUIRE_FALSE(json_lines.empty());
    REQUIRE(json_lines.back().is_object());
    CHECK(json_lines.back()["pi"] == "0x9423");
    CHECK(json_lines.back()["ps"] == "  KRKA  ");
  }

  SECTION("Using Group 15B") {
    // Дорожное 2017-07-03
    // clang-format off
    const auto json_lines = hex2json({
      0x7827'F928'7827'F928
    }, options, 0x7827);
    // clang-format on

    REQUIRE_FALSE(json_lines.empty());
    REQUIRE(json_lines.back().is_object());
    CHECK(json_lines.back()["group"] == "15B");
    CHECK(json_lines.back()["prog_type"] == "Varied");
    CHECK(json_lines.back()["tp"] == false);
  }

  // The test for 15B with missing block 2 is in components-bits.cc
}

TEST_CASE("PTY name") {
  redsea::Options options;
  // walczakp/rds-spy-logs/Poland/3ABC - 2019-05-04 22-36-23.spy
  // clang-format off
  const auto json_lines = hex2json({
    0x3ABC'A750'4352'492E,
    0x3ABC'A751'434E'0D0D
  }, options, 0x3ABC);
  // clang-format on

  REQUIRE(json_lines.size() == 2);
  REQUIRE(json_lines[0].is_object());
  REQUIRE(json_lines[1].is_object());
  REQUIRE(json_lines[1].contains("pty_name"));
  CHECK(json_lines.back()["pty_name"] == "CRI.CN ");
}

TEST_CASE("PIN & SLC (Group 1)") {
  redsea::Options options;

  SECTION("PIN, SLC variants 0 & 3") {
    // YLE Yksi (fi) 2016-09-15
    // NOTE: PIN has disappeared from the RDS standard in 2021. Nowadays these bits are RFU.
    // clang-format off
    const auto json_lines = hex2json({
      0x6201'10E0'00E1'7C54,
      0x6201'10E0'3027'7C54
    }, options, 0x6201);
    // clang-format on

    REQUIRE(json_lines.size() == 2);
    REQUIRE(json_lines[0].is_object());
    REQUIRE(json_lines[1].is_object());
    CHECK(json_lines.at(0)["prog_item_number"] == 31828);
    CHECK(json_lines.at(0)["prog_item_started"]["day"] == 15);
    CHECK(json_lines.at(0)["prog_item_started"]["time"] == "17:20");
    CHECK(json_lines.at(0)["country"] == "fi");
    CHECK(json_lines.at(1)["language"] == "Finnish");
  }

  SECTION("SLC variant 6") {
    // RTL 102.5 (it) 2019-05-04
    // walczakp/rds-spy-logs/Italy/5218 - 2019-05-04 22-24-42.spy
    const auto json_lines = hex2json({0x5218'1520'6DAB'0000}, options, 0X5218);

    REQUIRE(json_lines.size() == 1);
    REQUIRE(json_lines.back().is_object());
    CHECK(json_lines.back()["slc_broadcaster_bits"] == "0x5AB");
  }
}

TEST_CASE("Callsign") {
  redsea::Options options;

  SECTION("RBDS station (USA)") {
    options.rbds = true;

    // 98.5 KFOX (KUFX) (us) 2020-08-19
    // walczakp/rds-spy-logs/USA/4569 - 2020-08-19 20-45-06.spy
    const auto json_lines = hex2json({0x4569'00C8'CDCD'416E}, options, 0x4569);

    REQUIRE_FALSE(json_lines.empty());
    CHECK(json_lines.back()["callsign"] == "KUFX");
  }

  SECTION("RBDS station + TMC: Uncertain callsign") {
    options.rbds = true;

    // walczakp/rds-spy-logs/USA/16C6 - 2019-05-04 21-43-25.spy
    const auto json_lines = hex2json({0x16C6'00EA'E0CD'6F77}, options, 0x16C6);

    REQUIRE_FALSE(json_lines.empty());
    REQUIRE(json_lines.back().is_object());
    CHECK(json_lines.back()["callsign_uncertain"] == "KCOS");
  }

  SECTION("RBDS station (Canada)") {
    options.rbds = true;

    // CBC Radio 2 (ca)
    // walczakp/rds-spy-logs/Canada/B203 - 2019-05-05 09-33-12.spy
    const auto json_lines = hex2json({0xB203'21C1'5553'4943}, options, 0xB203);

    REQUIRE_FALSE(json_lines.empty());
    REQUIRE(json_lines.back().is_object());
    CHECK(json_lines.back()["callsign"] == "CBC English - Radio Two");
  }

  SECTION("No callsign for non-RBDS station") {
    options.rbds = false;

    const auto json_lines = hex2json({0x4569'00C8'CDCD'416E}, options, 0x4569);

    REQUIRE_FALSE(json_lines.empty());
    REQUIRE(json_lines.back().is_object());
    CHECK_FALSE(json_lines.back().contains("callsign"));
  }
}

// Radio Paging (group 7A, EN 50067 Annex M). Vectors are hand-built from the
// Annex M wire format (block B bits 4-0 = A/B flag + PSAC; address packed as
// 6 BCD digits across block C and the high byte of block D).
TEST_CASE("Radio Paging (Group 7A)") {
  redsea::Options options;

  SECTION("Tone-only page") {
    // PSAC=0, A-flag=0, address 123456, control byte 0x00.
    // Block B = 0x7000 (7A, TP=0, PTY=0, AB=0, PSAC=0)
    // Block C = 0x1234 (Y1Y2Z1Z2), Block D = 0x5600 (Z3Z4 | ctrl)
    const auto json_lines = hex2json({0x1234'7000'1234'5600}, options, 0x1234);

    REQUIRE(json_lines.size() == 1);
    REQUIRE(json_lines.back().is_object());
    CHECK(json_lines.back()["radio_paging"]["call"]["type"] == "tone");
    CHECK(json_lines.back()["radio_paging"]["call"]["address"] == 123456);
    CHECK(json_lines.back()["radio_paging"]["call"]["call_flag"] == "A");
  }

  SECTION("Tone-only page with E2-E0 extended field and call counter") {
    // Control byte 0x63 = E2E1E0=011 (0x3), R=0, P3-P0=0011 (counter 3).
    // Block D = 0x5663 (Z3Z4 | control byte 0x63). EN 50067 Table M.12.
    const auto json_lines = hex2json({0x1234'7000'1234'5663}, options, 0x1234);

    REQUIRE(json_lines.size() == 1);
    CHECK(json_lines.back()["radio_paging"]["call"]["type"] == "tone");
    CHECK(json_lines.back()["radio_paging"]["call"]["tone_extended"] == 3);
    CHECK(json_lines.back()["radio_paging"]["call"]["call_counter"] == 3);
  }

  SECTION("10-digit numeric page") {
    // Address 123456, message "7890123456".
    // Group 1 (PSAC=2): addr + first 2 digits "78" -> D low = 0x78
    // Group 2 (PSAC=3): remaining 8 digits "90123456" -> C=0x9012 D=0x3456
    const auto json_lines =
        hex2json({0x1234'7002'1234'5678, 0x1234'7003'9012'3456}, options, 0x1234);

    REQUIRE(json_lines.size() == 2);
    CHECK(json_lines.back()["radio_paging"]["call"]["type"] == "numeric");
    CHECK(json_lines.back()["radio_paging"]["call"]["address"] == 123456);
    CHECK(json_lines.back()["radio_paging"]["call"]["message"] == "7890123456");
  }

  SECTION("Alphanumeric page") {
    // Address 123456, message "HELO".
    // Group 1 (PSAC=8): addr + control byte 0x00 (MT=alpha, NI=0, R=0, P=0)
    // Group 2 (PSAC=9): 'H' 'E' 'L' 'O' -> C=0x4845 D=0x4C4F
    // Group 3 (PSAC=F): end of message
    const auto json_lines = hex2json(
        {0x1234'7008'1234'5600, 0x1234'7009'4845'4C4F, 0x1234'700F'0000'0000}, options, 0x1234);

    REQUIRE(json_lines.size() == 3);
    CHECK(json_lines.back()["radio_paging"]["call"]["type"] == "alphanumeric");
    CHECK(json_lines.back()["radio_paging"]["call"]["address"] == 123456);
    CHECK(json_lines.back()["radio_paging"]["call"]["message"] == "HELO");
  }

  SECTION("A short alpha message does not truncate a following longer one") {
    // Regression: finishMessage shrinks the alpha buffer to the received
    // length; reset() must restore the full capacity. A 2-char international
    // message ("HI", via the M.3.5.7 header group) followed by a 4-char
    // national one ("HELO") must still decode "HELO" in full.
    const auto json_lines =
        hex2json({0x1234'7008'1234'5620, 0x1234'7009'2345'4849, 0x1234'700F'0000'0000,
                  0x1234'7008'1234'5600, 0x1234'7009'4845'4C4F, 0x1234'700F'0000'0000},
                 options, 0x1234);

    REQUIRE(json_lines.size() == 6);
    CHECK(json_lines.at(2)["radio_paging"]["call"]["message"] == "HI");
    CHECK(json_lines.back()["radio_paging"]["call"]["message"] == "HELO");
  }

  SECTION("A lost data group is filled with '?' and the message tagged incomplete") {
    // Alpha message with data group PSAC 0xA dropped: 9 ("ABCD"), [A lost],
    // B ("IJKL"), end F. The gap is filled with 4 '?' and flagged incomplete.
    const auto json_lines = hex2json({0x1234'7008'1234'5600, 0x1234'7009'4142'4344,
                                      0x1234'700B'494A'4B4C, 0x1234'700F'0000'0000},
                                     options, 0x1234);

    REQUIRE(json_lines.size() == 4);
    CHECK(json_lines.back()["radio_paging"]["call"]["message"] == "ABCD????IJKL");
    CHECK(json_lines.back()["radio_paging"]["call"]["incomplete"] == true);
  }

  SECTION("A pre-empted message is emitted as partial with --show-partial") {
    redsea::Options partial_opts;
    partial_opts.show_partial = true;
    // Alpha "ABCD" with no end marker, then a NEW call (fresh address group)
    // pre-empts it. The first message is emitted partial + incomplete.
    const auto json_lines =
        hex2json({0x1234'7008'1234'5600, 0x1234'7009'4142'4344, 0x1234'7008'1234'5600,
                  0x1234'7009'4546'4748, 0x1234'700F'0000'0000},
                 partial_opts, 0x1234);

    REQUIRE_FALSE(json_lines.empty());
    // The pre-empting group's line carries the partial of the first message.
    bool found_partial = false;
    for (const auto& line : json_lines) {
      if (!line.contains("radio_paging"))
        continue;
      const auto& rp = line["radio_paging"];
      if (!rp.contains("call"))
        continue;
      const auto& call = rp["call"];
      if (call.contains("incomplete") && call.contains("message") && call["message"] == "ABCD")
        found_partial = true;
    }
    CHECK(found_partial);
  }

  SECTION("International alphanumeric page (NI=1, country code + OPC)") {
    // EN 50067 M.3.5.7, Figure M.17.
    // Group 1 (PSAC=8): address + control byte 0x20 (NI=1, R=0, P=0)
    // Group 2 (PSAC=9): header -> C = I1I2I3 OPC (country 234, OPC 5 -> 0x2345),
    //                   D = first 2 chars 'H' 'I' (0x4849)
    // Group 3 (PSAC=F): end of message
    const auto json_lines = hex2json(
        {0x1234'7008'1234'5620, 0x1234'7009'2345'4849, 0x1234'700F'0000'0000}, options, 0x1234);

    REQUIRE(json_lines.size() == 3);
    CHECK(json_lines.back()["radio_paging"]["call"]["type"] == "alphanumeric_international");
    CHECK(json_lines.back()["radio_paging"]["call"]["address"] == 123456);
    CHECK(json_lines.back()["radio_paging"]["call"]["country_code"] == 234);
    CHECK(json_lines.back()["radio_paging"]["call"]["operator_code"] == 5);
    CHECK(json_lines.back()["radio_paging"]["call"]["message"] == "HI");
  }

  SECTION("Variable-length numeric page") {
    // EN 50067 M.3.5.5, Figure M.15. Control byte 0x40 = bits 7-6 = 01 (numeric).
    // Group 1 (PSAC=8): address + control byte 0x40 -> D = 0x5640
    // Group 2 (PSAC=9): 8 digits "12345678" -> C=0x1234 D=0x5678
    // Group 3 (PSAC=F): end
    const auto json_lines = hex2json(
        {0x1234'7008'1234'5640, 0x1234'7009'1234'5678, 0x1234'700F'0000'0000}, options, 0x1234);

    REQUIRE(json_lines.size() == 3);
    CHECK(json_lines.back()["radio_paging"]["call"]["type"] == "variable_numeric");
    CHECK(json_lines.back()["radio_paging"]["call"]["address"] == 123456);
    CHECK(json_lines.back()["radio_paging"]["call"]["message"] == "12345678");
  }

  SECTION("Variable-length functions page") {
    // EN 50067 M.3.5.6, Figure M.16. Control byte 0xC0 = bits 7-6 = 11 (functions).
    // Group 1 (PSAC=8): address + control byte 0xC0 -> D = 0x56C0
    // Group 2 (PSAC=9): 8 hex nibbles "ABCDEF01" -> C=0xABCD D=0xEF01
    // Group 3 (PSAC=F): end
    const auto json_lines = hex2json(
        {0x1234'7008'1234'56C0, 0x1234'7009'ABCD'EF01, 0x1234'700F'0000'0000}, options, 0x1234);

    REQUIRE(json_lines.size() == 3);
    CHECK(json_lines.back()["radio_paging"]["call"]["type"] == "functions");
    CHECK(json_lines.back()["radio_paging"]["call"]["message"] == "ABCDEF01");
  }

  SECTION("International functions page (header block D uses full 0-F hex)") {
    // EN 50067 M.3.5.7, Figure M.19. Control byte 0xE0 = bits 7-6 = 11
    // (functions), NI = 1 (bit 5). The header group's block D = F1 F2 F3 F4,
    // coded as full 0-F hex (regression for the GAP where it used BCD A=space).
    // Group 1 (PSAC=8): address + control byte 0xE0 -> D = 0x56E0
    // Group 2 (PSAC=9): header C = I1I2I3 OPC (country 234, OPC 5 -> 0x2345),
    //                   D = F1F2F3F4 = 0xABCD -> "ABCD"
    // Group 3 (PSAC=F): end
    const auto json_lines = hex2json(
        {0x1234'7008'1234'56E0, 0x1234'7009'2345'ABCD, 0x1234'700F'0000'0000}, options, 0x1234);

    REQUIRE(json_lines.size() == 3);
    CHECK(json_lines.back()["radio_paging"]["call"]["type"] == "functions_international");
    CHECK(json_lines.back()["radio_paging"]["call"]["country_code"] == 234);
    CHECK(json_lines.back()["radio_paging"]["call"]["operator_code"] == 5);
    CHECK(json_lines.back()["radio_paging"]["call"]["message"] == "ABCD");
  }

  SECTION("International 15-digit numeric page") {
    // EN 50067 M.2.1.6.5, Figure M.6, Table M.5. PSAC sequence 7 -> 5 -> 6.
    // Country code X1X2X3 = 2,3,4 -> 234. Message A1..A15 = "123456789012345".
    // Group 1 (PSAC=7): addr + D low = X1 X2 = 0x23 -> D = 0x5623
    // Group 2 (PSAC=5): C = X3 A1 A2 A3 = 0x4123, D = A4 A5 A6 A7 = 0x4567
    // Group 3 (PSAC=6): C = A8..A11 = 0x8901, D = A12..A15 = 0x2345
    const auto json_lines = hex2json(
        {0x1234'7007'1234'5623, 0x1234'7005'4123'4567, 0x1234'7006'8901'2345}, options, 0x1234);

    REQUIRE(json_lines.size() == 3);
    CHECK(json_lines.back()["radio_paging"]["call"]["type"] == "numeric_international_15");
    CHECK(json_lines.back()["radio_paging"]["call"]["address"] == 123456);
    CHECK(json_lines.back()["radio_paging"]["call"]["country_code"] == 234);
    CHECK(json_lines.back()["radio_paging"]["call"]["message"] == "123456789012345");
  }

  SECTION("International functions message (PSAC 1 + 3)") {
    // EN 50067 M.2.1.6.6, Figure M.7, Table M.6. PSAC sequence 1 -> 3.
    // Country code X1X2X3 = 2,3,4 -> 234. Functions F1..F7 = "ABCDEF0" (hex).
    // Group 1 (PSAC=1): addr + D low = X1 X2 = 0x23 -> D = 0x5623
    // Group 2 (PSAC=3): C = X3 F1 F2 F3 = 0x4ABC, D = F4 F5 F6 F7 = 0xDEF0
    const auto json_lines =
        hex2json({0x1234'7001'1234'5623, 0x1234'7003'4ABC'DEF0}, options, 0x1234);

    REQUIRE(json_lines.size() == 2);
    CHECK(json_lines.back()["radio_paging"]["call"]["type"] == "functions_international");
    CHECK(json_lines.back()["radio_paging"]["call"]["address"] == 123456);
    CHECK(json_lines.back()["radio_paging"]["call"]["country_code"] == 234);
    CHECK(json_lines.back()["radio_paging"]["call"]["message"] == "ABCDEF0");
  }

  SECTION("RPC enabling context from Group 1A") {
    // Group 1A with RPC = 0x04 in block B bits 4-0 (desig=1 -> groups 00-99).
    // Block B = 0x1004 (1A, TP=0, PTY=0, RPC=0x04).
    const auto json_lines = hex2json({0x1234'1004'0000'0000}, options, 0x1234);

    REQUIRE(json_lines.size() == 1);
    CHECK(json_lines.back()["radio_paging"]["rpc"]["value"] == 4);
    CHECK(json_lines.back()["radio_paging"]["rpc"]["has_paging"] == true);
    CHECK(json_lines.back()["radio_paging"]["rpc"]["group_range"] == "00-99");
  }

  SECTION("System Information: Operator Code from 1A variant 0") {
    // 1A variant 0 (block C bits 14-12 = 000). OPC = bits 11-8 = 5,
    // ECC = bits 7-0 = 0xE2. Block C = 0x05E2. EN 50067 M.3.2, Figure M.10a.
    const auto json_lines = hex2json({0x1234'1000'05E2'0000}, options, 0x1234);

    REQUIRE(json_lines.size() == 1);
    CHECK(json_lines.back()["radio_paging"]["system_info"]["operator_code"] == 5);
    CHECK(json_lines.back()["radio_paging"]["system_info"]["enhanced_paging"] == true);
  }

  SECTION("System Information: OPC + PAC from 1A variant 2") {
    // 1A variant 2 (block C bits 14-12 = 010). PAC = bits 9-4 = 12,
    // OPC = bits 3-0 = 3. Block C = 0x20C3. EN 50067 M.3.2, Figure M.10b.
    const auto json_lines = hex2json({0x1234'1000'20C3'0000}, options, 0x1234);

    REQUIRE(json_lines.size() == 1);
    CHECK(json_lines.back()["radio_paging"]["system_info"]["operator_code"] == 3);
    CHECK(json_lines.back()["radio_paging"]["system_info"]["paging_area_code"] == 12);
    CHECK(json_lines.back()["radio_paging"]["system_info"]["enhanced_paging"] == true);
  }

  SECTION("System Information: OPC 0 means no enhanced paging") {
    // 1A variant 2, OPC = 0 -> no enhanced paging service (M.3.2.2, Note 1).
    // Block C = 0x2000 (variant 2, PAC 0, OPC 0).
    const auto json_lines = hex2json({0x1234'1000'2000'0000}, options, 0x1234);

    REQUIRE(json_lines.size() == 1);
    CHECK(json_lines.back()["radio_paging"]["system_info"]["enhanced_paging"] == false);
  }

  SECTION("7A reassigned to ODA is not decoded as paging") {
    // 3A assigns AID 0x4BD7 (RT+) to group 7A, then a 7A group must route to
    // ODA, not to the paging decoder. The target group-type lives in the 3A
    // block B bits 4-0: 7A = code 0x0E, so block B = 0x300E (3A, TP=0, PTY=0).
    const auto json_lines =
        hex2json({0x1234'300E'0000'4BD7, 0x1234'7000'1234'5600}, options, 0x1234);

    REQUIRE(json_lines.size() == 2);
    CHECK_FALSE(json_lines.back().contains("radio_paging"));
  }
}

// Enhanced Radio Paging (group 13A, EN 50067 Annex M Figures M.11a/M.11b).
// Block B: bits 4-3 = CS, bits 2-0 = STY. Block C: bits 15-12 = IT, bit 11 =
// S1, bit 10 = S2, bit 9 = X, bits 8-0 = notify[24:16]. Block D = notify[15:0].
TEST_CASE("Enhanced Radio Paging (Group 13A)") {
  redsea::Options options;

  SECTION("25-bit address notification (STY 000)") {
    // Block B = 0xD000 (13A, TP=0, PTY=0, CS=00 1-min, STY=000)
    // Block C = 0x50AA (IT=5, S1=0, S2=0, X=0, notify[24:16]=0x0AA)
    // Block D = 0x1234 (notify[15:0]) -> notify25 = 0x0AA1234
    const auto json_lines = hex2json({0x1234'D000'50AA'1234}, options, 0x1234);

    REQUIRE(json_lines.size() == 1);
    CHECK(json_lines.back()["radio_paging"]["enhanced"]["sub_type"] == "notification_25bit");
    CHECK(json_lines.back()["radio_paging"]["enhanced"]["cycle"] == "1min");
    CHECK(json_lines.back()["radio_paging"]["enhanced"]["interval"] == 5);
    CHECK(json_lines.back()["radio_paging"]["enhanced"]["sort"] == "none");
    CHECK(json_lines.back()["radio_paging"]["enhanced"]["notification_bits"] == 0x0AA1234);
  }

  SECTION("50-bit address notification (STY 001 then 010)") {
    // High half STY=001 CS=10: B=0xD011, notify[49:25] hi = 0x0055678
    // Low  half STY=010 CS=10: B=0xD012, notify[24:0]  lo = 0x00A4321
    const auto json_lines = hex2json(
        {
            0x1234'D011'0055'678A,  // C=0x0055 (IT=0,notify_hi=0x055) D=0x678A
            0x1234'D012'000A'4321   // C=0x000A (IT=0,notify_hi=0x00A) D=0x4321
        },
        options, 0x1234);

    REQUIRE(json_lines.size() == 2);
    CHECK(json_lines.at(0)["radio_paging"]["enhanced"]["sub_type"] == "notification_50bit_high");
    CHECK(json_lines.back()["radio_paging"]["enhanced"]["sub_type"] == "notification_50bit_low");
    CHECK(json_lines.back()["radio_paging"]["enhanced"]["notification_bits_hi"] == 0x055678A);
    CHECK(json_lines.back()["radio_paging"]["enhanced"]["notification_bits_lo"] == 0x00A4321);
  }

  SECTION("Sort indicator (S1=1, S2=0 -> ascending)") {
    // Block C = 0x5800 (IT=5, S1=1, S2=0) -> sort ascending
    const auto json_lines = hex2json({0x1234'D000'5800'0000}, options, 0x1234);

    REQUIRE(json_lines.size() == 1);
    CHECK(json_lines.back()["radio_paging"]["enhanced"]["sort"] == "ascending");
  }

  SECTION("13A reassigned to ODA is not decoded as paging") {
    // 3A assigns AID 0x4BD7 to group 13A (code 0x1A), block B = 0x301A.
    const auto json_lines =
        hex2json({0x1234'301A'0000'4BD7, 0x1234'D000'50AA'1234}, options, 0x1234);

    REQUIRE(json_lines.size() == 2);
    CHECK_FALSE(json_lines.back().contains("radio_paging"));
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
    const auto json_lines = hex2json({
      0xC954'24F0'4A41'434B,  // "JACK"
      0xC954'24F1'2039'362E,  // " 96."
      0xC954'24F2'390D'0000   // "9\r  "
    }, options, 0xC954);
    // clang-format on

    REQUIRE(json_lines.size() == 3);
    REQUIRE(json_lines.back().is_object());
    CHECK(json_lines.back()["radiotext"] == "JACK 96.9");

    // Other lines shouldn't have RadioText
    for (auto prev_line = std::begin(json_lines); prev_line <= std::prev(json_lines.end(), 2);
         prev_line++)
      REQUIRE_FALSE(prev_line->contains("radiotext"));
  }

  SECTION("String length method B: Padded to 64 characters") {
    // Radio Grün-Weiß (at) 2021-07-18
    // clang-format off
    const auto json_lines = hex2json({
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
    }, options, 0xA959);
    // clang-format on

    REQUIRE(json_lines.size() == 16);
    REQUIRE(json_lines.back().is_object());
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
    const auto json_lines = hex2json(
        {0xCD59'2120'414D'5020, 0xCD59'2121'2020'2020, 0xCD59'2122'2020'2020, 0xCD59'2123'2020'2020,
         0xCD59'2124'2020'2020, 0xCD59'2125'2020'2020, 0xCD59'2126'2020'2020, 0xCD59'2127'2020'2020,
         0xCD59'2128'2020'2020, 0xCD59'2129'2020'2020, 0xCD59'212A'2020'2020, 0xCD59'212B'2020'2020,
         0xCD59'212C'2020'2020, 0xCD59'212D'2020'2020, 0xCD59'212E'2020'2020,
         0xCD59'212F'2020'2020},
        options, 0xCD59);

    REQUIRE(json_lines.size() == 16);
    REQUIRE(json_lines.back().is_object());
    CHECK(json_lines.back()["radiotext"] == "AMP");

    // Other lines shouldn't have RadioText
    for (auto prev_line = std::begin(json_lines); prev_line < std::prev(json_lines.end(), 1);
         prev_line++)
      REQUIRE_FALSE(prev_line->contains("radiotext"));
  }

  SECTION("String length method B using Group 2B") {
    // Radio Krka (si)
    const auto json_lines = hex2json(
        {0x9423'2800'0000'5052, 0x9423'2801'0000'494A, 0x9423'2802'0000'4554, 0x9423'2803'0000'4E4F,
         0x9423'2804'0000'2050, 0x9423'2805'0000'4F53, 0x9423'2806'0000'4C55, 0x9423'2807'0000'5341,
         0x9423'2808'0000'4E4A, 0x9423'2809'0000'4520, 0x9423'280A'0000'5241, 0x9423'280B'0000'4449,
         0x9423'280C'0000'4120, 0x9423'280D'0000'4B52, 0x9423'280E'0000'4B41,
         0x9423'280F'0000'2020},
        options, 0x9423);

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
    const auto json_lines = hex2json({
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
    }, options, 0xA540);
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
    const auto json_lines = hex2json({
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
    }, options, 0x6205);
    // clang-format on

    REQUIRE(json_lines.size() == 16);
    CHECK(json_lines.back()["radiotext"] == "Vega Kväll");
  }

  SECTION("Partial") {
    options.show_partial = true;

    // Antenne Kärnten (at) 2021-07-26
    // clang-format off
    const auto json_lines = hex2json({
      0xA540'2540'526F'6262,  // "Robb"
      0xA540'2541'6965'2057,  // "ie W"
      0xA540'2542'696C'6C69,  // "illi"
      0xA540'2543'616D'7320,  // "ams "
      0xA540'2544'2D20'4665   // "- Fe"
    }, options, 0xA540);
    // clang-format on

    REQUIRE(json_lines.size() == 5);
    REQUIRE(json_lines.back().is_object());
    REQUIRE(json_lines.back().contains("partial_radiotext"));
    CHECK(json_lines.back()["partial_radiotext"] ==
          "Robbie Williams - Fe"
          "                                            ");
    CHECK(json_lines.back()["rt_ab"] == "A");

    // All lines should have RadioText
    for (auto line : json_lines) REQUIRE(line.contains("partial_radiotext"));
  }

  // https://github.com/windytan/redsea/issues/118
  SECTION("String length hybrid method A+B: Terminated *and* padded") {
    // Radio Austria (at) 2024
    // The station sends terminator 0x0D followed by padding spaces.
    // RadioText should be output exactly once per cycle (when string becomes complete).
    const auto json_lines = hex2json(
        {0xA3E0'2550'5375'7065, 0xA3E0'2551'7273'7461, 0xA3E0'2552'7273'2026, 0xA3E0'2553'2053'7570,
         0xA3E0'2554'6572'6869, 0xA3E0'2555'7473'0D20, 0xA3E0'2556'2020'2020, 0xA3E0'2557'2020'2020,
         0xA3E0'2558'2020'2020, 0xA3E0'2559'2020'2020, 0xA3E0'255A'2020'2020, 0xA3E0'255B'2020'2020,
         0xA3E0'255C'2020'2020, 0xA3E0'255D'2020'2020, 0xA3E0'255E'2020'2020,
         0xA3E0'255F'2020'2020},
        options, 0xA3E0);

    REQUIRE(json_lines.size() == 16);
    // RadioText completes on line 6 (index 5) when terminator is received
    REQUIRE(json_lines[5].is_object());
    REQUIRE(json_lines[5].contains("radiotext"));
    CHECK(json_lines[5]["radiotext"] == "Superstars & Superhits");

    // Other lines shouldn't have RadioText (including subsequent padding groups)
    for (std::size_t i = 0; i < json_lines.size(); i++) {
      REQUIRE(json_lines[i].is_object());
      if (i != 5)
        REQUIRE_FALSE(json_lines[i].contains("radiotext"));
    }
  }

  SECTION("Message changes") {
    const auto json_lines = hex2json(
        {0x6202'2150'4749'524C, 0x6202'2151'5327'2047, 0x6202'2152'454E'4552, 0x6202'2153'4154'494F,
         0x6202'2154'4E20'2D20, 0x6202'2155'5275'6E20, 0x6202'2156'6465'7669, 0x6202'2157'6C20'7275,
         0x6202'2158'6E0D'2020, 0x6202'2140'5061'7061, 0x6202'2141'6E61'6E61, 0x6202'2142'616D'616E,
         0x6202'2143'204B'2D70, 0x6202'2144'6F70'2D73, 0x6202'2145'686F'770D},
        options, 0x6202);

    REQUIRE(json_lines.size() == 15);
    REQUIRE(json_lines[8].is_object());
    REQUIRE(json_lines[8].contains("radiotext"));
    CHECK(json_lines[8]["radiotext"] == "GIRLS' GENERATION - Run devil run");
    REQUIRE(json_lines[14].is_object());
    REQUIRE(json_lines[14].contains("radiotext"));
    CHECK(json_lines[14]["radiotext"] == "Papananaaman K-pop-show");

    // Other lines shouldn't have RadioText
    for (std::size_t i = 0; i < json_lines.size(); i++) {
      REQUIRE(json_lines[i].is_object());
      if (i != 8 && i != 14)
        REQUIRE_FALSE(json_lines[i].contains("radiotext"));
    }
  }

  SECTION("Repeated message") {
    // Radio Nova (fi)
    const auto json_lines =
        hex2json({0x6209'2540'5261'6469, 0x6209'2541'6F20'4E6F, 0x6209'2542'7661'2070,
                  0x6209'2543'6172'6173, 0x6209'2544'2073'656B, 0x6209'2545'6F69'7475,
                  0x6209'2546'7320'6B6C, 0x6209'2547'6173'7369, 0x6209'2548'6B6F'6974,
                  0x6209'2549'6120'6A61, 0x6209'254A'2074'7979, 0x6209'254B'6C69'6B6B,
                  0x6209'254C'6169'6D70, 0x6209'254D'6961'2075, 0x6209'254E'7574'7575,
                  0x6209'254F'6B73'6961, 0x6209'2540'5261'6469, 0x6209'2541'6F20'4E6F,
                  0x6209'2542'7661'2070, 0x6209'2543'6172'6173, 0x6209'2544'2073'656B,
                  0x6209'2545'6F69'7475, 0x6209'2546'7320'6B6C, 0x6209'2547'6173'7369,
                  0x6209'2548'6B6F'6974, 0x6209'2549'6120'6A61, 0x6209'254A'2074'7979,
                  0x6209'254B'6C69'6B6B, 0x6209'254C'6169'6D70, 0x6209'254D'6961'2075,
                  0x6209'254E'7574'7575, 0x6209'254F'6B73'6961},
                 options, 0x6209);

    REQUIRE(json_lines.size() == 32);

    REQUIRE(json_lines[15].is_object());
    REQUIRE(json_lines[15].contains("radiotext"));
    CHECK(json_lines[15]["radiotext"] ==
          "Radio Nova paras sekoitus klassikoita ja tyylikkaimpia uutuuksia");

    REQUIRE(json_lines[31].is_object());
    REQUIRE(json_lines[31].contains("radiotext"));
    CHECK(json_lines[31]["radiotext"] ==
          "Radio Nova paras sekoitus klassikoita ja tyylikkaimpia uutuuksia");

    const auto num_repeats =
        std::count_if(json_lines.begin(), json_lines.end(),
                      [](const auto& line) { return line.contains("radiotext"); });
    CHECK(num_repeats == 2);
  }
}

// TODO Below test do NOT pass
TEST_CASE("Radiotext failing corner cases", "[!mayfail]") {
  redsea::Options options;

  SECTION("TODO: String length method A received out-of-order") {
    options.rbds = true;

    // JACK 96.9 (ca) 2019-05-05
    // clang-format off
    const auto json_lines = hex2json({
      0xC954'24F2'390D'0000,   // "9\r  "
      0xC954'24F0'4A41'434B,  // "JACK"
      0xC954'24F1'2039'362E  // " 96."
    }, options, 0xC954);
    // clang-format on

    REQUIRE(json_lines.size() == 3);
    REQUIRE(json_lines.back().is_object());
    REQUIRE(json_lines.back().contains("radiotext"));
    CHECK(json_lines.back()["radiotext"] == "JACK 96.9");

    // Other lines shouldn't have RadioText
    for (auto prev_line = std::begin(json_lines); prev_line != std::prev(json_lines.end(), 2);
         prev_line++)
      REQUIRE_FALSE(prev_line->contains("radiotext"));
  }
}

TEST_CASE("RDS2 Enhanced RadioText") {
  redsea::Options options;

  // Järviradio (fi)
  // clang-format off
  const auto json_lines = hex2json({
    // eRT ODA identifier
    0x6255'3538'0001'6552,
    // Text data
    0x6255'C520'4AC3'A472,
    0x6255'C521'7669'7261,
    0x6255'C522'6469'6F20,
    0x6255'C523'5244'5332,
    0x6255'C524'2045'5254,
    0x6255'C525'0D0D'0D0D,
  }, options, 0x6255);
  // clang-format on

  REQUIRE_FALSE(json_lines.empty());
  REQUIRE(json_lines.back().is_object());
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
    const auto json_lines = hex2json({
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
    }, options, 0x53C5);
    // clang-format on

    REQUIRE_FALSE(json_lines.empty());
    REQUIRE(json_lines.back().is_object());
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
    const auto json_lines = hex2json({
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
    }, options, 0xD318);
    // clang-format on

    REQUIRE_FALSE(json_lines.empty());
    REQUIRE(json_lines.back().is_object());
    REQUIRE(json_lines.back()["radiotext_plus"].is_object());
    REQUIRE(json_lines.back()["radiotext_plus"]["tags"].is_array());
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
    const auto json_lines = hex2json({
      0x49B1'F180'4272'6565,
      0x49B1'F181'7A65'2031,
      0x49B1'F182'3030'2E36,
      0x49B1'F183'2047'6F6C,
      0x49B1'F184'6420'436F,
      0x49B1'F185'6173'7400,
      0x49B1'F186'0000'0000,
      0x49B1'F187'0000'0000
    }, options, 0x49B1);
    // clang-format on

    REQUIRE_FALSE(json_lines.empty());
    REQUIRE(json_lines.back().is_object());
    REQUIRE(json_lines.back().contains("long_ps"));
    CHECK(json_lines.back()["long_ps"] == "Breeze 100.6 Gold Coast");
  }

  SECTION("String-terminated, contains non-ASCII UTF-8 character") {
    // Järviradio (fi)
    // clang-format off
    const auto json_lines = hex2json({
      0x6255'F520'4AC3'A452,
      0x6255'F521'5649'5241,
      0x6255'F522'4449'4F0D
    }, options, 0x6255);
    // clang-format on

    REQUIRE_FALSE(json_lines.empty());
    REQUIRE(json_lines.back().is_object());
    REQUIRE(json_lines.back().contains("long_ps"));
    CHECK(json_lines.back()["long_ps"] == "JäRVIRADIO");  // sic
  }
}

TEST_CASE("Alternative frequencies") {
  redsea::Options options;

  SECTION("Method A") {
    // YLE Yksi (fi) 2016-09-15
    // clang-format off
    const auto json_lines = hex2json({
      0x6201'00F7'E704'5349,
      0x6201'00F0'2217'594C,
      0x6201'00F1'1139'4520,
      0x6201'00F2'0A14'594B
    }, options, 0x6201);
    // clang-format on

    REQUIRE(json_lines.size() == 4);
    REQUIRE(json_lines.back().is_object());
    REQUIRE(json_lines.back().contains("alt_frequencies_a"));
    CHECK(listEquals(json_lines.back()["alt_frequencies_a"],
                     {87'900, 90'900, 89'800, 89'200, 93'200, 88'500, 89'500}));
  }

  SECTION("Method B") {
    // YLE Helsinki (fi) 2016-09-15
    // clang-format off
    const auto json_lines = hex2json({
      0x6403'0447'F741'4920, 0x6403'0440'415F'594C, 0x6403'0441'4441'4520, 0x6403'0442'5541'484B,
      0x6403'0447'1C41'4920, 0x6403'0440'6841'594C, 0x6403'0441'5E41'4520, 0x6403'0442'414B'484B,
      0x6403'0447'4156'4920, 0x6403'0440'CB41'594C, 0x6403'0441'B741'4520, 0x6403'0442'4174'484B
    }, options, 0x6403);
    // clang-format on

    REQUIRE(json_lines.size() == 12);
    REQUIRE(json_lines.back().is_object());
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
    const auto json_lines = hex2json({
      0x6403'0447'F741'4920, 0x6403'0440'415F'594C, 0x6403'0441'4441'4520, 0x6403'0442'5541'484B
    }, options, 0x6403);
    // clang-format on

    REQUIRE(json_lines.size() == 4);
    REQUIRE(json_lines[0].is_object());
    REQUIRE(json_lines[0].contains("partial_alt_frequencies"));
    CHECK(listEquals(json_lines[0]["partial_alt_frequencies"], {94'000}));
    REQUIRE(json_lines[1].contains("partial_alt_frequencies"));
  }
}

TEST_CASE("Clock-time and date") {
  redsea::Options options;

  SECTION("During DST") {
    // BR-KLASSIK (de) 2017-04-04
    const auto json_lines = hex2json({0xD314'41C1'C3EF'5AC4}, options, 0xD314);

    REQUIRE(json_lines.size() == 1);
    REQUIRE(json_lines.back().is_object());
    REQUIRE(json_lines.back().contains("clock_time"));
    CHECK(json_lines.back()["clock_time"] == "2017-04-04T23:43:00+02:00");
  }

  SECTION("Outside of DST") {
    // 104.6RTL (de) 2018-11-01
    // walczakp/rds-spy-logs/Germany/D42A - 2018-11-01 14-17-16 DE BER RTL104_6.rds
    const auto json_lines = hex2json({0xD42A'4541'C86E'D482}, options, 0xD42A);

    REQUIRE(json_lines.size() == 1);
    REQUIRE(json_lines.back().is_object());
    REQUIRE(json_lines.back().contains("clock_time"));
    CHECK(json_lines.back()["clock_time"] == "2018-11-01T14:18:00+01:00");
  }

  SECTION("Negative UTC offset") {
    options.rbds = true;

    // 98.5 KFOX (KUFX) (us) 2020-08-19
    // walczakp/rds-spy-logs/USA/4569 - 2020-08-19 20-45-06.spy
    const auto json_lines = hex2json({0x4569'40DD'CD92'3BAE}, options, 0x4569);

    REQUIRE(json_lines.size() == 1);
    REQUIRE(json_lines.back().is_object());
    REQUIRE(json_lines.back().contains("clock_time"));
    CHECK(json_lines.back()["clock_time"] == "2020-08-19T20:46:00-07:00");
  }

  SECTION("Zero UTC offset") {
    // Vikerraadio (ee) 2016-07-18 (though ee is not actually UTC+0)
    const auto json_lines = hex2json({0x22E1'4581'C1E7'4280}, options, 0x22E1);

    REQUIRE(json_lines.size() == 1);
    REQUIRE(json_lines.back().is_object());
    REQUIRE(json_lines.back().contains("clock_time"));
    CHECK(json_lines.back()["clock_time"] == "2016-07-18T20:10:00Z");
  }

  SECTION("Across local midnight") {
    // https://github.com/windytan/redsea/issues/83
    // clang-format off
    const auto json_lines = hex2json({
      0xF201'441D'D299'5EC4,
      0xF201'441D'D299'6004
    }, options, 0xF201);
    // clang-format on

    REQUIRE(json_lines.size() == 2);
    REQUIRE(json_lines[0].is_object());
    REQUIRE(json_lines[1].is_object());
    REQUIRE(json_lines[0].contains("clock_time"));
    REQUIRE(json_lines[1].contains("clock_time"));
    CHECK(json_lines[0]["clock_time"] == "2022-05-25T23:59:00+02:00");
    CHECK(json_lines[1]["clock_time"] == "2022-05-26T00:00:00+02:00");
  }

  SECTION("Across UTC midnight") {
    // https://github.com/windytan/redsea/issues/83
    // clang-format off
    const auto json_lines = hex2json({
      0xF201'441D'D299'7EC4,
      0xF201'441D'D29A'0004
    }, options, 0xF201);
    // clang-format on

    REQUIRE(json_lines.size() == 2);
    REQUIRE(json_lines[0].is_object());
    REQUIRE(json_lines[1].is_object());
    REQUIRE(json_lines[0].contains("clock_time"));
    REQUIRE(json_lines[1].contains("clock_time"));
    CHECK(json_lines[0]["clock_time"] == "2022-05-26T01:59:00+02:00");
    CHECK(json_lines[1]["clock_time"] == "2022-05-26T02:00:00+02:00");
  }

  SECTION("Invalid MJD is handled cleanly") {
    // Integer underflow fixed in 1.0.0
    const auto json_lines = hex2json(
        {
            0xD314'41C0'7530'5AC4  // MJD = 15000
        },
        options, 0xD314);

    REQUIRE(json_lines.size() == 1);
    REQUIRE(json_lines.back().is_object());
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
  const auto json_lines = hex2json(
      {0x83D2'5540'00C8'006D, 0x83D2'5541'FF00'0000, 0x83D2'5542'00E2'00E3, 0x83D2'5543'00C8'00E0,
       0x83D2'5544'00DE'00D8, 0x83D2'5545'00DF'00E4, 0x83D2'5546'5452'4453, 0x83D2'5547'3430'3031,
       0x83D2'5548'2052'656C, 0x83D2'5549'6561'7365, 0x83D2'554A'2030'3230, 0x83D2'554B'3130'3930,
       0x83D2'554C'3020'3136, 0x83D2'554D'2F30'362F, 0x83D2'554E'3230'3033, 0x83D2'554F'202D'2052,
       0x83D2'5550'5652'2045, 0x83D2'5551'6C65'7474, 0x83D2'5552'726F'6E69, 0x83D2'5553'6361'2053,
       0x83D2'5554'7061'0037, 0x83D2'5555'0020'2037, 0x83D2'5556'0020'2037, 0x83D2'5557'0020'2020,
       0x83D2'5558'2020'2020, 0x83D2'5559'2020'2020, 0x83D2'555A'2020'2020, 0x83D2'555B'2020'2020,
       0x83D2'555C'2020'2020, 0x83D2'555D'2020'2020, 0x83D2'555E'2020'2020, 0x83D2'555F'2053'20AC},
      options, 0x83D2);

  REQUIRE(json_lines.size() == 32);
  REQUIRE(json_lines.back().is_object());
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
  const auto json_lines = hex2json({
    0xD314'61C0'AFFE'AFFE,
    0xD314'61C1'D100'0A19,
    0xD314'61C2'0000'0B01,
    0xD314'61C3'2005'2015,
    0xD314'61DF'0000'D314},
  options, 0xD314);
  // clang-format on

  REQUIRE(json_lines.size() == 5);
  for (const auto& line : json_lines) {
    REQUIRE(line.is_object());
    REQUIRE(line.contains("in_house_data"));
    REQUIRE(line["in_house_data"].is_array());
  }
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
    const auto json_lines = hex2json({
      0x6202'E150'594C'6203,
      0x6202'E151'4553'6203,
      0x6202'E152'554F'6203,
      0x6202'E153'4D49'6203,
      0x6202'E155'2C41'6203,
      0x6202'E15C'0000'6203,
      0x6202'E15D'4800'6203,
      0x6202'E15E'7C83'6203},
    options, 0x6202);
    // clang-format on

    REQUIRE(json_lines.size() == 8);
    for (const auto& line : json_lines) {
      REQUIRE(line.is_object());
    }
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
    const auto json_lines = hex2json({0xD220'EA90'D220'D313}, options, 0xD220);

    REQUIRE_FALSE(json_lines.empty());
    REQUIRE(json_lines.back().is_object());
    REQUIRE(json_lines.back().contains("pi"));
    REQUIRE(json_lines.back().contains("other_network"));
    CHECK(json_lines.back()["pi"] == "0xD220");

    // Refers to Bayern 3
    REQUIRE(json_lines.back()["other_network"].is_object());
    REQUIRE(json_lines.back()["other_network"].contains("pi"));
    REQUIRE(json_lines.back()["other_network"].contains("tp"));
    REQUIRE(json_lines.back()["other_network"].contains("ta"));
    CHECK(json_lines.back()["other_network"]["pi"] == "0xD313");
    CHECK(json_lines.back()["other_network"]["tp"] == true);
    CHECK(json_lines.back()["other_network"]["ta"] == false);
  }

  SECTION("Alt frequencies") {
    // Radio Gioconda (it)
    const auto json_lines =
        hex2json({0x53C5'E554'E2AD'53C6, 0x53C5'E554'C2CD'53C6}, options, 0x53C5);

    REQUIRE(json_lines.size() == 2);
    REQUIRE(json_lines.back().is_object());
    REQUIRE(json_lines.back().contains("other_network"));
    REQUIRE(json_lines.back()["other_network"].is_object());
    REQUIRE(json_lines.back()["other_network"].contains("alt_frequencies"));
    REQUIRE(json_lines.back()["other_network"]["alt_frequencies"].is_array());
    CHECK(listEquals(json_lines.back()["other_network"]["alt_frequencies"], {104'800, 106'900}));
  }
}

TEST_CASE("DAB cross-referencing") {
  redsea::Options options;
  // BBC Radio 4 (gb) 2015-09-27
  // walczakp/rds-spy-logs/UK/C204 - 2015-09-27 23-35-46 UK NRW BBC4.rds
  const auto json_lines = hex2json({0xC204'3138'0000'0093, 0xC204'C124'3717'CE15}, options, 0xC204);

  REQUIRE(json_lines.size() == 2);

  // Source: https://www.bbc.co.uk/programmes/articles/98FthRzhxJ4z0fXYJnsvlM/about-radio-4
  CHECK(json_lines.back()["dab"]["channel"] == "12B");
  CHECK(json_lines.back()["dab"]["kilohertz"] == 225'648);
}

TEST_CASE("Unspecified ODA") {
  redsea::Options options;

  // WDR 5 (de) 2019-05-05
  // walczakp/rds-spy-logs/Germany/D395 - 2019-05-05 09-46-23.spy
  const auto json_lines = hex2json({0xD395'B065'279A'0020}, options, 0xD395);

  REQUIRE(json_lines.size() == 1);
  REQUIRE(json_lines.back().is_object());
  CHECK(json_lines.back()["group"] == "11A");
  REQUIRE(json_lines.back().contains("unknown_oda"));
  REQUIRE(json_lines.back()["unknown_oda"].is_object());
  CHECK(json_lines.back()["unknown_oda"]["raw_data"] == "05 279A 0020");
}

TEST_CASE("Block error rate (BLER) reporting") {
  redsea::Options options;

  SECTION("Disabled") {
    options.bler = false;
    // clang-format off
    const auto json_lines = hex2json({
      0x7827'0000'7827'F928
    }, options, 0x7827, DeleteOneBlock::Block2);
    // clang-format on

    REQUIRE_FALSE(json_lines.empty());
    REQUIRE(json_lines.back().is_object());
    REQUIRE_FALSE(json_lines.back().contains("bler"));
  }

  SECTION("Enabled") {
    options.bler = true;

    constexpr int num_erroneous_blocks = 1;

    // clang-format off
    const auto json_lines = hex2json({
      0x7827'0000'7827'F928
    }, options, 0x7827, DeleteOneBlock::Block2);
    // clang-format on

    REQUIRE_FALSE(json_lines.empty());
    REQUIRE(json_lines.back().is_object());
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
    const auto json_lines = hex2json({
      0x7827'F928'7827'F928
    }, options, 0x7827);
    // clang-format on

    REQUIRE_FALSE(json_lines.empty());
    REQUIRE(json_lines.back().is_object());
    REQUIRE_FALSE(json_lines.back().contains("rx_time"));
  }

  SECTION("Enabled") {
    options.timestamp = true;

    // clang-format off
    const auto json_lines = hex2json({
      0x7827'F928'7827'F928
    }, options, 0x7827);
    // clang-format on

    REQUIRE_FALSE(json_lines.empty());
    REQUIRE(json_lines.back().is_object());
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
  const auto json_lines = hex2json({
    0x7827'F928'7827'F928
  }, options, 0x7827, DeleteOneBlock::Block2);
  // clang-format on

  REQUIRE_FALSE(json_lines.empty());
  REQUIRE(json_lines.back().is_object());
  CHECK(json_lines.back()["raw_data"] == "7827 ---- 7827 F928");
};
