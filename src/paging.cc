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
#include "src/paging.hh"

#include <array>
#include <cstdint>
#include <string>
#include <string_view>

#include "src/group.hh"
#include "src/util/tree.hh"
#include "src/util/util.hh"

namespace redsea {

namespace {

// Group designation address ranges (EN 50067 Table M.1), indexed by the group
// designation (RPC bits 4-2). Designation 0 means no basic paging.
constexpr std::array<std::string_view, 8> kRpcDesignationRanges{"none",  "00-99", "00-39", "40-69",
                                                                "70-99", "00-19", "20-39", "40-59"};

// Decode one BCD paging nibble (EN 50067 Annex M). 0-9 are digits, 0xA is a
// space; 0xB-0xF are undefined and shown as space.
char bcdNibbleToChar(std::uint8_t nibble) {
  if (nibble <= 0x9)
    return static_cast<char>('0' + nibble);
  return ' ';
}

// Filler emitted in place of a message group lost to block errors. redsea uses
// '?' for undecodable characters (see sanitizeUtf8); the paging decoder follows
// that convention so a gap is visible rather than silently closed.
constexpr char kFiller{'?'};

// Append the four BCD nibbles of a 16-bit block, most significant first.
void appendBcdBlock(std::string& out, std::uint16_t block) {
  out += bcdNibbleToChar(getBits<4>(block, 12));
  out += bcdNibbleToChar(getBits<4>(block, 8));
  out += bcdNibbleToChar(getBits<4>(block, 4));
  out += bcdNibbleToChar(getBits<4>(block, 0));
}

// Unpack the 6-digit paging address from block C and the high byte of block D
// (EN 50067 Annex M). Address digits are Y1 Y2 Z1 Z2 Z3 Z4.
std::uint32_t unpackAddress(std::uint16_t block_c, std::uint16_t block_d) {
  std::uint32_t addr = 0;
  addr += static_cast<std::uint32_t>(getBits<4>(block_c, 12)) * 100000U;
  addr += static_cast<std::uint32_t>(getBits<4>(block_c, 8)) * 10000U;
  addr += static_cast<std::uint32_t>(getBits<4>(block_c, 4)) * 1000U;
  addr += static_cast<std::uint32_t>(getBits<4>(block_c, 0)) * 100U;
  addr += static_cast<std::uint32_t>(getBits<4>(block_d, 12)) * 10U;
  addr += static_cast<std::uint32_t>(getBits<4>(block_d, 8));
  return addr;
}

}  // namespace

void RadioPaging::reset() {
  assembling_     = false;
  var_type_       = VarType::kNone;
  basic_num_type_ = BasicNumType::kNone;
  // finishMessage() shrinks alpha_message_ to the received length before str(),
  // so restore the full EN 50067 M.3.5 80-character capacity for the next
  // message rather than only clearing it.
  alpha_message_  = RDSString{80};
  alpha_pos_      = 0;
  numeric_message_.clear();
  incomplete_         = false;
  next_data_psac_     = 0;
  expect_intl_header_ = false;
  country_code_       = 0;
  opc_                = 0;
}

void RadioPaging::decodeRPC(std::uint8_t rpc, ObjectTree& out) {
  have_rpc_ = true;
  rpc_      = rpc & 0x1FU;

  const std::uint8_t designation = (static_cast<unsigned>(rpc_) >> 2U) & 0x07U;
  const std::uint8_t sync        = rpc_ & 0x03U;

  auto& rpc_out             = out["radio_paging"]["rpc"];
  rpc_out["value"]          = static_cast<int>(rpc_);
  rpc_out["has_paging"]     = (rpc_ != 0);
  rpc_out["group_range"]    = std::string(kRpcDesignationRanges.at(designation));
  rpc_out["battery_saving"] = static_cast<int>(sync);
}

void RadioPaging::decodeSystemInfo(int slow_label_variant, std::uint16_t block_c, ObjectTree& out) {
  // Operator Code and Paging Area Code (EN 50067 M.3.2, Figures M.10a/M.10b).
  //   variant 0: OPC = block C bits 11-8
  //   variant 2: PAC = block C bits 9-4, OPC = block C bits 3-0 (SI sub-type 0)
  if (slow_label_variant == 0) {
    si_opc_   = getBits<4>(block_c, 8);
    have_opc_ = true;
  } else if (slow_label_variant == 2) {
    si_pac_   = getBits<6>(block_c, 4);
    si_opc_   = getBits<4>(block_c, 0);
    have_opc_ = true;
    have_pac_ = true;
  } else {
    return;
  }

  // OPC == 0 means no enhanced paging service on the channel (M.3.2.2, Note 1).
  auto& si              = out["radio_paging"]["system_info"];
  si["operator_code"]   = static_cast<int>(si_opc_);
  si["enhanced_paging"] = (si_opc_ != 0);
  if (have_pac_)
    si["paging_area_code"] = static_cast<int>(si_pac_);
}

void RadioPaging::noteClockTimePresent() {
  has_clock_time_ = true;
}

// Group 13A Enhanced Radio Paging address notification (EN 50067 Annex M,
// Figures M.11a/M.11b, Tables M.9/M.10, §M.3.4). Field layout (ground truth,
// EN 50067:1998 p.110-111):
//   Block B: bits 4-3 = CS (cycle selection), bits 2-0 = STY (sub-type)
//   Block C: bits 15-12 = IT (interval), bit 11 = S1, bit 10 = S2,
//            bit 9 = X (reserved), bits 8-0 = notification bits 24..16
//   Block D: bits 15-0 = notification bits 15..0
// STY 000 = 25-bit (one group); STY 001/010 = 50-bit high/low halves (two
// groups); STY 011 = Value Added Services; 1xx reserved.
void RadioPaging::decodeGroup13A(const Group& group, ObjectTree& out) {
  if (!(group.has(BLOCK3) && group.has(BLOCK4)))
    return;

  const std::uint8_t cs  = getBits<2>(group.get(BLOCK2), 3);
  const std::uint8_t sty = getBits<3>(group.get(BLOCK2), 0);

  auto& enhanced = out["radio_paging"]["enhanced"];

  // CS (cycle selection, Table M.9).
  switch (cs) {
    case 0b00: enhanced["cycle"] = "1min"; break;
    case 0b10: enhanced["cycle"] = "2min_even"; break;
    case 0b11: enhanced["cycle"] = "2min_odd"; break;
    default:   enhanced["cycle"] = "reserved"; break;  // 0b01
  }

  const std::uint8_t interval        = getBits<4>(group.get(BLOCK3), 12);
  const bool s1                      = getBool(group.get(BLOCK3), 11);
  const bool s2                      = getBool(group.get(BLOCK3), 10);
  // bit 9 = X (reserved); bits 8-0 = notification bits 24..16.
  const std::uint32_t notify_lo_bits = getBits<9>(group.get(BLOCK3), 0);

  enhanced["interval"] = static_cast<int>(interval);

  // Sort indicator (Table M.10): S1 S2.
  if (!s1 && !s2)
    enhanced["sort"] = "none";
  else if (s1 && !s2)
    enhanced["sort"] = "ascending";
  else if (s1 && s2)
    enhanced["sort"] = "descending";
  else
    enhanced["sort"] = "reserved";  // S1=0 S2=1

  // 25 notification bits: block C bits 8-0 (24..16) + block D bits 15-0 (15..0).
  const std::uint32_t notify25 =
      (notify_lo_bits << 16U) | static_cast<std::uint32_t>(group.get(BLOCK4));

  switch (sty) {
    case 0b000:
      // Single group: 25 notification bits.
      enhanced["sub_type"]          = "notification_25bit";
      enhanced["notification_bits"] = static_cast<int>(notify25 & 0x1FFFFFFU);
      have_notify_hi_               = false;
      break;

    case 0b001:
      // 50-bit mode, high half (bits 49..25). Hold for the next group.
      enhanced["sub_type"] = "notification_50bit_high";
      notify_hi_           = notify25 & 0x1FFFFFFU;
      have_notify_hi_      = true;
      break;

    case 0b010: {
      // 50-bit mode, low half (bits 24..0). Combine with the held high half.
      // The 50-bit value is reported as its two 25-bit halves; each fits an int.
      enhanced["sub_type"]    = "notification_50bit_low";
      const std::uint32_t low = notify25 & 0x1FFFFFFU;
      if (have_notify_hi_) {
        enhanced["notification_bits_hi"] = static_cast<int>(notify_hi_);
        enhanced["notification_bits_lo"] = static_cast<int>(low);
      }
      have_notify_hi_ = false;
      break;
    }

    case 0b011: enhanced["sub_type"] = "value_added_services"; break;

    default:    enhanced["sub_type"] = "reserved"; break;
  }
}

void RadioPaging::decodeGroup7A(const Group& group, ObjectTree& out, bool show_partial) {
  if (!(group.has(BLOCK3) && group.has(BLOCK4)))
    return;

  const bool ab_flag          = getBool(group.get(BLOCK2), 4);
  const std::uint8_t psac     = getBits<4>(group.get(BLOCK2), 0);
  const std::uint16_t block_c = group.get(BLOCK3);
  const std::uint16_t block_d = group.get(BLOCK4);

  auto& paging = out["radio_paging"];

  // Non-blocking conformance context: accurate CT (group 4A) is required on a
  // paging transmission (EN 50067 M.2.1.1.1), and basic paging must be enabled
  // via a non-zero RPC in group 1A (M.2.1.2). Report both without suppressing
  // the decode of this call.
  paging["conformance"]["has_clock_time"] = has_clock_time_;
  if (have_rpc_)
    paging["conformance"]["paging_enabled"] = (rpc_ != 0);
  if (have_opc_)
    paging["conformance"]["enhanced_paging"] = (si_opc_ != 0);

  // A/B flag toggles for every new call to the same address. A changed flag
  // mid-assembly means a new call started: emit the partial (if requested)
  // before abandoning it.
  if (assembling_ && ab_flag != ab_flag_) {
    if (show_partial)
      emitPartial(out);
    else
      reset();
  }

  // Any address/tone group (PSAC 0,1,2,4,7,8) arriving mid-assembly pre-empts
  // the in-progress message (its end-of-message marker was lost). Emit the
  // partial before the address case resets and starts the new one.
  const bool is_start_group =
      (psac == kPsacTone || psac == kPsacFuncAddr || psac == kPsacNum10Addr ||
       psac == kPsacNum18Addr || psac == kPsacIntl15 || psac == kPsacAlphaAddr);
  if (assembling_ && is_start_group && show_partial)
    emitPartial(out);

  switch (psac) {
    case kPsacTone:
      // Tone-only: a single group carries the address and control byte.
      reset();
      beginMessage(unpackAddress(block_c, block_d), getUint8(block_d, 0), VarType::kNone);
      ab_flag_ = ab_flag;
      {
        auto& call        = paging["call"];
        call["type"]      = "tone";
        call["address"]   = address_;
        call["call_flag"] = ab_flag_ ? "B" : "A";
        call["is_repeat"] = is_repeat_;
        if (call_counter_ != 0)
          call["call_counter"] = static_cast<int>(call_counter_);
        // E2-E0 extended field (Table M.12); operator-defined, report when set.
        if (tone_ext_ != 0)
          call["tone_extended"] = static_cast<int>(tone_ext_);
      }
      reset();
      break;

    case kPsacNum10Addr:
    case kPsacNum18Addr:
      // National numeric address group (EN 50067 Fig M.3/M.4): address in C +
      // D high byte, first two message digits in D low byte. No control byte
      // for basic numeric: the lower byte carries digits.
      reset();
      assembling_     = true;
      var_type_       = VarType::kNone;
      basic_num_type_ = (psac == kPsacNum10Addr) ? BasicNumType::kNum10 : BasicNumType::kNum18;
      address_        = unpackAddress(block_c, block_d);
      ab_flag_        = ab_flag;
      have_control_   = false;
      numeric_message_.clear();
      numeric_message_ += bcdNibbleToChar(getBits<4>(block_d, 4));
      numeric_message_ += bcdNibbleToChar(getBits<4>(block_d, 0));
      break;

    case kPsacIntl15:
      // International 15-digit numeric address group (EN 50067 Fig M.6, Table
      // M.5): address in C + D high byte; D low byte = country-code digits
      // X1 (bits 7-4) and X2 (bits 3-0). The message starts in group 2.
      reset();
      assembling_       = true;
      var_type_         = VarType::kNone;
      basic_num_type_   = BasicNumType::kIntl15;
      address_          = unpackAddress(block_c, block_d);
      ab_flag_          = ab_flag;
      have_control_     = false;
      is_international_ = true;
      numeric_message_.clear();
      // Country code X1 X2 (X3 arrives in group 2); accumulate as BCD digits.
      country_code_ =
          static_cast<std::uint16_t>(getBits<4>(block_d, 4) * 100U + getBits<4>(block_d, 0) * 10U);
      break;

    case kPsacFuncAddr:
      // International functions message address group (EN 50067 Fig M.7, Table
      // M.6, §M.2.1.6.6): address in C + D high byte; D low byte = country-code
      // digits X1 (bits 7-4) and X2 (bits 3-0). Functions follow in group 2.
      reset();
      assembling_       = true;
      var_type_         = VarType::kNone;
      basic_num_type_   = BasicNumType::kIntlFunc;
      address_          = unpackAddress(block_c, block_d);
      ab_flag_          = ab_flag;
      have_control_     = false;
      is_international_ = true;
      numeric_message_.clear();
      country_code_ =
          static_cast<std::uint16_t>(getBits<4>(block_d, 4) * 100U + getBits<4>(block_d, 0) * 10U);
      break;

    case kPsacNum10Data:
      // PSAC 0011: 10-digit national data group, OR the international functions
      // data group (X3 + F1-F7). Disambiguated by the address group.
      if (!assembling_)
        break;
      if (basic_num_type_ == BasicNumType::kNum10) {
        appendBcdBlock(numeric_message_, block_c);
        appendBcdBlock(numeric_message_, block_d);
        if (numeric_message_.size() >= 10)
          finishMessage(out);
      } else if (basic_num_type_ == BasicNumType::kIntlFunc) {
        // Block C bits 15-12 = X3 (completes country code); then F1 F2 F3
        // (bits 11-0) and F4-F7 (block D), as full 0-F hex functions digits.
        country_code_ = static_cast<std::uint16_t>(country_code_ + getBits<4>(block_c, 12));
        static constexpr std::string_view kHex{"0123456789ABCDEF"};
        numeric_message_ += kHex[getBits<4>(block_c, 8)];
        numeric_message_ += kHex[getBits<4>(block_c, 4)];
        numeric_message_ += kHex[getBits<4>(block_c, 0)];
        numeric_message_ += kHex[getBits<4>(block_d, 12)];
        numeric_message_ += kHex[getBits<4>(block_d, 8)];
        numeric_message_ += kHex[getBits<4>(block_d, 4)];
        numeric_message_ += kHex[getBits<4>(block_d, 0)];
        finishMessage(out);  // F1-F7 complete the single functions data group.
      }
      break;

    case kPsacNum18Data1:
      // PSAC 0101: 18-digit national data group 1, OR 15-digit international
      // group 2 (X3 + digits A1-A7). Disambiguated by the address group.
      if (!assembling_)
        break;
      if (basic_num_type_ == BasicNumType::kNum18) {
        appendBcdBlock(numeric_message_, block_c);
        appendBcdBlock(numeric_message_, block_d);
      } else if (basic_num_type_ == BasicNumType::kIntl15) {
        // Block C bits 15-12 = X3 (completes the 3-digit country code), then
        // A1 A2 A3 (bits 11-0); block D = A4 A5 A6 A7.
        country_code_ = static_cast<std::uint16_t>(country_code_ + getBits<4>(block_c, 12));
        numeric_message_ += bcdNibbleToChar(getBits<4>(block_c, 8));
        numeric_message_ += bcdNibbleToChar(getBits<4>(block_c, 4));
        numeric_message_ += bcdNibbleToChar(getBits<4>(block_c, 0));
        appendBcdBlock(numeric_message_, block_d);
      }
      break;

    case kPsacNum18Data2:
      // PSAC 0110: 18-digit national data group 2, OR 15-digit international
      // group 3 (digits A8-A15). Disambiguated by the address group.
      if (!assembling_)
        break;
      if (basic_num_type_ == BasicNumType::kNum18) {
        appendBcdBlock(numeric_message_, block_c);
        appendBcdBlock(numeric_message_, block_d);
        if (numeric_message_.size() >= 18)
          finishMessage(out);
      } else if (basic_num_type_ == BasicNumType::kIntl15) {
        appendBcdBlock(numeric_message_, block_c);
        appendBcdBlock(numeric_message_, block_d);
        if (numeric_message_.size() >= 15)
          finishMessage(out);
      }
      break;

    case kPsacAlphaAddr: {
      // Variable-length address group (EN 50067 M.3.5): address in C + D high
      // byte, control byte in D low byte. Control-byte bits 7-6 select the
      // message type (Table M.12): 00 alpha, 01 numeric, 11 functions.
      reset();
      const std::uint8_t ctrl = getUint8(block_d, 0);
      const std::uint8_t mt   = (static_cast<unsigned>(ctrl) >> 6U) & 0x03U;
      const VarType type      = (mt == 0b01)   ? VarType::kNumeric
                                : (mt == 0b11) ? VarType::kFunctions
                                               : VarType::kAlpha;
      beginMessage(unpackAddress(block_c, block_d), ctrl, type);
      ab_flag_            = ab_flag;
      assembling_         = true;
      next_data_psac_     = kPsacAlphaData1;  // First expected data group PSAC (0x9)
      // International (NI=1): the next data group is a country-code + OPC header
      // (EN 50067 M.3.5.7, Fig M.17-M.19), not message content.
      expect_intl_header_ = is_international_;
      break;
    }

    case kPsacAlphaData1:
    case 0xA:
    case 0xB:
    case 0xC:
    case 0xD:
    case kPsacAlphaDataN:
      // Variable-length data group (cyclic PSAC 0x9-0xE).
      if (!assembling_ || var_type_ == VarType::kNone)
        break;
      if (expect_intl_header_) {
        // International header group (EN 50067 M.3.5.7): block C = I1 I2 I3 OPC
        // (CCITT E.212 country code in bits 15-4, Operator Code in bits 3-0).
        country_code_ = static_cast<std::uint16_t>(
            getBits<4>(block_c, 12) * 100U + getBits<4>(block_c, 8) * 10U + getBits<4>(block_c, 4));
        opc_                = getBits<4>(block_c, 0);
        expect_intl_header_ = false;
        // Block D carries the first message content of the header group.
        // Alpha: 2 characters (Fig M.17). Numeric/functions: 4 nibbles (Fig
        // M.18/M.19), coded per type (numeric A = space, functions full 0-F).
        if (var_type_ == VarType::kAlpha) {
          alpha_message_.set(alpha_pos_++, getUint8(block_d, 8));
          alpha_message_.set(alpha_pos_++, getUint8(block_d, 0));
        } else {
          appendNibbleBlock(block_d);
        }
        next_data_psac_ = (psac == kPsacAlphaDataN) ? static_cast<std::uint8_t>(kPsacAlphaData1)
                                                    : static_cast<std::uint8_t>(psac + 1);
        break;
      }
      // Detect data groups lost to block errors: the PSAC cycles 0x9..0xE, so a
      // jump past the expected value means one or more groups were dropped.
      // Fill each lost group with filler so the gap is visible and the message
      // stays positionally aligned (EN 50067 Fig M.5/M.14-M.16).
      if (next_data_psac_ != 0 && psac != next_data_psac_) {
        for (std::uint8_t p = next_data_psac_; p != psac;
             p              = (p == kPsacAlphaDataN) ? static_cast<std::uint8_t>(kPsacAlphaData1)
                                                     : static_cast<std::uint8_t>(p + 1)) {
          incomplete_ = true;
          if (var_type_ == VarType::kAlpha) {
            for (int i = 0; i < 4; i++)
              alpha_message_.set(alpha_pos_++, static_cast<std::uint8_t>(kFiller));
          } else {
            numeric_message_.append(8, kFiller);
          }
        }
      }
      if (var_type_ == VarType::kAlpha)
        appendCharGroup(block_c, block_d);
      else
        appendNibbleGroup(block_c, block_d);
      next_data_psac_ = (psac == kPsacAlphaDataN) ? static_cast<std::uint8_t>(kPsacAlphaData1)
                                                  : static_cast<std::uint8_t>(psac + 1);
      break;

    case kPsacAlphaEnd:
      // End-of-message marker.
      if (assembling_ && var_type_ != VarType::kNone)
        finishMessage(out);
      else
        reset();
      break;

    default:
      // All defined PSAC values have an explicit case above; this is a safety
      // net for an impossible value. Report the raw segment, never drop data.
      paging["debug"] = std::string("unhandled PSAC ") + std::to_string(psac);
      break;
  }
}

void RadioPaging::beginMessage(std::uint32_t address, std::uint8_t ctrl, VarType type) {
  address_      = address;
  var_type_     = type;
  have_control_ = true;
  alpha_message_.clear();
  alpha_pos_ = 0;
  numeric_message_.clear();

  // Control byte X1X2 (EN 50067 Table M.12, Table M.13). Bits 7-0:
  //   Tone-only (PSAC 0000):  E2 E1 E0 R P3 P2 P1 P0
  //     - E2-E0 (bits 7-5): operator-defined extended field (Table M.12 note)
  //   Variable (PSAC 1XXX):   b7 b6 NI R P3 P2 P1 P0
  //     - b7 b6 (bits 7-6): 00 alpha, 01 numeric, 11 functions
  //     - NI (bit 5): 0 = national, 1 = international
  //   R (bit 4): 0 = original / not implemented, 1 = repeat (Table M.13)
  //   P3-P0 (bits 3-0): per-address call counter (1-15; 0 = not implemented)
  call_counter_ = ctrl & 0x0FU;
  is_repeat_    = (ctrl & 0x10U) != 0;
  if (type == VarType::kNone) {
    tone_ext_ = (static_cast<unsigned>(ctrl) >> 5U) & 0x07U;
  } else {
    is_international_ = (ctrl & 0x20U) != 0;
  }
}

void RadioPaging::appendCharGroup(std::uint16_t block_c, std::uint16_t block_d) {
  // Four 8-bit characters, most significant byte first. Decoding goes through
  // RDSString, so the message uses the RDS basic character set (EN 50067
  // Annex E) exactly like RadioText and PS, rather than a raw ASCII mapping.
  const std::array<std::uint8_t, 4> chars{getUint8(block_c, 8), getUint8(block_c, 0),
                                          getUint8(block_d, 8), getUint8(block_d, 0)};
  for (const std::uint8_t ch : chars) {
    alpha_message_.set(alpha_pos_, ch);
    ++alpha_pos_;
  }
}

void RadioPaging::appendNibbleBlock(std::uint16_t block) {
  // Four 4-bit hex nibbles, most significant first (EN 50067 M.3.5.5/M.3.5.6).
  // Numeric uses 0-9 with hex A = space; functions uses the full 0-F range.
  if (var_type_ == VarType::kFunctions) {
    static constexpr std::string_view kHex{"0123456789ABCDEF"};
    numeric_message_ += kHex[getBits<4>(block, 12)];
    numeric_message_ += kHex[getBits<4>(block, 8)];
    numeric_message_ += kHex[getBits<4>(block, 4)];
    numeric_message_ += kHex[getBits<4>(block, 0)];
  } else {
    appendBcdBlock(numeric_message_, block);
  }
}

void RadioPaging::appendNibbleGroup(std::uint16_t block_c, std::uint16_t block_d) {
  // Eight 4-bit nibbles across blocks C and D (EN 50067 M.3.5.5/M.3.5.6).
  appendNibbleBlock(block_c);
  appendNibbleBlock(block_d);
}

void RadioPaging::finishMessage(ObjectTree& out) {
  if (!assembling_)
    return;

  auto& call        = out["radio_paging"]["call"];
  call["address"]   = address_;
  call["call_flag"] = ab_flag_ ? "B" : "A";

  if (var_type_ == VarType::kAlpha) {
    call["type"] = is_international_ ? "alphanumeric_international" : "alphanumeric";
    // Trim the buffer to the characters actually received so str() does not
    // pad out to the full 80-char allocation.
    alpha_message_.resize(alpha_pos_ > 0 ? alpha_pos_ : 1);
    call["message"] = alpha_message_.str();
    if (is_international_) {
      // Country code + Operator Code from the M.3.5.7 header group.
      call["country_code"]  = static_cast<int>(country_code_);
      call["operator_code"] = static_cast<int>(opc_);
    }
  } else if (var_type_ == VarType::kNumeric) {
    call["type"]    = is_international_ ? "variable_numeric_international" : "variable_numeric";
    call["message"] = numeric_message_;
    if (is_international_) {
      call["country_code"]  = static_cast<int>(country_code_);
      call["operator_code"] = static_cast<int>(opc_);
    }
  } else if (var_type_ == VarType::kFunctions) {
    call["type"]    = is_international_ ? "functions_international" : "functions";
    call["message"] = numeric_message_;
    if (is_international_) {
      call["country_code"]  = static_cast<int>(country_code_);
      call["operator_code"] = static_cast<int>(opc_);
    }
  } else if (basic_num_type_ == BasicNumType::kIntl15) {
    call["type"]         = "numeric_international_15";
    call["message"]      = numeric_message_;
    call["country_code"] = static_cast<int>(country_code_);
  } else if (basic_num_type_ == BasicNumType::kIntlFunc) {
    call["type"]         = "functions_international";
    call["message"]      = numeric_message_;
    call["country_code"] = static_cast<int>(country_code_);
  } else {
    call["type"]    = "numeric";
    call["message"] = numeric_message_;
  }

  if (have_control_) {
    call["is_repeat"] = is_repeat_;
    if (call_counter_ != 0)
      call["call_counter"] = static_cast<int>(call_counter_);
  }

  // Flag messages reassembled with lost groups (filler inserted) or emitted
  // before their end-of-message marker was seen.
  if (incomplete_)
    call["incomplete"] = true;

  reset();
}

// Emit whatever has been assembled so far as an incomplete message, then reset.
// Used when a new call pre-empts an in-progress one (A/B flip or a fresh
// address group) and the user asked for partial output.
void RadioPaging::emitPartial(ObjectTree& out) {
  if (!assembling_)
    return;
  // A message with content but no end marker is by definition incomplete.
  const bool has_content =
      (var_type_ == VarType::kAlpha) ? (alpha_pos_ > 0) : !numeric_message_.empty();
  if (!has_content) {
    reset();
    return;
  }
  incomplete_ = true;
  finishMessage(out);
}

}  // namespace redsea
