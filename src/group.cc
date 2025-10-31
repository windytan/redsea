#include "src/group.hh"

#include <chrono>
#include <cstdint>
#include <numeric>
#include <string>

#include "src/util/maybe.hh"
#include "src/util/util.hh"

namespace redsea {

GroupType::GroupType(std::uint16_t type_code)
    : number(static_cast<std::uint16_t>(type_code >> 1U) & 0xFU),
      version((type_code & 0x1U) == 0 ? GroupType::Version::A : GroupType::Version::B) {}

std::string GroupType::str() const {
  if (version == Version::C)
    return "C";
  return std::to_string(number) + (version == Version::A ? "A" : "B");
}

bool operator==(const GroupType& type1, const GroupType& type2) {
  return type1.number == type2.number && type1.version == type2.version;
}

bool operator<(const GroupType& type1, const GroupType& type2) {
  return (type1.number < type2.number) ||
         (type1.number == type2.number && type1.version < type2.version);
}

std::uint16_t Group::get(eBlockNumber block_num) const {
  return blocks_[block_num].data;
}

bool Group::has(eBlockNumber block_num) const {
  return blocks_[block_num].is_received;
}

bool Group::isEmpty() const {
  return !(has(BLOCK1) || has(BLOCK2) || has(BLOCK3) || has(BLOCK4));
}

// Remember to check if hasPI()
std::uint16_t Group::getPI() const {
  if (blocks_[BLOCK1].is_received)
    return blocks_[BLOCK1].data;
  else if (blocks_[BLOCK3].is_received && blocks_[BLOCK3].offset == Offset::Cprime)
    return blocks_[BLOCK3].data;
  else
    return 0x0000;
}

// \return Block error rate, percent
Maybe<float> Group::getBLER() const {
  return bler_;
}

int Group::getNumErrors() const {
  return std::accumulate(blocks_.cbegin(), blocks_.cend(), 0, [](int a, Block b) {
    return a + ((b.had_errors || !b.is_received) ? 1 : 0);
  });
}

Maybe<double> Group::getTimeFromStart() const {
  return time_from_start_;
}

bool Group::hasPI() const {
  return type_.value.version != GroupType::Version::C &&
         (blocks_[BLOCK1].is_received ||
          (blocks_[BLOCK3].is_received && blocks_[BLOCK3].offset == Offset::Cprime));
}

Maybe<GroupType> Group::getType() const {
  return type_;
}

Maybe<std::chrono::time_point<std::chrono::system_clock>> Group::getRxTime() const {
  return time_received_;
}

// Don't expect the C' offset for version B groups (e.g. hex input)
void Group::disableOffsets() {
  no_offsets_ = true;
}

// Group is version C (RDS2 extra data streams)
void Group::setVersionC() {
  type_ = makeGroupTypeC();
}

void Group::setDataStream(std::uint32_t stream) {
  data_stream_ = stream;
}

std::uint32_t Group::getDataStream() const {
  return data_stream_;
}

void Group::setBlock(eBlockNumber block_num, Block block) {
  blocks_[block_num] = block;

  // Try to find out the group type if unknown so far
  if (!type_.has_value) {
    if (block_num == BLOCK2) {
      type_ = GroupType(getBits<5>(block.data, 11));
      if (type_.value.version == GroupType::Version::B) {
        // Type is deferred unless blocks were received out-of-order: C' before B
        type_.has_value = (has_c_prime_ || no_offsets_);
      }

    } else if (block_num == BLOCK4) {
      if (has_c_prime_ && !type_.has_value) {
        const GroupType potential_type(getBits<5>(block.data, 11));
        if (potential_type.number == 15 && potential_type.version == GroupType::Version::B) {
          type_ = potential_type;
        }
      }
    }

    if (block.offset == Offset::Cprime && has(BLOCK2)) {
      type_.has_value = (type_.value.version == GroupType::Version::B);
    }
  }
}

void Group::setRxTime(std::chrono::time_point<std::chrono::system_clock> t) {
  time_received_ = t;
}

// \param bler Block error rate, percent
void Group::setAverageBLER(float bler) {
  bler_ = bler;
}

void Group::setTimeFromStart(double time_from_start) {
  time_from_start_ = time_from_start;
}

/**
 * Get the raw group data encoded as hex, like in RDS Spy. (No timestamps etc.)
 *
 * Invalid blocks are replaced with "----".
 *
 */
std::string Group::asHex() const {
  std::string result;
  result.reserve(4 * 4 + 3);  // 4 blocks, 4 chars each, 3 spaces
  for (const eBlockNumber block_num : {BLOCK1, BLOCK2, BLOCK3, BLOCK4}) {
    const Block& block = blocks_[block_num];
    if (block.is_received) {
      result += getHexString<4>(block.data);
    } else {
      result += "----";
    }
    if (block_num != BLOCK4)
      result += ' ';
  }

  return result;
}

}  // namespace redsea
