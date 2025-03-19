#ifndef TEST_HELPERS_H_
#define TEST_HELPERS_H_

#include <nlohmann/json.hpp>

#include "../src/block_sync.hh"
#include "../src/channel.hh"
#include "../src/constants.hh"
#include "../src/groups.hh"
#include "../src/options.hh"

#include <cstdint>
#include <initializer_list>
#include <vector>

using HexInputData = std::initializer_list<std::uint64_t>;

enum class DeleteOneBlock { Block1 = 0, Block2, Block3, Block4, None };

// Convert synchronized hex data into groups. Error correction is omitted and ignored.
// \param block_to_delete Simulate losing some block to noise (same block in every group)
inline std::vector<redsea::Group> hex2groups(const HexInputData& input_data,
                                             DeleteOneBlock block_to_delete) {
  std::vector<redsea::Group> groups;
  groups.reserve(input_data.size());

  for (const auto& hexgroup : input_data) {
    redsea::Group group;
    group.disableOffsets();
    for (auto nblock : {redsea::BLOCK1, redsea::BLOCK2, redsea::BLOCK3, redsea::BLOCK4}) {
      redsea::Block block;
      block.data        = hexgroup >> (16 * (3 - static_cast<int>(nblock))) & 0xFFFF;
      block.is_received = static_cast<int>(nblock) != static_cast<int>(block_to_delete);
      group.setBlock(nblock, block);
    }
    groups.push_back(group);
  }

  return groups;
}

// Convert string of unsynchronized ASCII bits into JSON.
inline std::vector<nlohmann::ordered_json> asciibin2json(const std::string& bindata,
                                                         const redsea::Options& options) {
  std::vector<nlohmann::ordered_json> result;

  std::stringstream json_stream;
  redsea::Channel channel(options, 0, json_stream);

  for (const auto& ascii_bit : bindata) {
    const int bit{ascii_bit == '1' ? 1 : 0};

    channel.processBit(bit, 0);
    if (!json_stream.str().empty()) {
      nlohmann::ordered_json jsonroot;
      json_stream >> jsonroot;
      result.push_back(jsonroot);

      json_stream.str("");
      json_stream.clear();
    }
  }

  return result;
}

// Convert string of unsynchronized ASCII bits into groups.
inline std::vector<redsea::Group> asciibin2groups(const std::string& bindata,
                                                  const redsea::Options& options) {
  std::vector<redsea::Group> result;
  redsea::BlockStream block_stream;
  block_stream.init(options);

  for (const auto& ascii_bit : bindata) {
    const int bit{ascii_bit == '1' ? 1 : 0};

    block_stream.pushBit(bit);
    if (block_stream.hasGroupReady()) {
      result.push_back(block_stream.popGroup());
    }
  }

  return result;
}

// Run redsea's full decoder and convert the ASCII JSON output back into JSON objects.
inline std::vector<nlohmann::ordered_json> groups2json(const std::vector<redsea::Group>& data,
                                                       const redsea::Options& options,
                                                       std::uint16_t pi) {
  std::vector<nlohmann::ordered_json> result;

  std::stringstream json_stream;
  redsea::Channel channel(options, json_stream, pi);
  for (const auto& group : data) {
    json_stream.str("");
    json_stream.clear();
    channel.processGroup(group, 0);
    if (!json_stream.str().empty()) {
      nlohmann::ordered_json jsonroot;
      json_stream >> jsonroot;
      result.push_back(jsonroot);
    }
  }

  return result;
}

// Convert synchronized hex data (without offset words) into JSON.
inline std::vector<nlohmann::ordered_json> hex2json(
    const HexInputData& input_data, const redsea::Options& options, std::uint16_t pi,
    DeleteOneBlock block_to_delete = DeleteOneBlock::None) {
  return groups2json(hex2groups(input_data, block_to_delete), options, pi);
}

template <typename T>
bool listEquals(nlohmann::ordered_json json, std::initializer_list<T> list) {
  if (json.size() != list.size())
    return false;

  auto json_it = json.begin();
  auto list_it = list.begin();
  while (json_it != json.end()) {
    if (*json_it != *list_it)
      return false;
    json_it++;
    list_it++;
  }
  return true;
}

// Flip a bit in a string of ASCII bits.
void flipAsciiBit(std::string& str, size_t bit_index) {
  str[bit_index] = str[bit_index] == '0' ? '1' : '0';
}

#endif  // TEST_HELPERS_H_
