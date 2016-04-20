#ifndef GROUPS_H_
#define GROUPS_H_

#include <string>
#include <vector>

namespace redsea {

enum {
  TYPE_A, TYPE_B
};

uint16_t bits (uint16_t bitstring, int starting_at, int len);

class RDSString {
  public:
  RDSString(int len=8);
  void setAt(int, int);
  size_t lengthReceived() const;
  size_t lengthExpected() const;
  std::string getString() const;
  std::string getLastCompleteString() const;
  bool isComplete() const;
  void clear();

  private:
  std::vector<int> chars_;
  std::vector<bool> is_char_sequential_;
  int prev_pos_;
  std::string last_complete_string_;

};

struct Group {
  Group(std::vector<uint16_t> blockbits) : num_blocks(blockbits.size()) {
    if (num_blocks > 0)
      block1 = blockbits[0];
    if (num_blocks > 1) {
      block2 = blockbits[1];
      type    = bits(blockbits[1], 12, 4);
      type_ab = bits(blockbits[1], 11, 1);
    }
    if (num_blocks > 2)
      block3 = blockbits[2];
    if (num_blocks > 3)
      block4 = blockbits[3];
  }

  int type;
  int type_ab;
  int num_blocks;
  uint16_t block1;
  uint16_t block2;
  uint16_t block3;
  uint16_t block4;

};

class Station {
  public:
    Station();
    Station(uint16_t pi);
    void add(Group);
    bool hasPS() const;
    std::string getPS() const;
    std::string getRT() const;
    uint16_t getPI() const;
    std::string getCountryCode() const;
  private:
    void decodeType0(Group);
    void decodeType1(Group);
    void decodeType2(Group);
    void decodeType4(Group);
    void addAltFreq(uint8_t);
    void updatePS(int pos, std::vector<int> chars);
    void updateRadioText(int pos, std::vector<int> chars);
    uint16_t pi_;
    RDSString ps_;
    RDSString rt_;
    int rt_ab_;
    int pty_;
    bool is_tp_;
    bool is_ta_;
    bool is_music_;
    std::vector<double> alt_freqs_;
    int num_alt_freqs_;
    int pin_;
    int ecc_;
    int cc_;
    int tmc_id_;
    int ews_channel_;
    int lang_;
    int linkage_la_;
    std::string clock_time_;
    bool has_country_;

    int pager_pac_;
    int pager_opc_;
    int pager_tng_;
    int pager_ecc_;
    int pager_ccf_;
    int pager_interval_;

};

} // namespace redsea
#endif // GROUPS_H_
