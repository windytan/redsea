#ifndef GROUP_HH_
#define GROUP_HH_

#include <array>
#include <chrono>
#include <cstdint>
#include <string>

#include "src/util/maybe.hh"

namespace redsea {

// A scoped enum couldn't readily be used for indexing
enum eBlockNumber : std::uint8_t { BLOCK1, BLOCK2, BLOCK3, BLOCK4 };
enum class Offset : std::uint8_t { A, B, C, Cprime, D, invalid };

struct Block {
  std::uint32_t raw{};
  std::uint16_t data{};
  bool is_received{false};
  bool had_errors{false};
  Offset offset{Offset::invalid};
};

struct GroupType {
  enum class Version : std::uint8_t { A, B, C };

  GroupType() = default;
  explicit GroupType(std::uint16_t type_code);

  [[nodiscard]] std::string str() const;

  std::uint16_t number{};
  Version version{Version::A};
};

bool operator<(const GroupType& type1, const GroupType& type2);

inline GroupType makeGroupTypeC() {
  GroupType type;
  type.version = GroupType::Version::C;
  return type;
}

/*
 * A single RDS group transmitted as four 16-bit blocks.
 *
 */
class Group {
 public:
  Group() = default;

  [[nodiscard]] std::uint16_t get(eBlockNumber block_num) const;
  [[nodiscard]] bool has(eBlockNumber block_num) const;

  [[nodiscard]] bool hasPI() const;
  [[nodiscard]] bool isEmpty() const;

  [[nodiscard]] std::uint16_t getPI() const;
  [[nodiscard]] std::uint32_t getDataStream() const;
  [[nodiscard]] int getNumErrors() const;
  [[nodiscard]] std::string asHex() const;

  [[nodiscard]] Maybe<GroupType> getType() const;
  [[nodiscard]] Maybe<float> getBLER() const;
  [[nodiscard]] Maybe<double> getTimeFromStart() const;
  [[nodiscard]] Maybe<std::chrono::time_point<std::chrono::system_clock>> getRxTime() const;

  void setVersionC();
  void setDataStream(std::uint32_t stream);
  void setBlock(eBlockNumber block_num, Block block);
  void setRxTime(std::chrono::time_point<std::chrono::system_clock> t);
  void setAverageBLER(float bler);
  void setTimeFromStart(double time_from_start);
  void disableOffsets();

 private:
  std::array<Block, 4> blocks_;
  std::uint32_t data_stream_{0};

  Maybe<GroupType> type_;
  Maybe<std::chrono::time_point<std::chrono::system_clock>> time_received_;
  Maybe<float> bler_;
  // Seconds from the beginning of the file until the first bit of this group
  Maybe<double> time_from_start_;
  bool has_c_prime_{false};
  bool no_offsets_{false};
};

}  // namespace redsea

#endif  // GROUP_HH_
