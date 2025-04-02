#include <sstream>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include "../src/dsp/subcarrier.hh"
#include "../src/io/input.hh"
#include "../src/options.hh"
#include "test_helpers.hh"

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
      channel.processBit(bit, 0);
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

  const int num_streams = 4;

  while (!mpx.eof()) {
    mpx.fillBuffer();
    const auto bits = subcarriers.processChunk(mpx.readChunk(0), num_streams);

    for (int n_stream = 0; n_stream < num_streams; n_stream++) {
      channel.processBits(bits, n_stream);
      if (!json_stream.str().empty()) {
        nlohmann::ordered_json jsonroot;
        json_stream >> jsonroot;

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

        json_stream.str("");
        json_stream.clear();
      }
    }
  }

  FAIL();
}
