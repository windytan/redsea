#ifndef RFT_HH_
#define RFT_HH_

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "src/base64.hh"
#include "src/util.hh"

namespace redsea {

struct RFTSegment {
  std::array<std::uint8_t, 5> bytes;
};

class RFTFile {
 public:
  RFTFile() = default;

  // \note Does nothing if size is too large
  void setSize(std::uint32_t size) {
    constexpr std::uint32_t kMaxSize = kMaxNumSegments * sizeof(RFTSegment);
    if (size <= kMaxSize)
      expected_size_bytes_ = size;
  }

  void clear() {
    for (std::size_t i = 0; i < received_.size(); i++) {
      received_[i] = false;
    }
    is_printed_ = false;
  }

  // \param flag 0: no CRC, 1: CRC
  void setCRCFlag(int flag) {
    expect_crc_ = flag;
  }

  // \param crc Expected crc16_ccitt
  // \note TODO CRC mode
  void setExpectedCRC(std::uint16_t expected_crc) {
    expected_crc_ = expected_crc;
  }

  // Receive segment data for this file
  // \param toggle RFT toggle bit
  // \param segment_address 0..32767
  void receive(int toggle, std::uint32_t segment_address, std::uint16_t block2,
               std::uint16_t block3, std::uint16_t block4) {
    // To prevent having to hold 16 x 170 kB buffers at all times, memory is allocated when
    // the first group is received on this pipe
    // (vector::resize() is a no-op on subsequent calls)
    received_.resize(kMaxNumSegments);
    data_.resize(kMaxNumSegments);

    // File contents changed
    if (toggle != prev_toggle_) {
      clear();
    }
    prev_toggle_ = toggle;

    if (segment_address < kMaxNumSegments) {
      RFTSegment segment;
      segment.bytes              = {static_cast<std::uint8_t>(getBits<8>(block2, 0)),
                                    static_cast<std::uint8_t>(getBits<8>(block3, 8)),
                                    static_cast<std::uint8_t>(getBits<8>(block3, 0)),
                                    static_cast<std::uint8_t>(getBits<8>(block4, 8)),
                                    static_cast<std::uint8_t>(getBits<8>(block4, 0))};
      data_[segment_address]     = segment;
      received_[segment_address] = true;
    }
  }

  bool hasNewCompleteFile() const {
    if (is_printed_ || expected_size_bytes_ == 0 || received_.empty())
      return false;

    const std::size_t expected_num_segments = divideRoundingUp(expected_size_bytes_, 5U);
    for (std::size_t i = 0; i < expected_num_segments; i++) {
      if (!received_[i]) {
        return false;
      }
    }

    return true;
  }

  // Return the file contents encoded as PEM Base64
  std::string getBase64Data() {
    is_printed_ = true;
    return asBase64(data_.data(), expected_size_bytes_);
  }

 private:
  static constexpr std::size_t kMaxNumSegments = 1 << 15;

  // 163.8 kB buffer in heap
  std::vector<RFTSegment> data_;
  // ~4 kB buffer in heap
  std::vector<bool> received_;
  std::uint32_t expected_size_bytes_{};
  bool is_printed_{};
  bool expect_crc_{};
  std::uint16_t expected_crc_{};
  int prev_toggle_{};
};

}  // namespace redsea

#endif  // RFT_HH_
