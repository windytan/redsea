// Redsea tests: Component tests that read MPX files

#include <sstream>
#include <vector>

#include <nlohmann/json.hpp>

#include "../src/channel.hh"
#include "../src/dsp/subcarrier.hh"
#include "../src/io/input.hh"
#include "../src/options.hh"

// Both Catch2 and liquid define a macro called DEPRECATED
#ifdef DEPRECATED
#pragma push_macro("DEPRECATED")
#undef DEPRECATED
#endif

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#ifdef DEPRECATED
#pragma pop_macro("DEPRECATED")
#endif

TEST_CASE("MPX file input") {
  redsea::Options options;

  options.sndfilename = "../test/resources/mpx-testfile-yksi.flac";
  options.input_type  = redsea::InputType::MPX_sndfile;

  redsea::MPXReader mpx;
  mpx.init(options);
  options.samplerate   = mpx.getSamplerate();
  options.num_channels = mpx.getNumChannels();

  redsea::SubcarrierSet subcarriers(options.samplerate);

  std::stringstream output_stream;

  SECTION("To hex") {
    options.output_type = redsea::OutputType::Hex;
    redsea::Channel channel(options, 0);

    bool success{};

    while (!mpx.eof()) {
      const auto bits = subcarriers.chunkToBits(mpx.readChunk(0), 1);

      channel.processBits(bits, output_stream);

      if (!output_stream.str().empty()) {
        SUCCEED();
        success = true;
        break;
      }
    }

    if (!success) {
      FAIL();
    }
  }

  SECTION("To JSON") {
    redsea::Channel channel(options, 0);

    std::vector<nlohmann::ordered_json> json;

    while (!mpx.eof()) {
      const auto bits = subcarriers.chunkToBits(mpx.readChunk(0), 1);

      channel.processBits(bits, output_stream);

      if (!output_stream.str().empty()) {
        nlohmann::ordered_json jsonroot;
        output_stream >> jsonroot;
        json.push_back(jsonroot);

        output_stream.str("");
        output_stream.clear();
      }
    }

    CHECK(json.size() == 2);
    CHECK(json.at(0)["pi"] == "0x6201");
    CHECK(json.at(0)["prog_type"] == "Serious classical");
  }

  SECTION("Timestamp") {
    options.timestamp = true;
    redsea::Channel channel(options, 0);

    std::vector<nlohmann::ordered_json> json;

    while (!mpx.eof()) {
      const auto bits = subcarriers.chunkToBits(mpx.readChunk(0), 1);
      channel.processBits(bits, output_stream);

      if (!output_stream.str().empty()) {
        nlohmann::ordered_json jsonroot;
        output_stream >> jsonroot;
        json.push_back(jsonroot);

        output_stream.str("");
        output_stream.clear();
      }
    }

    REQUIRE(!json.empty());
    REQUIRE(json.back().contains("rx_time"));

    // There's not much else we can confidently test here without mocking system_clock
  }
}

TEST_CASE("RDS2/RFT station logo from MPX") {
  redsea::Options options;

  // This test signal was generated with Anthony96922's MiniRDS
  // https://github.com/Anthony96922/MiniRDS

  options.sndfilename = "../test/resources/rds2-minirds-192k.flac";
  options.input_type  = redsea::InputType::MPX_sndfile;
  options.streams     = true;

  redsea::MPXReader mpx;
  mpx.init(options);
  options.samplerate   = mpx.getSamplerate();
  options.num_channels = mpx.getNumChannels();

  std::stringstream json_stream;
  redsea::Channel channel(options, 0);
  redsea::SubcarrierSet subcarriers(options.samplerate);

  constexpr int num_streams = 4;
  bool success{};

  while (!mpx.eof()) {
    const auto bits = subcarriers.chunkToBits(mpx.readChunk(0), num_streams);

    channel.processBits(bits, json_stream);
    if (!json_stream.str().empty()) {
      nlohmann::ordered_json jsonroot;
      std::string line;
      while (std::getline(json_stream, line)) {
        std::istringstream(line) >> jsonroot;
        if (jsonroot.contains("rft") && jsonroot["rft"].contains("data") &&
            jsonroot["rft"]["data"].contains("file_contents")) {
          CHECK(jsonroot["rft"]["data"]["file_contents"] ==
                "iVBORw0KGgoAAAANSUhEUgAAAGgAAABoAgMAAABgcl0yAAAADFBMVEUAAADtHCT/"
                "rsn////OXmVkAAAAXElEQVRIx+3WOQ7AMAhE0X/JuSSXJIWlODZxn0GZCunRspDH"
                "0ICAWtkTSBI8K38CKSJCGnC3taGZLrTnp+9SxTKVprRi2aLGNBHK1bOmgfB6za0p"
                "E45fijU1+g8vzrC0S1+S2vwAAAAASUVORK5CYII=");
          SUCCEED();
          success = true;
          break;
        }
      }

      json_stream.str("");
      json_stream.clear();
    }
  }

  if (!success) {
    FAIL();
  }
}

