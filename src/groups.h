#ifndef GROUPS_H_
#define GROUPS_H_

#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include <json/json.h>

#include "config.h"
#include "src/common.h"
#include "src/rdsstring.h"
#include "src/tmc/tmc.h"

namespace redsea {

enum eGroupTypeVersion {
  VERSION_A, VERSION_B
};

enum eOffset {
  OFFSET_A, OFFSET_B, OFFSET_C, OFFSET_CI, OFFSET_D, OFFSET_INVALID
};

class GroupType {
 public:
  GroupType(uint16_t type_code = 0x00);
  GroupType(const GroupType& obj);

  bool operator==(const GroupType& other);

  std::string toString() const;

  uint16_t num;
  eGroupTypeVersion ab;
};

bool operator<(const GroupType& obj1, const GroupType& obj2);

class Group {
 public:
  Group();
  void printHex(std::ostream* stream) const;

  GroupType type;
  bool hasType;
  uint16_t pi;
  bool hasPi;
  std::vector<bool> hasOffset;
  std::vector<uint16_t> block;
};

class Station {
 public:
  Station();
  Station(uint16_t pi, Options options);
  void updateAndPrint(const Group& group, std::ostream* stream);
  bool hasPS() const;
  std::string getPS() const;
  std::string getRT() const;
  uint16_t getPI() const;
  std::string getCountryCode() const;

 private:
  void decodeBasics(const Group& group);
  void decodeType0(const Group& group);
  void decodeType1(const Group& group);
  void decodeType2(const Group& group);
  void decodeType3A(const Group& group);
  void decodeType4A(const Group& group);
  void decodeType6(const Group& group);
  void decodeType14A(const Group& group);
  void decodeType15B(const Group& group);
  void decodeODAgroup(const Group& group);
  void addAltFreq(uint8_t);
  void updatePS(int pos, std::vector<int> chars);
  void updateRadioText(int pos, std::vector<int> chars);
  void parseRadioTextPlus(const Group& group);
  uint16_t pi_;
  Options options_;
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
  bool linkage_la_;
  std::string clock_time_;
  bool has_country_;
  std::map<GroupType, uint16_t> oda_app_for_group_;
  bool has_rt_plus_;
  bool rt_plus_cb_;
  uint16_t rt_plus_scb_;
  uint16_t rt_plus_template_num_;
  bool rt_plus_toggle_;
  bool rt_plus_item_running_;
  std::map<uint16_t, RDSString> eon_ps_names_;
  bool last_block_had_pi_;

  int pager_pac_;
  int pager_opc_;
  int pager_tng_;
  int pager_ecc_;
  int pager_ccf_;
  int pager_interval_;

  Json::StreamWriterBuilder writer_builder_;
  std::unique_ptr<Json::StreamWriter> writer_;
  Json::Value json_;

#ifdef ENABLE_TMC
  tmc::TMC tmc_;
#endif
};

struct RTPlusTag {
  uint16_t content_type;
  uint16_t start;
  uint16_t length;
};

}  // namespace redsea
#endif // GROUPS_H_
