#include "src/group.hh"

#include <chrono>
#include <cstdint>
#include <numeric>
#include <string>

#include "src/maybe.hh"
#include "src/util.hh"

namespace redsea {

GroupType::GroupType(std::uint16_t type_code)
    : number(static_cast<std::uint16_t>(type_code >> 1U) & 0xFU),
      version((type_code & 0x1U) == 0 ? GroupType::Version::A : GroupType::Version::B) {}

std::string GroupType::str() const {
  if (version == Version::C)
    return "C";
  return std::string(std::to_string(number) + (version == Version::A ? "A" : "B"));
}

bool operator<(const GroupType& type1, const GroupType& type2) {
  return (type1.number < type2.number) ||
         (type1.number == type2.number && type1.version < type2.version);
}

bool operator==(const GroupType& type1, const GroupType& type2) {
  return (type1.number == type2.number) && (type1.version == type2.version);
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
// \note Remember to check if hasBLER()
float Group::getBLER() const {
  return bler_;
}

int Group::getNumErrors() const {
  return std::accumulate(blocks_.cbegin(), blocks_.cend(), 0, [](int a, Block b) {
    return a + ((b.had_errors || !b.is_received) ? 1 : 0);
  });
}

Maybe<double> Group::getTimeFromStart() const {
  return {time_from_start_, has_time_from_start_};
}

bool Group::hasPI() const {
  return type_.version != GroupType::Version::C &&
         (blocks_[BLOCK1].is_received ||
          (blocks_[BLOCK3].is_received && blocks_[BLOCK3].offset == Offset::Cprime));
}

GroupType Group::getType() const {
  return type_;
}

bool Group::hasType() const {
  return has_type_;
}

bool Group::hasBLER() const {
  return has_bler_;
}

bool Group::hasRxTime() const {
  return has_rx_time_;
}

std::chrono::time_point<std::chrono::system_clock> Group::getRxTime() const {
  return time_received_;
}

void Group::disableOffsets() {
  no_offsets_ = true;
}

void Group::setVersionC() {
  type_.version = GroupType::Version::C;
  has_type_     = true;
}

void Group::setDataStream(int stream) {
  data_stream_ = stream;
}

int Group::getDataStream() const {
  return data_stream_;
}

void Group::setBlock(eBlockNumber block_num, Block block) {
  blocks_[block_num] = block;

  if (has_type_)
    return;

  if (block_num == BLOCK2) {
    type_ = GroupType(getBits(block.data, 11, 5));
    if (type_.version == GroupType::Version::A) {
      has_type_ = true;
    } else {
      has_type_ = (has_c_prime_ || no_offsets_);
    }

  } else if (block_num == BLOCK4) {
    if (has_c_prime_ && !has_type_) {
      const GroupType potential_type(getBits(block.data, 11, 5));
      if (potential_type.number == 15 && potential_type.version == GroupType::Version::B) {
        type_     = potential_type;
        has_type_ = true;
      }
    }
  }

  if (block.offset == Offset::Cprime && has(BLOCK2)) {
    has_type_ = (type_.version == GroupType::Version::B);
  }
}

void Group::setRxTime(std::chrono::time_point<std::chrono::system_clock> t) {
  time_received_ = t;
  has_rx_time_   = true;
}

// \param bler Block error rate, percent
void Group::setAverageBLER(float bler) {
  bler_     = bler;
  has_bler_ = true;
}

void Group::setTimeFromStart(double time_from_start) {
  time_from_start_     = time_from_start;
  has_time_from_start_ = true;
}

}  // namespace redsea
