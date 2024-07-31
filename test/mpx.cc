#include <sstream>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include "../src/dsp/subcarrier.h"
#include "../src/input.h"
#include "../src/options.h"
#include "test_helpers.h"

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
  redsea::Subcarrier subcarrier(options);

  while (!mpx.eof()) {
    mpx.fillBuffer();
    const auto bits = subcarrier.processChunk(mpx.readChunk(0));

    for (const auto& bit : bits.bits) {
      channel.processBit(bit);
      if (!json_stream.str().empty()) {
        nlohmann::ordered_json jsonroot;
        json_stream >> jsonroot;
        json.push_back(jsonroot);

        json_stream.str("");
        json_stream.clear();
      }
    }
  }

  CHECK(json.size() == 1);
  CHECK(json.at(0)["pi"] == "0x6201");
  CHECK(json.at(0)["prog_type"] == "Serious classical");
}
