#ifndef BLOCKRECEIVER_H_
#define BLOCKRECEIVER_H_

#include <map>

#include "bitreceiver.h"

enum {
  A, B, C, CI, D
};

class BlockReceiver {
  public:
  BlockReceiver();
  std::vector<uint16_t> getNextGroup();

  private:
  void blockError();
  int bitcount_;
  int prevbitcount_;
  int left_to_read_;
  int wideblock_;
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
  BitReceiver bit_receiver_;
  bool has_whole_group_;
  std::map<uint16_t,uint16_t> error_vector_;


};


#endif // BLOCKRECEIVER_H_
