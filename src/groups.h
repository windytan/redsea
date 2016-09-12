#ifndef GROUPS_H_
#define GROUPS_H_

#include <map>
#include <set>
#include <string>

#include "rdsstring.h"
#include "tmc.h"

namespace redsea {

enum {
  TYPE_A, TYPE_B
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

class Group {
  public:
  Group(std::vector<uint16_t> blockbits);
  void printHex() const;

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

    tmc::TMC tmc_;

};

} // namespace redsea
#endif // GROUPS_H_
