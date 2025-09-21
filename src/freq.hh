#ifndef FREQ_HH_
#define FREQ_HH_

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace redsea {

class CarrierFrequency {
 public:
  enum class Band : std::uint8_t { LF_MF, FM };

  explicit CarrierFrequency(std::uint16_t code, Band band = Band::FM);
  [[nodiscard]] bool isValid() const;
  [[nodiscard]] int kHz() const;
  [[nodiscard]] std::string str() const;
  friend bool operator==(const CarrierFrequency& f1, const CarrierFrequency& f2);
  friend bool operator<(const CarrierFrequency& f1, const CarrierFrequency& f2);

 private:
  std::uint16_t code_{};
  Band band_{Band::FM};
};

class AltFreqList {
 public:
  AltFreqList() = default;
  void insert(std::uint16_t af_code);
  [[nodiscard]] bool isComplete() const;
  [[nodiscard]] bool isMethodB() const;
  [[nodiscard]] std::vector<int> getRawList() const;
  void clear();

 private:
  std::array<int, 25> alt_freqs_{};
  std::size_t num_expected_{0};
  std::size_t num_received_{0};
  bool lf_mf_follows_{false};
};

}  // namespace redsea

#endif  // FREQ_HH_
