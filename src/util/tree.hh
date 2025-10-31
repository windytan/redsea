#ifndef TREE_HH_
#define TREE_HH_

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace redsea {

// ObjectTree represents the output JSON structure
class ObjectTree {
 public:
  using object_t = std::vector<std::pair<std::string, ObjectTree>>;
  using array_t  = std::vector<ObjectTree>;
  using value_t  = std::variant<std::nullptr_t, bool, int, double, std::string, object_t, array_t>;

  ObjectTree() : value(nullptr) {}

  // NOLINTBEGIN(google-explicit-constructor)
  // NOLINTBEGIN(hicpp-explicit-conversions)
  ObjectTree(int v) : value(v) {}
  ObjectTree(std::uint32_t v) : value(static_cast<int>(v)) {}
  ObjectTree(long v) : value(static_cast<int>(v)) {}
  ObjectTree(double v) : value(v) {}
  ObjectTree(const std::string& v) : value(v) {}
  ObjectTree(const std::string_view& v) : value(std::string{v}) {}
  ObjectTree(const char* v) : value(std::string(v)) {}
  ObjectTree(bool v) : value(v) {}
  // NOLINTEND(hicpp-explicit-conversions)
  // NOLINTEND(google-explicit-constructor)

  ObjectTree& operator[](const std::string_view& key) {
    if (!std::holds_alternative<object_t>(value))
      value = object_t{};
    auto& obj = std::get<object_t>(value);
    for (auto& kv : obj) {
      if (kv.first == key)
        return kv.second;
    }
    obj.emplace_back(key, ObjectTree{});
    return obj.back().second;
  }
  ObjectTree& operator[](std::size_t index) {
    if (!std::holds_alternative<array_t>(value))
      value = array_t{};
    auto& arr = std::get<array_t>(value);
    if (index >= arr.size())
      arr.resize(index + 1);
    return arr.at(index);
  }
  void push_back(const ObjectTree& v) {
    if (!std::holds_alternative<array_t>(value))
      value = array_t{};
    std::get<array_t>(value).push_back(v);
  }

  [[nodiscard]] const value_t& get() const {
    return value;
  }

  [[nodiscard]] bool empty() const {
    return std::holds_alternative<std::nullptr_t>(value);
  }

  [[nodiscard]] bool contains(const std::string_view& key) const {
    if (!std::holds_alternative<object_t>(value))
      return false;
    const auto& obj = std::get<object_t>(value);
    for (const auto& kv : obj) {
      if (kv.first == key)
        return true;
    }
    return false;
  }

 private:
  value_t value;
};

}  // namespace redsea

#endif  // TREE_HH_
