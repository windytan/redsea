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
inline std::vector<redsea::Group> makeGroupsFromHex(const HexData& hexdata) {
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

inline std::vector<nlohmann::ordered_json> decodeBinary(const BinaryData& bindata,
                                                        const redsea::Options& options) {
  std::vector<nlohmann::ordered_json> result;

  std::stringstream json_stream;
  redsea::Channel channel(options, 0, json_stream);

  for (const auto& word : bindata) {
    constexpr auto wordsize_bits{sizeof(word) * 8};

    for (size_t nbit{}; nbit < wordsize_bits; nbit++) {
      int bit = (word >> (wordsize_bits - 1 - nbit)) & 0b1;
      channel.processBit(bit);
      if (!json_stream.str().empty()) {
        nlohmann::ordered_json jsonroot;
        json_stream >> jsonroot;
        result.push_back(jsonroot);

        json_stream.str("");
        json_stream.clear();
      }
    }
  }

  return result;
}

// Run redsea's decoder and convert the ascii json output into json objects.
inline std::vector<nlohmann::ordered_json> decodeGroups(const std::vector<redsea::Group>& data,
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

inline std::vector<nlohmann::ordered_json> decodeGroups(const HexData& hexdata,
                                                        const redsea::Options& options,
                                                        uint16_t pi) {
  return decodeGroups(makeGroupsFromHex(hexdata), options, pi);
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

#endif  // TEST_HELPERS_H_
