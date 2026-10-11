/*
 * Copyright (c) Vasyl Samoilov
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
#ifndef PAGING_HH_
#define PAGING_HH_

#include <cstdint>
#include <string>

#include "src/text/rdsstring.hh"

namespace redsea {

class Group;
class ObjectTree;

// RDS Radio Paging decoder (EN 50067 Annex M / IEC 62106).
//
// Radio Paging is carried in group type 7A (basic paging) with its enabling
// context announced in group type 1A, and optionally extended by group type
// 13A (Enhanced Radio Paging, not decoded here). Group 7A is shared with ODA:
// when a 3A group reassigns 7A to an Open Data Application, those 7A groups are
// that application's payload, not paging. The caller is responsible for routing
// a 7A group here only when it has NOT been ODA-reassigned (NRSC-4-B Table 3,
// which flags 7A as "Radio Paging or ODA").
//
// A page is signalled over one or more consecutive 7A groups selected by the
// 4-bit PSAC (Paging Segment Address Code) in block B. This class reassembles
// those segments into a complete message and emits the result into the output
// tree. Battery-saving and sub-network selection are described by the Radio
// Paging Code (RPC) carried in group 1A, which this class also tracks so a
// 7A group can be reported with its enabling context.
class RadioPaging {
 public:
  // Decode a single group 7A (block B bits 4-0 = A/B flag + PSAC, blocks C/D
  // = address or data). Writes under out["radio_paging"]. When show_partial is
  // set, an incomplete message pre-empted by a new call is still emitted (with
  // filler characters for the lost groups and an "incomplete" flag) before it
  // is discarded, matching redsea's partial_* convention.
  void decodeGroup7A(const Group& group, ObjectTree& out, bool show_partial);

  // Decode a single group 13A Enhanced Radio Paging address-notification group
  // (EN 50067 Annex M, Figures M.11a/M.11b, §M.3.4). Reached only when 13A has
  // not been reassigned to an ODA by a 3A group. Writes under
  // out["radio_paging"]["enhanced"].
  void decodeGroup13A(const Group& group, ObjectTree& out);

  // Decode the Radio Paging Code from a group 1A whose block B carries it
  // (EN 50067 M.2.1.2). rpc = block B bits 4-0. RPC == 0 means no basic paging
  // on this channel (Table M.1). Writes under out["radio_paging"]["rpc"].
  void decodeRPC(std::uint8_t rpc, ObjectTree& out);

  // Decode the Enhanced Paging Protocol System Information from a group 1A
  // slow-labelling variant (EN 50067 M.3.2, Figures M.9/M.10a/M.10b):
  //   variant 0: block C = LA | 000 | OPC (bits 11-8) | ECC (bits 7-0)
  //   variant 2: block C = 000000 | PAC (bits 9-4) | OPC (bits 3-0)
  // OPC == 0 means no enhanced paging service on the channel (M.3.2.2, Note 1).
  // Together with the ECC and PI country part this identifies the paging
  // network. Writes under out["radio_paging"]["system_info"].
  void decodeSystemInfo(int slow_label_variant, std::uint16_t block_c, ObjectTree& out);

  // Note that an accurate CT (group 4A) has been seen. EN 50067 M.2.1.1.1
  // requires 4A at the start of every minute on a paging transmission, and
  // §6.1.5.6 NOTE 8 makes accurate CT mandatory where radio paging is
  // implemented. This is a non-blocking conformance signal: a page still
  // decodes without it.
  void noteClockTimePresent();

  // True once an accurate CT (group 4A) has been observed on this station.
  [[nodiscard]] bool hasClockTime() const {
    return has_clock_time_;
  }

 private:
  // PSAC values (EN 50067 Annex M Table M.2).
  enum Psac : std::uint8_t {
    kPsacTone       = 0x0,  // Tone-only (1 group)
    kPsacFuncAddr   = 0x1,  // International functions: address group (M.2.1.6.6)
    kPsacNum10Addr  = 0x2,  // 10-digit numeric: address group
    kPsacNum10Data  = 0x3,  // 10-digit numeric data; intl-functions data group
    kPsacNum18Addr  = 0x4,  // 18-digit numeric: address group
    kPsacNum18Data1 = 0x5,  // 18-digit numeric: data group 1
    kPsacNum18Data2 = 0x6,  // 18-digit numeric: data group 2
    kPsacIntl15     = 0x7,  // International 15-digit
    kPsacAlphaAddr  = 0x8,  // Alphanumeric/variable-numeric: address group
    kPsacAlphaData1 = 0x9,  // Alpha/vnum: first data group (cycles 0x9..0xE)
    kPsacAlphaDataN = 0xE,  // Alpha/vnum: last data group
    kPsacAlphaEnd   = 0xF   // Alpha/vnum: end-of-message
  };

  // Variable-length message type, selected by control-byte bits 7-6 for the
  // PSAC-1XXX message family (EN 50067 Table M.12).
  enum class VarType : std::uint8_t {
    kNone,       // Not a variable-length message (tone / basic numeric)
    kAlpha,      // 00: alphanumeric (8-bit characters, Annex E)
    kNumeric,    // 01: variable-length numeric (hex nibbles, A = space)
    kFunctions,  // 11: variable-length functions (hex nibbles 0-F)
  };

  // Basic (fixed-length) numeric message type, tracked to disambiguate the
  // PSAC-0XXX family: 10-digit (PSAC 2/3), 18-digit (4/5/6), and 15-digit
  // international (7/5/6). PSACs 5 and 6 overlap between 18-digit and 15-digit;
  // the address-group PSAC resolves which (Table M.2 / Table M.5).
  enum class BasicNumType : std::uint8_t {
    kNone,      // Not in basic-numeric assembly
    kNum10,     // 10-digit national
    kNum18,     // 18-digit national
    kIntl15,    // 15-digit international (M.2.1.6.5)
    kIntlFunc,  // International functions message (M.2.1.6.6, PSAC 1 + 3)
  };

  void beginMessage(std::uint32_t address, std::uint8_t ctrl, VarType type);
  void appendCharGroup(std::uint16_t block_c, std::uint16_t block_d);
  void appendNibbleGroup(std::uint16_t block_c, std::uint16_t block_d);
  void appendNibbleBlock(std::uint16_t block);
  void finishMessage(ObjectTree& out);
  void emitPartial(ObjectTree& out);
  void reset();

  // Reassembly state.
  bool assembling_{false};
  VarType var_type_{VarType::kNone};                  // Variable-length message type under assembly
  BasicNumType basic_num_type_{BasicNumType::kNone};  // Basic numeric type under assembly
  std::uint32_t address_{};        // 6-digit address of the message under assembly
  bool ab_flag_{};                 // A/B flag of the message under assembly
  bool incomplete_{};              // A group was lost mid-message (filler inserted)
  std::uint8_t next_data_psac_{};  // Expected PSAC of the next data group (0 = none yet)

  // Alphanumeric payload is decoded through the RDS basic character set
  // (EN 50067 Annex E), the same code table used for RadioText/PS, via
  // RDSString. Numeric payload is BCD digits, accumulated verbatim.
  RDSString alpha_message_{80};  // EN 50067 M.3.5: max 80 alphanumeric chars
  std::size_t alpha_pos_{};      // Next character position in alpha_message_
  std::string numeric_message_;  // Accumulated BCD digits

  // Last decoded control byte (EN 50067 M.3.5, Tables M.12/M.13).
  bool have_control_{false};
  std::uint8_t call_counter_{};  // P3-P0 (1-15, 0 = not implemented)
  bool is_repeat_{};             // R flag
  bool is_international_{};      // NI flag (alpha/vnum only)
  std::uint8_t tone_ext_{};      // E2-E0: operator-defined extended field (tone-only)

  // International paging (EN 50067 M.3.5.7, Figures M.17-M.19): the first data
  // group (PSAC 1001) carries a CCITT E.212 country code + Operator Code in
  // block C instead of message characters.
  bool expect_intl_header_{false};  // Next data group is the country/OPC header
  std::uint16_t country_code_{};    // 3-digit E.212 country code (0 = national)
  std::uint8_t opc_{};              // Operator Code (4 bits)

  // Radio Paging Code context (group 1A, EN 50067 M.2.1.2).
  bool have_rpc_{false};
  std::uint8_t rpc_{};

  // Enhanced Paging Protocol System Information (group 1A, EN 50067 M.3.2):
  // the Operator Code gates EPP/international paging (OPC == 0 -> no EPP).
  bool have_opc_{false};
  std::uint8_t si_opc_{};  // Operator Code (4 bits; 0 = no enhanced paging)
  std::uint8_t si_pac_{};  // Paging Area Code (6 bits; variant 2 only)
  bool have_pac_{false};

  // Clock-time conformance (group 4A, EN 50067 M.2.1.1.1 / §6.1.5.6 NOTE 8).
  bool has_clock_time_{false};

  // Enhanced paging (group 13A) 50-bit reassembly: STY=001 carries the high
  // half (notification bits 49-25), STY=010 the low half (24-0), in two
  // consecutive groups (EN 50067 Figure M.11b).
  bool have_notify_hi_{false};
  std::uint32_t notify_hi_{};  // Bits 49-25 from the STY=001 group
};

}  // namespace redsea

#endif  // PAGING_HH_
