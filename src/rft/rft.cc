#include "src/rft/rft.hh"

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <string>

#include "src/rft/base64.hh"
#include "src/rft/crc.hh"
#include "src/util.hh"

namespace redsea {

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
    RFTSegment segment;
    segment.bytes              = {static_cast<std::uint8_t>(getBits(block2, 0, 8)),
                                  static_cast<std::uint8_t>(getBits(block3, 8, 8)),
                                  static_cast<std::uint8_t>(getBits(block3, 0, 8)),
                                  static_cast<std::uint8_t>(getBits(block4, 8, 8)),
                                  static_cast<std::uint8_t>(getBits(block4, 0, 8))};
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

// Return the file contents encoded as PEM Base64
std::string RFTFile::getBase64Data() {
  is_printed_ = true;
  return asBase64(data_.data(), expected_size_bytes_);
}

}  // namespace redsea
