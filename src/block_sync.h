#ifndef BLOCK_SYNC_H_
#define BLOCK_SYNC_H_

#include <map>

#include "ascii_in.h"
#include "groups.h"
#include "subcarrier.h"

namespace redsea {

enum eOffset {
  OFFSET_A, OFFSET_B, OFFSET_C, OFFSET_CI, OFFSET_D, OFFSET_INVALID
};

enum eInputType {
  INPUT_MPX, INPUT_ASCIIBITS, INPUT_RDSSPY
};

enum eOutputType {
  OUTPUT_HEX, OUTPUT_JSON
};

class BlockStream {
  public:
  BlockStream(eInputType input_type=INPUT_MPX);
  Group getNextGroup();
  bool isEOF() const;
#ifdef DEBUG
  float getT() const;
#endif

  private:
  int getNextBit();
  void uncorrectable();
  uint32_t correctBurstErrors(uint32_t block) const;
  bool acquireSync();

  unsigned bitcount_;
  unsigned prevbitcount_;
  unsigned left_to_read_;
  uint32_t wideblock_;
  unsigned prevsync_;
  unsigned block_counter_;
  eOffset expected_offset_;
  eOffset received_offset_;
  uint16_t pi_;
  bool is_in_sync_;
  std::vector<uint16_t> group_data_;
  std::vector<bool> has_block_;
  std::vector<bool> block_has_errors_;
  Subcarrier subcarrier_;
  AsciiBits ascii_bits_;
  std::map<uint16_t,uint32_t> error_lookup_;
  unsigned num_blocks_received_;
  const eInputType input_type_;
  bool is_eof_;

};

} // namespace redsea
#endif // BLOCK_SYNC_H_
