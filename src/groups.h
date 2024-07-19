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

#include <array>
#include <chrono>
#include <iostream>
#include <map>
#include <memory>
#include <string>

#include <nlohmann/json.hpp>

#include "src/common.h"
#include "src/options.h"
#include "src/rdsstring.h"
#include "src/tmc/tmc.h"
#include "src/util.h"

namespace redsea {

// A scoped enum couldn't readily be used for indexing
enum eBlockNumber { BLOCK1, BLOCK2, BLOCK3, BLOCK4 };

enum class Offset { A, B, C, Cprime, D, invalid };

class Block {
 public:
  uint32_t raw{0};
  uint16_t data{0};
  bool is_received{false};
  bool had_errors{false};
  Offset offset{Offset::invalid};
};

class GroupType {
 public:
  enum class Version { A, B };

  GroupType() = default;
  explicit GroupType(uint16_t type_code);

  std::string str() const;

  uint16_t number{0x00};
  Version version{Version::A};
};

bool operator<(const GroupType& type1, const GroupType& type2);

class ProgramServiceName {
 public:
  ProgramServiceName() = default;
  void update(size_t pos, uint8_t byte1, uint8_t byte2) {
    text.set(pos, byte1, byte2);
  }

  RDSString text{8};
};

class LongPS {
 public:
  LongPS() {
    text.setEncoding(RDSString::Encoding::UTF8);
  }
  void update(size_t pos, uint8_t byte1, uint8_t byte2) {
    text.set(pos, byte1, byte2);
  }

  RDSString text{32};
};

class RadioText {
 public:
  RadioText() = default;
  bool isABChanged(int new_ab) {
    const bool is = (ab != new_ab);
    ab            = new_ab;
    return is;
  }
  void update(size_t pos, uint8_t byte1, uint8_t byte2) {
    text.set(pos, byte1, byte2);
  }

  struct Plus {
    bool exists{};
    bool cb{};
    uint16_t scb{};
    uint16_t template_num{};
    bool toggle{};
    bool item_running{};

    struct Tag {
      uint16_t content_type{};
      uint16_t start{};
      uint16_t length{};
    };
  };

  RDSString text{64};
  Plus plus;
  std::string previous_potentially_complete_message;
  int ab{};
};

class PTYName {
 public:
  PTYName() : text(8) {}
  bool isABChanged(int new_ab) {
    bool is = (ab != new_ab);
    ab      = new_ab;
    return is;
  }
  void update(size_t pos, uint8_t char1, uint8_t char2, uint8_t char3, uint8_t char4) {
    text.set(pos, char1, char2);
    text.set(pos + 2, char3, char4);
  }

  RDSString text;
  int ab{0};
};

class Pager {
 public:
  int pac{0};
  int opc{0};
  int paging_code{0};
  int ecc{0};
  int ccf{0};
  int interval{0};
  void decode1ABlock4(uint16_t block4);
};

/*
 * A single RDS group transmitted as four 16-bit blocks.
 *
 */
class Group {
 public:
  Group() = default;

  uint16_t get(eBlockNumber block_num) const;
  bool has(eBlockNumber block_num) const;

  bool isEmpty() const;
  GroupType getType() const;
  bool hasType() const;
  uint16_t getPI() const;
  float getBLER() const;
  int getNumErrors() const;
  bool hasPI() const;
  bool hasBLER() const;
  bool hasTime() const;
  std::chrono::time_point<std::chrono::system_clock> getRxTime() const;
  void printHex(std::ostream& stream) const;

  void disableOffsets();
  void setBlock(eBlockNumber block_num, Block block);
  void setTime(std::chrono::time_point<std::chrono::system_clock> t);
  void setAverageBLER(float bler);

 private:
  GroupType type_{};
  std::array<Block, 4> blocks_;
  std::chrono::time_point<std::chrono::system_clock> time_received_;
  float bler_{0.f};
  bool has_type_{false};
  bool has_c_prime_{false};
  bool has_bler_{false};
  bool has_time_{false};
  bool no_offsets_{false};
};

class Station {
 public:
  Station() = delete;
  Station(const Options& options, int which_channel, uint16_t _pi);
  Station(const Options& options, int which_channel);
  void updateAndPrint(const Group& group, std::ostream& stream);
  uint16_t getPI() const;

 private:
  void decodeBasics(const Group& group);
  void decodeType0(const Group& group);
  void decodeType1(const Group& group);
  void decodeType2(const Group& group);
  void decodeType3A(const Group& group);
  void decodeType4A(const Group& group);
  void decodeType5(const Group& group);
  void decodeType6(const Group& group);
  void decodeType7A(const Group& group);
  void decodeType9A(const Group& group);
  void decodeType10A(const Group& group);
  void decodeType14(const Group& group);
  void decodeType15A(const Group& group);
  void decodeType15B(const Group& group);
  void decodeODAGroup(const Group& group);
  void parseEnhancedRT(const Group& group);
  void parseDAB(const Group& group);

  uint16_t pi_{0x0000};
  bool has_pi_{false};
  Options options_;
  int which_channel_{0};
  ProgramServiceName ps_;
  LongPS long_ps_;
  RadioText radiotext_;
  RadioText ert_;
  PTYName ptyname_;
  RDSString full_tdc_{32 * 4};
  uint16_t pin_{0};
  uint16_t ecc_{0};
  uint16_t cc_{0};
  int tmc_id_{0};
  bool linkage_la_{0};
  std::string clock_time_{""};
  bool has_country_{false};
  std::map<GroupType, uint16_t> oda_app_for_group_;
  bool ert_uses_chartable_e3_{false};
  std::map<uint16_t, RDSString> eon_ps_names_;
  std::map<uint16_t, AltFreqList> eon_alt_freqs_;
  bool last_group_had_pi_{false};
  AltFreqList alt_freq_list_;
  Pager pager_;

  nlohmann::ordered_json json_;

  tmc::TMCService tmc_;
};

void parseRadioTextPlus(const Group& group, RadioText& rt, nlohmann::ordered_json& json_el);

}  // namespace redsea
#endif  // GROUPS_H_
