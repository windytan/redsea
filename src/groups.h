#ifndef GROUPS_H_
#define GROUPS_H_

#include <map>
#include <set>
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

class GroupType {
  public:
  GroupType(uint16_t type_code=0x00);
  GroupType(const GroupType& obj);

  bool operator==(const GroupType& other);

  std::string toString();

  uint16_t num;
  uint16_t ab;
};
bool operator<(const GroupType& obj1, const GroupType& obj2);

struct Group {
  Group(std::vector<uint16_t> blockbits) : num_blocks(blockbits.size()), type(bits(blockbits.at(1), 11, 5)) {
    if (num_blocks > 0)
      block1 = blockbits[0];
    if (num_blocks > 1) {
      block2 = blockbits[1];
    }
    if (num_blocks > 2)
      block3 = blockbits[2];
    if (num_blocks > 3)
      block4 = blockbits[3];
  }

  GroupType type;
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
    void update(Group);
    bool hasPS() const;
    std::string getPS() const;
    std::string getRT() const;
    uint16_t getPI() const;
    std::string getCountryCode() const;
  private:
    void decodeType0(Group);
    void decodeType1(Group);
    void decodeType2(Group);
    void decodeType3(Group);
    void decodeType4(Group);
    void decodeType8(Group);
    void decodeType14(Group);
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
    std::set<double> alt_freqs_;
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
    std::map<GroupType,uint16_t> oda_app_for_group_;

    int pager_pac_;
    int pager_opc_;
    int pager_tng_;
    int pager_ecc_;
    int pager_ccf_;
    int pager_interval_;

};

} // namespace redsea
#endif // GROUPS_H_
