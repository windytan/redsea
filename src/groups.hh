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
#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <map>
#include <string>

#include <nlohmann/json.hpp>

#include "src/maybe.hh"
#include "src/options.hh"
#include "src/rft.hh"
#include "src/text/radiotext.hh"
#include "src/text/rdsstring.hh"
#include "src/tmc/tmc.hh"
#include "src/util.hh"

namespace redsea {

// A scoped enum couldn't readily be used for indexing
enum eBlockNumber : std::uint8_t { BLOCK1, BLOCK2, BLOCK3, BLOCK4 };

enum class Offset : std::uint8_t { A, B, C, Cprime, D, invalid };

class Block {
 public:
  std::uint32_t raw{};
  std::uint16_t data{};
  bool is_received{false};
  bool had_errors{false};
  Offset offset{Offset::invalid};
};

class GroupType {
 public:
  enum class Version : std::uint8_t { A, B, C };

  GroupType() = default;
  explicit GroupType(std::uint16_t type_code);

  std::string str() const;

  std::uint16_t number{};
  Version version{Version::A};
};

bool operator<(const GroupType& type1, const GroupType& type2);

class Pager {
 public:
  int pac{};
  int opc{};
  int paging_code{};
  int ecc{};
  int ccf{};
  int interval{};
  void decode1ABlock4(std::uint16_t block4);
};

/*
 * A single RDS group transmitted as four 16-bit blocks.
 *
 */
class Group {
 public:
  Group() = default;

  std::uint16_t get(eBlockNumber block_num) const;
  bool has(eBlockNumber block_num) const;

  bool isEmpty() const;
  GroupType getType() const;
  bool hasType() const;
  std::uint16_t getPI() const;
  float getBLER() const;
  int getNumErrors() const;
  Maybe<double> getTimeFromStart() const;

  bool hasPI() const;
  bool hasBLER() const;
  bool hasRxTime() const;
  std::chrono::time_point<std::chrono::system_clock> getRxTime() const;
  void printHex(std::ostream& stream) const;
  void setVersionC();
  void setDataStream(std::uint32_t stream);
  std::uint32_t getDataStream() const;

  void disableOffsets();
  void setBlock(eBlockNumber block_num, Block block);
  void setRxTime(std::chrono::time_point<std::chrono::system_clock> t);
  void setAverageBLER(float bler);
  void setTimeFromStart(double time_from_start);

 private:
  GroupType type_;
  std::array<Block, 4> blocks_;
  std::chrono::time_point<std::chrono::system_clock> time_received_;
  float bler_{0.f};
  std::uint32_t data_stream_{0};
  // Seconds from the beginning of the file until the first bit of this group
  double time_from_start_{0.0};
  bool has_type_{false};
  bool has_c_prime_{false};
  bool has_bler_{false};
  bool has_rx_time_{false};
  bool has_time_from_start_{false};
  bool no_offsets_{false};
};

class Station {
 public:
  Station() = delete;
  Station(const Options& options, int which_channel, std::uint16_t pi);
  Station(const Options& options, int which_channel);
  void updateAndPrint(const Group& group, std::ostream& stream);
  std::uint16_t getPI() const;

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
  void decodeC(const Group& group);
  void parseEnhancedRT(const Group& group);
  void parseDAB(const Group& group);

  std::uint16_t pi_{};
  bool has_pi_{false};
  Options options_;
  int which_channel_{};
  ProgramServiceName ps_;
  LongPS long_ps_;
  RadioText radiotext_;
  RadioText ert_;
  PTYName ptyname_;
  RDSString full_tdc_{32U * 4U};
  std::uint16_t pin_{};
  std::uint16_t ecc_{};
  std::uint16_t cc_{};
  int tmc_id_{};
  bool linkage_la_{};
  std::string clock_time_;
  bool has_country_{false};
  std::map<GroupType, std::uint16_t> oda_app_for_group_;
  std::map<std::uint16_t, std::uint16_t> oda_app_for_pipe_;
  bool ert_uses_chartable_e3_{false};
  std::map<std::uint16_t, RDSString> eon_ps_names_;
  std::map<std::uint16_t, AltFreqList> eon_alt_freqs_;
  bool last_group_had_pi_{false};
  AltFreqList alt_freq_list_;
  Pager pager_;

  nlohmann::ordered_json json_;

  tmc::TMCService tmc_;

  // One RFT file per pipe
  std::array<RFTFile, 16> rft_file_;
};

void parseRadioTextPlus(const Group& group, RadioText& rt, nlohmann::ordered_json& json_el);

}  // namespace redsea
#endif  // GROUPS_H_
