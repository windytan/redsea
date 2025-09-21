#ifndef RFT_RFT_HH_
#define RFT_RFT_HH_

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "src/rft/crc.hh"

namespace redsea {

// \brief RFT is a file transfer protocol used in RDS2
class RFTFile {
 public:
  RFTFile() = default;

  // \note Does nothing if size is too large
  void setSize(std::uint32_t size);

  void clear();

  // \param flag 0: no CRC, 1: CRC
  void setCRCFlag(int flag);

  bool isCRCExpected() const;

  void receiveCRC(const ChunkCRC& chunk_crc);

  // Receive segment data for this file
  // \param toggle RFT toggle bit
  // \param segment_address 0..32767
  void receive(int toggle, std::uint32_t segment_address, std::uint16_t block2,
               std::uint16_t block3, std::uint16_t block4);

  bool hasNewCompleteFile() const;

  // Return the file contents encoded as PEM Base64
  std::string getBase64Data();

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

#endif  // RFT_RFT_HH_
