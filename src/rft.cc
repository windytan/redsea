#include "src/rft.hh"

#include <cstdint>
#include <string>
#include <vector>

#include "src/util/base64.hh"
#include "src/util/util.hh"

namespace redsea {

std::uint16_t ChunkCRC::getActualMode(std::uint32_t file_size_bytes) const {
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

std::uint32_t ChunkCRC::getChunkLength(std::uint32_t file_size_bytes) const {
  if (mode == kCRCModeEntireFile) {
    return file_size_bytes;
  }

  return sizeof(RFTSegment) * (8U << getActualMode(file_size_bytes));
}

std::uint32_t ChunkCRC::getByteAddress(std::uint32_t file_size_bytes) const {
  if (mode == kCRCModeEntireFile) {
    return 0;
  }

  return address_raw * getChunkLength(file_size_bytes);
}

std::uint32_t crc16_ccitt(const void* data, std::size_t address, std::size_t length) {
  if (data == nullptr || length == 0) {
    return 0;
  }

  std::uint32_t crc     = 0xFFFF;
  const auto* byte_data = static_cast<const std::uint8_t*>(data);

  for (std::size_t i = 0; i < length; ++i) {
    crc = static_cast<std::uint8_t>(crc >> 8U) | (crc << 8U);
    crc ^= byte_data[address + i];
    crc ^= static_cast<std::uint8_t>(crc & 0xFFU) >> 4U;
    crc ^= (crc << 8U) << 4U;
    crc ^= ((crc & 0xFFU) << 4U) << 1U;
  }
  return (crc ^ 0xFFFFU) & 0xFFFFU;
}

void RFTFile::setSize(std::uint32_t size) {
  constexpr std::uint32_t kMaxSize = kMaxNumSegments * sizeof(RFTSegment);
  if (size <= kMaxSize)
    expected_size_bytes_ = size;
}

void RFTFile::clear() {
  for (auto&& r : received_) {
    r = false;
  }
  is_printed_ = false;
  crc_chunks_.clear();
}

void RFTFile::setCRCFlag(int flag) {
  expect_crc_ = flag;
}

bool RFTFile::isCRCExpected() const {
  return expect_crc_;
}

void RFTFile::receiveCRC(const ChunkCRC& chunk_crc) {
  assert(chunk_crc.address_raw < kMaxNumCRCs);
  crc_chunks_.resize(kMaxNumCRCs);
  crc_chunks_[chunk_crc.address_raw]          = chunk_crc;
  crc_chunks_[chunk_crc.address_raw].received = true;
}

void RFTFile::receive(int toggle, std::uint32_t segment_address, std::uint16_t block2,
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
    RFTSegment segment{};
    segment.bytes              = {static_cast<std::uint8_t>(getBits<8>(block2, 0)),
                                  static_cast<std::uint8_t>(getBits<8>(block3, 8)),
                                  static_cast<std::uint8_t>(getBits<8>(block3, 0)),
                                  static_cast<std::uint8_t>(getBits<8>(block4, 8)),
                                  static_cast<std::uint8_t>(getBits<8>(block4, 0))};
    data_[segment_address]     = segment;
    received_[segment_address] = true;
  }
}

bool RFTFile::hasNewCompleteFile() const {
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

std::string RFTFile::getBase64Data() {
  is_printed_ = true;
  return asBase64(data_.data(), expected_size_bytes_);
}

}  // namespace redsea
