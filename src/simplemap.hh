#ifndef SIMPLEMAP_HH_
#define SIMPLEMAP_HH_

#include <algorithm>
#include <initializer_list>
#include <stdexcept>
#include <vector>

template <typename TKey, typename TValue>
class SimpleMap {
 public:
  SimpleMap() = default;

  SimpleMap(std::initializer_list<std::pair<TKey, TValue>> init) {
    entries_.reserve(init.size());
    for (const auto& pair : init) {
      entries_.push_back(Entry{pair.first, pair.second});
    }
    sort();
  }

  void insert(const TKey& key, const TValue& value) {
    for (auto& entry : entries_) {
      if (entry.key == key) {
        entry.value = value;
        return;
      }
    }
    entries_.push_back(Entry{key, value});
    sort();
  }
  [[nodiscard]] TValue& at(const TKey key) {
    for (auto& entry : entries_) {
      if (entry.key == key) {
        return entry.value;
      }
    }
    throw std::out_of_range("Key not found");
  }
  [[nodiscard]] const TValue& at(const TKey key) const {
    for (const auto& entry : entries_) {
      if (entry.key == key) {
        return entry.value;
      }
    }
    throw std::out_of_range("Key not found");
  }
  [[nodiscard]] bool contains(const TKey& key) const {
    for (const auto& entry : entries_) {
      if (entry.key == key) {
        return true;
      }
    }
    return false;
  }

 private:
  struct Entry {
    TKey key;
    TValue value;
  };
  void sort() {
    std::sort(entries_.begin(), entries_.end(),
              [](const Entry& a, const Entry& b) { return a.key < b.key; });
  }
  std::vector<Entry> entries_;
};

#endif  // SIMPLEMAP_HH_
