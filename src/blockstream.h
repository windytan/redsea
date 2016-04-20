#ifndef BLOCKSTREAM_H_
#define BLOCKSTREAM_H_

#include <map>

#include "bitstream.h"

namespace redsea {

enum {
  A, B, C, CI, D
};

class BlockStream {
  public:
  BlockStream();
  std::vector<uint16_t> getNextGroup();
  bool isEOF() const;

  private:
  void uncorrectable();
  int bitcount_;
  int prevbitcount_;
  int left_to_read_;
  uint32_t wideblock_;
  int prevsync_;
  int block_counter_;
  int expected_offset_;
  uint16_t pi_;
  std::vector<bool> has_sync_for_;
  bool is_in_sync_;
  std::vector<uint16_t> offset_word_;
  std::vector<int> block_for_offset_;
  std::vector<uint16_t> group_data_;
  std::vector<bool> has_block_;
  std::vector<bool> block_has_errors_;
  BitStream bit_stream_;
  bool has_whole_group_;
  std::map<uint16_t,uint16_t> error_lookup_;

};

} // namespace redsea
#endif // BLOCKSTREAM_H_
