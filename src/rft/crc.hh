#ifndef RFT_CRC_HH_
#define RFT_CRC_HH_

#include <array>
#include <cstddef>
#include <cstdint>

namespace redsea {

struct RFTSegment {
  std::array<std::uint8_t, 5> bytes;
};

struct ChunkCRC {
  std::uint16_t mode;
  std::uint16_t address_raw;
  std::uint16_t crc;
  bool received{};

  // In case of auto mode selection
  std::uint16_t getActualMode(std::uint32_t file_size_bytes) const;

  // Calculate the chunk length, in bytes, based on the mode and file size
  std::uint32_t getChunkLength(std::uint32_t file_size_bytes) const;

  // Calculate the byte address based on the mode and file size
  std::uint32_t getByteAddress(std::uint32_t file_size_bytes) const;
};

// IEC 62106-2 ED2:2021 Annex D
std::uint32_t crc16_ccitt(const void* data, std::size_t address, std::size_t length);

}  // namespace redsea

#endif  // RFT_CRC_HH_
