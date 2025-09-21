#ifndef GROUP_HH_
#define GROUP_HH_

#include <array>
#include <chrono>
#include <cstdint>
#include <string>

#include "src/maybe.hh"

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

class GroupType {
 public:
  enum class Version : std::uint8_t { A = 0, B = 1, C = 2 };

  GroupType() = default;
  explicit GroupType(std::uint16_t type_code);

  [[nodiscard]] std::string str() const;

  std::uint16_t number{};
  Version version{Version::A};
};

bool operator<(const GroupType& type1, const GroupType& type2);
bool operator==(const GroupType& type1, const GroupType& type2);

/*
 * A single RDS group transmitted as four 16-bit blocks.
 *
 */
class Group {
 public:
  Group() = default;

  [[nodiscard]] std::uint16_t get(eBlockNumber block_num) const;
  [[nodiscard]] bool has(eBlockNumber block_num) const;

  [[nodiscard]] bool isEmpty() const;
  [[nodiscard]] GroupType getType() const;
  [[nodiscard]] bool hasType() const;
  [[nodiscard]] std::uint16_t getPI() const;
  [[nodiscard]] float getBLER() const;
  [[nodiscard]] int getNumErrors() const;
  [[nodiscard]] Maybe<double> getTimeFromStart() const;

  [[nodiscard]] bool hasPI() const;
  [[nodiscard]] bool hasBLER() const;
  [[nodiscard]] bool hasRxTime() const;
  [[nodiscard]] std::chrono::time_point<std::chrono::system_clock> getRxTime() const;
  [[nodiscard]] int getDataStream() const;

  void setVersionC();
  void setDataStream(int stream);
  void disableOffsets();
  void setBlock(eBlockNumber block_num, Block block);
  void setRxTime(std::chrono::time_point<std::chrono::system_clock> t);
  void setAverageBLER(float bler);
  void setTimeFromStart(double time_from_start);

 private:
  GroupType type_;
  std::array<Block, 4> blocks_;
  std::chrono::time_point<std::chrono::system_clock> time_received_;
  float bler_{0.f};
  int data_stream_{0};
  // Seconds from the beginning of the file until the first bit of this group
  double time_from_start_{0.0};
  bool has_type_{false};
  bool has_c_prime_{false};
  bool has_bler_{false};
  bool has_rx_time_{false};
  bool has_time_from_start_{false};
  bool no_offsets_{false};
};

}  // namespace redsea

#endif  // GROUP_HH_
