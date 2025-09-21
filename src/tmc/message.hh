#ifndef TMC_MESSAGE_HH_
#define TMC_MESSAGE_HH_

#include <array>
#include <cstdint>
#include <utility>
#include <vector>

#include "src/io/output.hh"
#include "src/tmc/eventdb.hh"

namespace redsea {

class ObjectTree;

}  // namespace redsea

namespace redsea::tmc {

const bool kMessagePartIsReceived = true;

struct MessagePart {
  MessagePart() = default;
  MessagePart(bool _is_received, const std::array<std::uint16_t, 2>& data_in);

  bool is_received{false};
  std::array<std::uint16_t, 2> data{};
};

struct ServiceKey {
  std::uint8_t xorval{0};
  std::uint8_t xorstart{0};
  std::uint8_t nrot{0};
};

struct FreeformField {
  FieldLabel label;
  std::uint16_t data;
};

class Quantifiers {
 public:
  void insert(std::size_t index, std::uint16_t value);
  [[nodiscard]] bool has(std::size_t index) const;
  [[nodiscard]] std::uint16_t get(std::size_t index) const;

 private:
  std::vector<std::pair<std::size_t, std::uint16_t>> data;
};

class Message {
 public:
  explicit Message(bool is_loc_encrypted);
  void pushMulti(std::uint16_t x, std::uint16_t y, std::uint16_t z);
  void pushSingle(std::uint16_t x, std::uint16_t y, std::uint16_t z);
  [[nodiscard]] ObjectTree tree() const;
  void decrypt(const ServiceKey& key);
  [[nodiscard]] bool isComplete() const;
  void clear();
  [[nodiscard]] std::uint16_t getContinuityIndex() const;
  [[nodiscard]] bool hasLocation() const;
  [[nodiscard]] std::uint16_t getLocation() const;
  [[nodiscard]] int getExtent() const;

 private:
  void decodeMulti();

  bool is_encrypted_;
  bool was_encrypted_;
  std::uint16_t duration_{0};
  DurationType duration_type_{DurationType::Dynamic};
  bool diversion_advised_{false};
  Direction direction_{Direction::Positive};
  std::uint16_t extent_{0};
  std::vector<std::uint16_t> events_;
  std::vector<std::uint16_t> supplementary_;
  Quantifiers quantifiers_;
  std::vector<std::uint16_t> diversion_;
  std::uint16_t location_{0};
  std::uint16_t encrypted_location_{0};
  bool is_complete_{false};
  bool has_length_affected_{false};
  std::uint16_t length_affected_{0};
  bool has_time_until_{false};
  std::uint16_t time_until_{0};
  bool has_time_starts_{false};
  std::uint16_t time_starts_{0};
  bool has_speed_limit_{false};
  std::uint16_t speed_limit_{0};
  EventDirectionality directionality_{EventDirectionality::Single};
  EventUrgency urgency_{EventUrgency::None};
  std::uint16_t continuity_index_{0};
  std::array<MessagePart, 5> parts_{};
};

}  // namespace redsea::tmc

#endif  // TMC_MESSAGE_HH_
