/*
 * Copyright (c) Oona Räisänen
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 */
#ifndef GROUPS_H_
#define GROUPS_H_

#include <chrono>

#include <array>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include <json/json.h>

#include "config.h"
#include "src/common.h"
#include "src/options.h"
#include "src/rdsstring.h"
#include "src/tmc/tmc.h"
#include "src/util.h"

namespace redsea {

// A scoped enum couldn't readily be used for indexing
enum eBlockNumber {
  BLOCK1, BLOCK2, BLOCK3, BLOCK4
};

enum class GroupTypeVersion {
  A, B
};

enum class Offset {
  A, B, C, Cprime, D, invalid
};

class Block {
 public:
  uint32_t raw         { 0 };
  uint16_t data        { 0 };
  bool     is_received { false };
  bool     had_errors  { false };
  Offset   offset      { Offset::invalid };
};

class GroupType {
 public:
  explicit GroupType(uint16_t type_code = 0x00);

  bool operator==(const GroupType& other);

  std::string str() const;

  uint16_t number;
  GroupTypeVersion version;
};

bool operator<(const GroupType& type1, const GroupType& type2);

class ProgramServiceName {
  public:
   ProgramServiceName() : text(8) {}
   void Update(int pos, RDSChar char1, RDSChar char2) {
     text.set(pos, char1, char2);
   }

   RDSString text;
};

class RadioText {
 public:
  RadioText() : text(64) {}
  bool is_ab_changed(int new_ab) {
    bool is = (ab != new_ab);
    ab = new_ab;
    return is;
  }
  void Update(int pos, RDSChar char1, RDSChar char2) {
    text.set(pos, char1, char2);
  }

  RDSString text;
  int       ab { 0 };
};

class RadioTextPlus {
 public:
  bool     cb           { false };
  uint16_t scb          { 0 };
  uint16_t template_num { 0 };
  bool     toggle       { false };
  bool     item_running { false };
};

class Pager {
 public:
  int pac         { 0 };
  int opc         { 0 };
  int paging_code { 0 };
  int ecc         { 0 };
  int ccf         { 0 };
  int interval    { 0 };
  void Decode1ABlock4(uint16_t block4);
};

class Group {
 public:
  Group();

  uint16_t block(eBlockNumber block_num) const;
  uint16_t block1() const;
  uint16_t block2() const;
  uint16_t block3() const;
  uint16_t block4() const;

  bool has(eBlockNumber block_num) const;
  bool empty() const;
  GroupType type() const;
  bool has_type() const;
  uint16_t pi() const;
  uint8_t bler() const;
  uint8_t num_errors() const;
  bool has_pi() const;
  bool has_bler() const;
  bool has_time() const;
  std::chrono::time_point<std::chrono::system_clock> rx_time() const;
  void PrintHex(std::ostream* stream,
                const std::string& time_format) const;

  void disable_offsets();
  void set_block(eBlockNumber block_num, Block block);
  void set_time(std::chrono::time_point<std::chrono::system_clock> t);
  void set_bler(uint8_t bler);
  void set_channel(int which_channel);

 private:
  GroupType type_;
  uint16_t pi_      { 0x0000 };
  std::array<Block, 4> blocks_;
  std::chrono::time_point<std::chrono::system_clock> time_received_;
  uint8_t bler_     { 0 };
  bool has_type_    { false };
  bool has_pi_      { false };
  bool has_c_prime_ { false };
  bool has_bler_    { false };
  bool has_time_    { false };
  bool no_offsets_  { false };
};

class Station {
 public:
  Station();
  Station(uint16_t _pi, const Options& options, int which_channel);
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
  void ParseRadioTextPlus(const Group& group);

  uint16_t pi_             { 0 };
  Options options_;
  int which_channel_       { 0 };
  ProgramServiceName ps_;
  RadioText radiotext_;
  int pin_                 { 0 };
  int ecc_                 { 0 };
  int cc_                  { 0 };
  int tmc_id_              { 0 };
  bool linkage_la_         { 0 };
  std::string clock_time_  { "" };
  bool has_country_        { false };
  std::map<GroupType, uint16_t> oda_app_for_group_;
  bool has_radiotext_plus_ { false };
  RadioTextPlus radiotext_plus_;
  std::map<uint16_t, RDSString> eon_ps_names_;
  std::map<uint16_t, AltFreqList> eon_alt_freqs_;
  bool last_group_had_pi_  { false };
  AltFreqList alt_freq_list_;
  Pager pager_;

  Json::StreamWriterBuilder writer_builder_;
  std::unique_ptr<Json::StreamWriter> writer_;
  Json::Value json_;

#ifdef ENABLE_TMC
  tmc::TMCService tmc_;
#endif
};

class RTPlusTag {
 public:
  uint16_t content_type;
  uint16_t start;
  uint16_t length;
};

}  // namespace redsea
#endif  // GROUPS_H_