TEST_CASE("Time from start") {
  redsea::Options options;

  options.sndfilename     = "../test/resources/rds2-minirds-192k.flac";
  options.input_type      = redsea::InputType::MPX_sndfile;
  options.time_from_start = true;
  options.streams         = true;

  redsea::MPXReader mpx;
  mpx.init(options);
  options.samplerate   = mpx.getSamplerate();
  options.num_channels = mpx.getNumChannels();

  std::stringstream json_stream;
  redsea::Channel channel(options, 0);
  redsea::SubcarrierSet subcarriers(options.samplerate);

  // One array element for each stream
  constexpr int kNStreams{4};
  std::array<std::vector<double>, kNStreams> seen_timestamps{};

  while (!mpx.eof()) {
    const auto bits = subcarriers.chunkToBits(mpx.readChunk(0), kNStreams);

    channel.processBits(bits, json_stream);
    if (!json_stream.str().empty()) {
      nlohmann::ordered_json jsonroot;
      // Process each newline-terminated line one-by-one
      std::string line;
      while (std::getline(json_stream, line)) {
        std::istringstream(line) >> jsonroot;

        REQUIRE(jsonroot.contains("time_from_start"));
        REQUIRE(jsonroot.contains("stream"));
        REQUIRE(jsonroot["stream"].is_number());
        REQUIRE(jsonroot["stream"].get<int>() < kNStreams);
        seen_timestamps[jsonroot["stream"].get<int>()].push_back(jsonroot["time_from_start"]);
      }

      json_stream.str("");
      json_stream.clear();
    }
  }

  // Loop unwound to clarify the possible failure message
  REQUIRE(seen_timestamps[0].size() > 1);
  REQUIRE(seen_timestamps[1].size() > 1);
  REQUIRE(seen_timestamps[2].size() > 1);
  REQUIRE(seen_timestamps[3].size() > 1);

  constexpr double kBitDuration   = 1 / 1187.5;
  constexpr double kGroupDuration = 104 * kBitDuration;

  // We used a simple BPSK demodulator to measure this offset in the FLAC file
  // (It's where the first group starts in stream 0)
  constexpr double kOverallShiftInTestFile_ms = 18.208;

  constexpr double kGroupDurationMargin_ms            = 0.2;
  constexpr double kAbsoluteTimeStream0Margin_ms      = 0.2;
  constexpr double kAbsoluteTimeOtherStreamsMargin_ms = 1.6;

  /* Measured manually in rds2-minirds-192k.flac:
    0.018208,1000 0008 E0CD 4D69
    0.105786,1000 2003 7477 6172
    0.193365,1000 0009 E0CD 6E69
    0.280943,1000 2004 6520 5244
    0.368521,1000 000A E0CD 5244
    0.456104,1000 2005 5320 656E
    0.543682,1000 3018 0001 6552
    ....
  */

  for (int n_stream{}; n_stream < kNStreams; n_stream++) {
    double previous_timestamp{};
    int n_timestamp{};

    for (double timestamp : seen_timestamps[n_stream]) {
      // The first timestamps are always a little wobbly due to clock synchronization
      if (n_timestamp > 1) {
        // Redundant, but for clarity
        CHECK(timestamp > previous_timestamp);

        // Groups should last 104 bits +/- 200 μs
        REQUIRE_THAT(timestamp - previous_timestamp,
                     Catch::Matchers::WithinAbs(kGroupDuration, kGroupDurationMargin_ms * 1e-3));

        // The general offset of all groups in this test file
        // (It was only properly measured for stream 0 and the streams are offset from each other)
        const double offset_margin_seconds = n_stream == 0
                                                 ? kAbsoluteTimeStream0Margin_ms * 1e-3
                                                 : kAbsoluteTimeOtherStreamsMargin_ms * 1e-3;
        REQUIRE_THAT(
            std::fmod(timestamp, kGroupDuration),
            Catch::Matchers::WithinAbs(kOverallShiftInTestFile_ms * 1e-3, offset_margin_seconds));
      }
      previous_timestamp = timestamp;
      n_timestamp++;
    }
  }
}
