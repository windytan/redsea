#include "src/rft/crc.hh"

#include <cstddef>
#include <cstdint>

namespace redsea {

namespace {

constexpr std::uint8_t kCRCModeEntireFile = 0;
constexpr std::uint8_t kCRCModeAuto       = 7;

}  // namespace

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

// Calculate the chunk length, in bytes, based on the mode and file size
std::uint32_t ChunkCRC::getChunkLength(std::uint32_t file_size_bytes) const {
  if (mode == kCRCModeEntireFile) {
    return file_size_bytes;
  }

  return sizeof(RFTSegment) * (8U << getActualMode(file_size_bytes));
}

// Calculate the byte address based on the mode and file size
std::uint32_t ChunkCRC::getByteAddress(std::uint32_t file_size_bytes) const {
  if (mode == kCRCModeEntireFile) {
    return 0;
  }

  return address_raw * getChunkLength(file_size_bytes);
}

// IEC 62106-2 ED2:2021 Annex D
std::uint32_t crc16_ccitt(const void* data, std::size_t address, std::size_t length) {
  if (data == nullptr || length == 0) {
    return 0;
  }

  std::uint32_t crc     = 0xFFFF;
  const auto* byte_data = static_cast<const std::uint8_t*>(data);

  for (std::size_t i = 0; i < length; ++i) {
    crc = static_cast<std::uint8_t>(crc >> 8U) | (crc << 8U);
    crc ^= byte_data[address + i];
    crc ^= static_cast<std::uint8_t>((crc & 0xFFU) >> 4U);
    crc ^= (crc << 8U) << 4U;
    crc ^= ((crc & 0xFFU) << 4U) << 1U;
  }
  return (crc ^ 0xFFFFU) & 0xFFFFU;
}

}  // namespace redsea
