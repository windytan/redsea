#ifndef GROUPS_H_
#define GROUPS_H_

#include <string>
#include <vector>

enum {
  TYPE_A, TYPE_B
};

struct Group {
  Group(std::vector<uint16_t>);
  void decode0(std::vector<uint16_t>);
  void decode1(std::vector<uint16_t>);
  uint16_t pi;
  int type;
  int type_ab;
  bool tp;
  bool ta;
  bool is_music;
  int pty;
  int di_address;
  int di;
  int num_altfreqs;
  std::vector<double> altfreqs;
  int ps_position;
  std::string ps_chars;
  int pin;
  bool has_pager;
  int pager_tng;
  int pager_interval;
  bool linkage_la;
  int pager_opc;
  int pager_pac;
  int pager_ecc;
  int pager_ccf;
  int ecc;
  int cc;
  int tmc_id;
  int lang;
  int ews_channel;
};


#endif // GROUPS_H_
