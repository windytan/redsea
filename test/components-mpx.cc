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

  std::vector<nlohmann::ordered_json> json;

  redsea::MPXReader mpx;
  mpx.init(options);
  options.samplerate   = mpx.getSamplerate();
  options.num_channels = mpx.getNumChannels();

  std::stringstream json_stream;
  redsea::Channel channel(options, 0, json_stream);
  redsea::SubcarrierSet subcarriers(options.samplerate);

  while (!mpx.eof()) {
    mpx.fillBuffer();
    const auto bits = subcarriers.processChunk(mpx.readChunk(0), 1);

    for (const auto& bit : bits.bits[0]) {
      channel.processBit(bit.value, 0);
      if (!json_stream.str().empty()) {
        nlohmann::ordered_json jsonroot;
        json_stream >> jsonroot;
        json.push_back(jsonroot);

        json_stream.str("");
        json_stream.clear();
      }
    }
  }

  CHECK(json.size() == 2);
  CHECK(json.at(0)["pi"] == "0x6201");
  CHECK(json.at(0)["prog_type"] == "Serious classical");
}

TEST_CASE("RDS2/RFT station logo from MPX") {
  redsea::Options options;

  options.sndfilename = "../test/resources/rds2-minirds-192k.flac";
  options.input_type  = redsea::InputType::MPX_sndfile;
  options.streams     = true;

  redsea::MPXReader mpx;
  mpx.init(options);
  options.samplerate   = mpx.getSamplerate();
  options.num_channels = mpx.getNumChannels();

  std::stringstream json_stream;
  redsea::Channel channel(options, 0, json_stream);
  redsea::SubcarrierSet subcarriers(options.samplerate);

  constexpr int num_streams = 4;

  while (!mpx.eof()) {
    mpx.fillBuffer();
    const auto bits = subcarriers.processChunk(mpx.readChunk(0), num_streams);

    channel.processBits(bits);
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
          return;
        }
      }

      json_stream.str("");
      json_stream.clear();
    }
  }

  FAIL();
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
  redsea::Channel channel(options, 0, json_stream);
  redsea::SubcarrierSet subcarriers(options.samplerate);

  // One array element for each stream
  constexpr int n_streams{4};
  std::array<std::vector<double>, n_streams> seen_timestamps{};

  while (!mpx.eof()) {
    mpx.fillBuffer();
    const auto bits = subcarriers.processChunk(mpx.readChunk(0), n_streams);

    channel.processBits(bits);
    if (!json_stream.str().empty()) {
      nlohmann::ordered_json jsonroot;
      // Process each newline-terminated line one-by-one
      std::string line;
      while (std::getline(json_stream, line)) {
        std::istringstream(line) >> jsonroot;

        REQUIRE(jsonroot.contains("time_from_start"));
        REQUIRE(jsonroot.contains("stream"));
        REQUIRE(jsonroot["stream"].is_number());
        REQUIRE(jsonroot["stream"].get<int>() < n_streams);
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

  /* If you demodulate stream 0 of the test file, with a zero-delay demodulator (outside of redsea),
     you can then visually get the following timestamps for reference:
    0.018208,1000 0008 E0CD 4D69
    0.105786,1000 2003 7477 6172
    0.193365,1000 0009 E0CD 6E69
    0.280943,1000 2004 6520 5244
    0.368521,1000 000A E0CD 5244
    0.456104,1000 2005 5320 656E
    0.543682,1000 3018 0001 6552
    ....
  */

  for (int n_stream{}; n_stream < n_streams; n_stream++) {
    double previous_timestamp{};
    int n_timestamp{};

    for (double timestamp : seen_timestamps[n_stream]) {
      // The first timestamps are always a little wobbly due to clock synchronization
      if (n_timestamp > 1) {
        // Redundant, but for clarity
        CHECK(timestamp > previous_timestamp);

        // Groups should last 104 bits +/- 200 μs
        REQUIRE_THAT(timestamp - previous_timestamp,
                     Catch::Matchers::WithinAbs(kGroupDuration, 2e-4));

        // The general offset of all groups in this test file
        // (It was only properly measured for stream 0 and the streams are offset from each other)
        const double offset_margin_seconds = n_stream == 0 ? 0.0002 : 0.0016;
        REQUIRE_THAT(std::fmod(timestamp, kGroupDuration),
                     Catch::Matchers::WithinAbs(0.018208, offset_margin_seconds));
      }
      previous_timestamp = timestamp;
      n_timestamp++;
    }
  }
}
