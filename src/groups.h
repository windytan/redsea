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
#include "src/util.h"

namespace redsea {

enum eBlockNumber {
  BLOCK1, BLOCK2, BLOCK3, BLOCK4
};

enum eGroupTypeVersion {
  VERSION_A, VERSION_B
};

enum eOffset {
  OFFSET_A, OFFSET_B, OFFSET_C, OFFSET_CI, OFFSET_D, OFFSET_INVALID
};

class AltFreqList {
 public:
  AltFreqList();
  void insert(uint8_t code);
  bool complete() const;
  std::set<CarrierFrequency> get() const;
  void clear();

 private:
  std::set<CarrierFrequency> alt_freqs_;
  int num_alt_freqs_;
  bool lf_mf_follows_;
};

class GroupType {
 public:
  GroupType(uint16_t type_code = 0x00);
  GroupType(const GroupType& obj);

  bool operator==(const GroupType& other);

  std::string str() const;

  uint16_t num;
  eGroupTypeVersion ab;
};

bool operator<(const GroupType& obj1, const GroupType& obj2);

class Group {
 public:
  Group();
  uint16_t block(eBlockNumber blocknum) const;
  bool has(eBlockNumber blocknum) const;
  bool empty() const;
  GroupType type() const;
  bool has_type() const;
  uint16_t pi() const;
  bool has_pi() const;
  void set(eBlockNumber blocknum, uint16_t data);
  void set_ci(uint16_t data);

 private:
  GroupType type_;
  uint16_t pi_;
  std::vector<bool> has_block_;
  std::vector<uint16_t> block_;
  bool has_type_;
  bool has_pi_;
  bool has_ci_;
};

class Station {
 public:
  Station();
  Station(uint16_t pi, Options options);
  void UpdateAndPrint(const Group& group, std::ostream* stream);
  uint16_t pi() const;

 private:
  void DecodeBasics(const Group& group);
  void DecodeType0(const Group& group);
  void DecodeType1(const Group& group);
  void DecodeType2(const Group& group);
  void DecodeType3A(const Group& group);
  void DecodeType4A(const Group& group);
  void DecodeType6(const Group& group);
  void DecodeType14(const Group& group);
  void DecodeType15B(const Group& group);
  void DecodeODAGroup(const Group& group);
  void AddAltFreq(uint8_t);
  void UpdatePS(int pos, int chr1, int chr2);
  void UpdateRadioText(int pos, int chr1, int chr2);
  void ParseRadioTextPlus(const Group& group);
  uint16_t pi_;
  Options options_;
  RDSString ps_;
  RDSString radiotext_;
  int radiotext_ab_;
  int pty_;
  bool is_tp_;
  bool is_ta_;
  bool is_music_;
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
  bool has_radiotext_plus_;
  bool radiotext_plus_cb_;
  uint16_t radiotext_plus_scb_;
  uint16_t radiotext_plus_template_num_;
  bool radiotext_plus_toggle_;
  bool radiotext_plus_item_running_;
  std::map<uint16_t, RDSString> eon_ps_names_;
  std::map<uint16_t, AltFreqList> eon_alt_freqs_;
  bool last_block_had_pi_;
  AltFreqList alt_freq_list_;

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

void PrintHexGroup(const Group& group, std::ostream* stream);

}  // namespace redsea
#endif // GROUPS_H_
