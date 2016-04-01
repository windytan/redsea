#ifndef REDSEA_H_
#define REDSEA_H_

#include <vector>
#include <map>

enum {
  A, B, C, CI, D
};

class BitReceiver {
  public:
  BitReceiver();
  int getNextBit();

  private:
  void readMoreBits();
  void biphase(double acc);
  void deltaBit(int b);
  void bit(int b);
  double subcarr_phi_;
  double clock_offset_;
  double prevclock_;
  double prev_bb_;
  double acc_;
  int    numsamples_;
  double fsc_;

  double prev_acc_;
  int    counter_;
  std::vector<int>    tot_errs_;
  int    reading_frame_;

  int dbit_;

  std::vector<int> bit_buffer_;
  int bit_buffer_write_ptr_;
  int bit_buffer_read_ptr_;
  int bit_buffer_fill_count_;

};

class GroupReceiver {
  public:
  GroupReceiver();
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

#endif
