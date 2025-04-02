#ifndef RFT_HH_
#define RFT_HH_

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "src/base64.hh"
#include "src/util.hh"

namespace redsea {

struct RFTSegment {
  std::array<std::uint8_t, 5> bytes;
};

constexpr std::uint8_t kCRCModeEntireFile = 0;
constexpr std::uint8_t kCRCModeAuto       = 7;

struct ChunkCRC {
  std::uint16_t mode;
  std::uint16_t address_raw;
  std::uint16_t crc;
  bool received{};

  // In case of auto mode selection
  std::uint16_t getActualMode(std::uint32_t file_size_bytes) const {
    if (mode == kCRCModeAuto) {
      if (file_size_bytes <= 40'960) {
        return 1;
      } else if (file_size_bytes <= 81'920) {
        return 2;
      } else {
        return 3;
      }
    }
    return mode;
  }

  // Calculate the chunk length, in bytes, based on the mode and file size
  std::uint32_t getChunkLength(std::uint32_t file_size_bytes) const {
    if (mode == kCRCModeEntireFile) {
      return file_size_bytes;
    }

    return sizeof(RFTSegment) * (8U << getActualMode(file_size_bytes));
  }

  // Calculate the byte address based on the mode and file size
  std::uint32_t getByteAddress(std::uint32_t file_size_bytes) const {
    if (mode == kCRCModeEntireFile) {
      return 0;
    }

    return address_raw * getChunkLength(file_size_bytes);
  }
};

// IEC 62106-2 ED2:2021 Annex D
inline std::uint32_t crc16_ccitt(const void* data, std::size_t address, std::size_t length) {
  if (data == nullptr || length == 0) {
    return 0;
  }

  std::uint32_t crc             = 0xFFFF;
  const std::uint8_t* byte_data = static_cast<const std::uint8_t*>(data);

  for (std::size_t i = 0; i < length; ++i) {
    crc = static_cast<std::uint8_t>(crc >> 8U) | (crc << 8U);
    crc ^= byte_data[address + i];
    crc ^= static_cast<std::uint8_t>(crc & 0xFFU) >> 4U;
    crc ^= (crc << 8U) << 4U;
    crc ^= ((crc & 0xFFU) << 4U) << 1U;
  }
  return (crc ^ 0xFFFFU) & 0xFFFFU;
}

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
    crc_chunks_.clear();
  }

  // \param flag 0: no CRC, 1: CRC
  void setCRCFlag(int flag) {
    expect_crc_ = flag;
  }

  bool isCRCExpected() const {
    return expect_crc_;
  }

  void receiveCRC(const ChunkCRC& chunk_crc) {
    assert(chunk_crc.address_raw < kMaxNumCRCs);
    crc_chunks_.resize(kMaxNumCRCs);
    crc_chunks_[chunk_crc.address_raw]          = chunk_crc;
    crc_chunks_[chunk_crc.address_raw].received = true;
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
  static constexpr std::size_t kMaxNumCRCs     = 1 << 9;

  // 163.8 kB buffer in heap
  std::vector<RFTSegment> data_;
  // ~4 kB buffer in heap
  std::vector<bool> received_;
  std::vector<ChunkCRC> crc_chunks_;
  std::uint32_t expected_size_bytes_{};
  bool is_printed_{};
  bool expect_crc_{};
  int prev_toggle_{};
};

}  // namespace redsea

#endif  // RFT_HH_
