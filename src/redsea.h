#ifndef REDSEA_H_
#define REDSEA_H_

#include <vector>
#include <map>
#include <string>

struct Group {
  uint16_t pi;
  int type;
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
};

#endif
