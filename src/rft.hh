#ifndef RFT_HH_
#define RFT_HH_

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace redsea {

struct RFTSegment {
  std::array<std::uint8_t, 5> bytes;
};

constexpr std::uint8_t kCRCModeEntireFile = 0;
constexpr std::uint8_t kCRCModeAuto       = 7;

struct ChunkCRC {
  std::uint16_t mode{};
  std::uint16_t address_raw{};
  std::uint16_t crc{};
  bool received{};

  // In case of auto mode selection
  [[nodiscard]] std::uint16_t getActualMode(std::uint32_t file_size_bytes) const;

  // Calculate the chunk length, in bytes, based on the mode and file size
  [[nodiscard]] std::uint32_t getChunkLength(std::uint32_t file_size_bytes) const;

  // Calculate the byte address based on the mode and file size
  [[nodiscard]] std::uint32_t getByteAddress(std::uint32_t file_size_bytes) const;
};

// IEC 62106-2 ED2:2021 Annex D
std::uint32_t crc16_ccitt(const void* data, std::size_t address, std::size_t length);

class RFTFile {
 public:
  RFTFile() = default;

  // \note Does nothing if size is too large
  void setSize(std::uint32_t size);

  void clear();

  // \param flag 0: no CRC, 1: CRC
  void setCRCFlag(int flag);

  [[nodiscard]] bool isCRCExpected() const;

  void receiveCRC(const ChunkCRC& chunk_crc);

  // Receive segment data for this file
  // \param toggle RFT toggle bit
  // \param segment_address 0..32767
  void receive(int toggle, std::uint32_t segment_address, std::uint16_t block2,
               std::uint16_t block3, std::uint16_t block4);

  [[nodiscard]] bool hasNewCompleteFile() const;

  // Return the file contents encoded as PEM Base64
  [[nodiscard]] std::string getBase64Data();

 private:
  static constexpr std::size_t kMaxNumSegments = 1U << 15U;
  static constexpr std::size_t kMaxNumCRCs     = 1U << 9U;

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
