#ifndef TEST_HELPERS_H_
#define TEST_HELPERS_H_

#include <nlohmann/json.hpp>

#include "../src/block_sync.h"
#include "../src/channel.h"
#include "../src/common.h"
#include "../src/groups.h"
#include "../src/options.h"

#include <vector>

using HexData    = std::vector<uint64_t>;
using BinaryData = std::vector<uint32_t>;

// Convert synchronized hex data into groups. Error correction is omitted and ignored.
inline std::vector<redsea::Group> hex2groups(const HexData& hexdata) {
  std::vector<redsea::Group> groups;
  groups.reserve(hexdata.size());

  for (const auto& hexgroup : hexdata) {
    redsea::Group group;
    group.disableOffsets();
    for (auto nblock : {redsea::BLOCK1, redsea::BLOCK2, redsea::BLOCK3, redsea::BLOCK4}) {
      redsea::Block block;
      block.data        = hexgroup >> (16 * (3 - static_cast<int>(nblock))) & 0xFFFF;
      block.is_received = true;
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

    channel.processBit(bit);
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
  redsea::BlockStream block_stream(options);

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
                                                       uint16_t pi) {
  std::vector<nlohmann::ordered_json> result;

  std::stringstream json_stream;
  redsea::Channel channel(options, json_stream, pi);
  for (const auto& group : data) {
    json_stream.str("");
    json_stream.clear();
    channel.processGroup(group);
    if (!json_stream.str().empty()) {
      nlohmann::ordered_json jsonroot;
      json_stream >> jsonroot;
      result.push_back(jsonroot);
    }
  }

  return result;
}

// Convert synchronized hex data (without offset words) into JSON.
inline std::vector<nlohmann::ordered_json> hex2json(const HexData& hexdata,
                                                    const redsea::Options& options, uint16_t pi) {
  return groups2json(hex2groups(hexdata), options, pi);
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
