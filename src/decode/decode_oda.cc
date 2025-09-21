#include "src/decode/decode_oda.hh"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "src/group.hh"
#include "src/io/tree.hh"
#include "src/simplemap.hh"
#include "src/tables.hh"
#include "src/text/radiotext.hh"
#include "src/text/stringutil.hh"
#include "src/tmc/tmc.hh"
#include "src/util.hh"

namespace redsea {

// Group 3A: Application identification for Open Data
void decodeType3A(const Group& group, ObjectTree& tree,
                  SimpleMap<GroupType, std::uint16_t>& oda_app_for_group, RadioText& radiotext,
                  RadioText& ert, tmc::TMCService& tmc) {
  if (!(group.has(BLOCK3) && group.has(BLOCK4)))
    return;

  if (group.getType().version != GroupType::Version::A)
    return;

  const GroupType oda_group_type(getBits(group.get(BLOCK2), 0, 5));
  const std::uint16_t oda_message{group.get(BLOCK3)};
  const std::uint16_t oda_app_id{group.get(BLOCK4)};

  oda_app_for_group.insert(oda_group_type, oda_app_id);

  tree["open_data_app"]["oda_group"] = oda_group_type.str();
  tree["open_data_app"]["app_name"]  = getAppNameString(oda_app_id);

  switch (oda_app_id) {
    // DAB cross-referencing
    case 0x0093:
      // Message bits are not used
      break;

    // RT+
    case 0x4BD7:
      radiotext.plus.exists       = true;
      radiotext.plus.cb           = getBool(oda_message, 12);
      radiotext.plus.scb          = getBits(oda_message, 8, 4);
      radiotext.plus.template_num = getUint8(oda_message, 0);
      break;

    // RT+ for Enhanced RadioText
    case 0x4BD8:
      ert.plus.exists       = true;
      ert.plus.cb           = getBool(oda_message, 12);
      ert.plus.scb          = getBits(oda_message, 8, 4);
      ert.plus.template_num = getUint8(oda_message, 0);
      break;

    // Enhanced RadioText (eRT)
    case 0x6552:
      ert.text.setEncoding(getBool(oda_message, 0) ? RDSString::Encoding::UTF8
                                                   : RDSString::Encoding::UCS2);
      ert.text.setDirection(getBool(oda_message, 1) ? RDSString::Direction::RTL
                                                    : RDSString::Direction::LTR);
      ert.uses_chartable_e3 = getBits(oda_message, 2, 4) == 0;
      break;

    // RDS-TMC
    case 0xCD46:
    case 0xCD47: tmc.receiveSystemGroup(oda_message, tree); break;

    default:
      tree["debug"].push_back("TODO: Unimplemented ODA app " + getHexString(oda_app_id, 4));
      tree["open_data_app"]["message"] = oda_message;
      break;
  }
}

/* Open Data Application */
void decodeODAGroup(const Group& group, ObjectTree& tree,
                    const SimpleMap<GroupType, std::uint16_t>& oda_app_for_group,
                    RadioText& radiotext, RadioText& ert, tmc::TMCService& tmc) {
  if (!oda_app_for_group.contains(group.getType())) {
    tree["unknown_oda"]["raw_data"] =
        getHexString(group.get(BLOCK2) & 0b11111U, 2) + " " +
        (group.has(BLOCK3) ? getHexString(group.get(BLOCK3), 4) : "----") + " " +
        (group.has(BLOCK4) ? getHexString(group.get(BLOCK4), 4) : "----");

    return;
  }

  const std::uint16_t oda_app_id = oda_app_for_group.at(group.getType());

  switch (oda_app_id) {
    // DAB cross-referencing
    case 0x0093: decodeDAB(group, tree); break;

    // RT+
    case 0x4BD7: decodeRadioTextPlus(group, radiotext, tree["radiotext_plus"]); break;

    // RT+ for Enhanced RadioText
    case 0x4BD8: decodeRadioTextPlus(group, ert, tree["ert_plus"]); break;

    // Enhanced RadioText (eRT)
    case 0x6552: decodeEnhancedRT(group, tree, ert); break;

    // RDS-TMC
    case 0xCD46:
    case 0xCD47:
      if (group.has(BLOCK2) && group.has(BLOCK3) && group.has(BLOCK4))
        tmc.receiveUserGroup(getBits(group.get(BLOCK2), 0, 5), group.get(BLOCK3), group.get(BLOCK4),
                             tree);
      break;

    default:
      tree["unknown_oda"]["app_id"]   = getHexString(oda_app_id, 4);
      tree["unknown_oda"]["app_name"] = getAppNameString(oda_app_id);
      tree["unknown_oda"]["raw_data"] =
          getHexString(group.get(BLOCK2) & 0b11111U, 2) + " " +
          (group.has(BLOCK3) ? getHexString(group.get(BLOCK3), 4) : "----") + " " +
          (group.has(BLOCK4) ? getHexString(group.get(BLOCK4), 4) : "----");
  }
}

// RadioText Plus (content-type tagging for RadioText)
void decodeRadioTextPlus(const Group& group, RadioText& rt, ObjectTree& tree_el) {
  const bool item_toggle  = getBool(group.get(BLOCK2), 4);
  const bool item_running = getBool(group.get(BLOCK2), 3);

  if (item_toggle != rt.plus.toggle || item_running != rt.plus.item_running) {
    rt.text.clear();
    rt.plus.toggle       = item_toggle;
    rt.plus.item_running = item_running;
  }

  tree_el["item_running"] = item_running;
  tree_el["item_toggle"]  = item_toggle ? 1 : 0;

  const std::size_t num_tags = group.has(BLOCK3) ? (group.has(BLOCK4) ? 2 : 1) : 0;
  std::vector<RadioText::Plus::Tag> tags(num_tags);

  if (num_tags > 0) {
    tags[0].content_type = getBits(group.get(BLOCK2), group.get(BLOCK3), 13, 6);
    tags[0].start        = getBits(group.get(BLOCK3), 7, 6);
    tags[0].length       = getBits(group.get(BLOCK3), 1, 6) + 1;

    if (num_tags == 2) {
      tags[1].content_type = getBits(group.get(BLOCK3), group.get(BLOCK4), 11, 6);
      tags[1].start        = getBits(group.get(BLOCK4), 5, 6);
      tags[1].length       = getBits(group.get(BLOCK4), 0, 5) + 1;
    }
  }

  for (const auto& tag : tags) {
    const std::string text = rt.text.getLastCompleteString(tag.start, tag.length);

    if (text.length() > 0 && tag.content_type != 0) {
      ObjectTree tag_tree;
      tag_tree["content-type"] = getRTPlusContentTypeString(tag.content_type);
      tag_tree["data"]         = rtrim(text);
      tree_el["tags"].push_back(tag_tree);
    }
  }
}

// RDS2 Enhanced RadioText (eRT)
void decodeEnhancedRT(const Group& group, ObjectTree& tree, RadioText& ert) {
  const std::size_t position = getBits(group.get(BLOCK2), 0, 5) * 4U;

  ert.update(position, getUint8(group.get(BLOCK3), 8), getUint8(group.get(BLOCK3), 0));

  if (group.has(BLOCK4)) {
    ert.update(position + 2U, getUint8(group.get(BLOCK4), 8), getUint8(group.get(BLOCK4), 0));
  }

  if (ert.text.isComplete()) {
    tree["enhanced_radiotext"] = rtrim(ert.text.getLastCompleteString());
  }
}

// ETSI EN 301 700 V1.1.1 (2000-03)
void decodeDAB(const Group& group, ObjectTree& tree) {
  const bool es_flag = getBool(group.get(BLOCK2), 4);

  if (es_flag) {
    // Service table
    tree["debug"].push_back("TODO: DAB service table");

  } else {
    // Ensemble table

    const auto mode = getBits(group.get(BLOCK2), 2, 2);
    const std::array<std::string, 4> modes{"unspecified", "I", "II or III", "IV"};
    tree["dab"]["mode"] = modes[mode];

    const std::uint32_t freq = 16 * getBits(group.get(BLOCK2), group.get(BLOCK3), 0, 18);

    tree["dab"]["kilohertz"] = freq;

    const SimpleMap<std::uint32_t, std::string_view> dab_channels({
        // clang-format off
        { 174'928,  "5A"}, { 176'640,  "5B"}, { 178'352,  "5C"}, { 180'064,  "5D"},
        { 181'936,  "6A"}, { 183'648,  "6B"}, { 185'360,  "6C"}, { 187'072,  "6D"},
        { 188'928,  "7A"}, { 190'640,  "7B"}, { 192'352,  "7C"}, { 194'064,  "7D"},
        { 195'936,  "8A"}, { 197'648,  "8B"}, { 199'360,  "8C"}, { 201'072,  "8D"},
        { 202'928,  "9A"}, { 204'640,  "9B"}, { 206'352,  "9C"}, { 208'064,  "9D"},
        { 209'936, "10A"}, { 211'648, "10B"}, { 213'360, "10C"}, { 215'072, "10D"},
        { 216'928, "11A"}, { 218'640, "11B"}, { 220'352, "11C"}, { 222'064, "11D"},
        { 223'936, "12A"}, { 225'648, "12B"}, { 227'360, "12C"}, { 229'072, "12D"},
        { 230'784, "13A"}, { 232'496, "13B"}, { 234'208, "13C"}, { 235'776, "13D"},
        { 237'488, "13E"}, { 239'200, "13F"}, {1452'960,  "LA"}, {1454'672,  "LB"},
        {1456'384,  "LC"}, {1458'096,  "LD"}, {1459'808,  "LE"}, {1461'520,  "LF"},
        {1463'232,  "LG"}, {1464'944,  "LH"}, {1466'656,  "LI"}, {1468'368,  "LJ"},
        {1470'080,  "LK"}, {1471'792,  "LL"}, {1473'504,  "LM"}, {1475'216,  "LN"},
        {1476'928,  "LO"}, {1478'640,  "LP"}, {1480'352,  "LQ"}, {1482'064,  "LR"},
        {1483'776,  "LS"}, {1485'488,  "LT"}, {1487'200,  "LU"}, {1488'912,  "LV"},
        {1490'624,  "LW"}  // clang-format on
    });

    if (dab_channels.contains(freq)) {
      tree["dab"]["channel"] = dab_channels.at(freq);
    }

    tree["dab"]["ensemble_id"] = getPrefixedHexString(group.get(BLOCK4), 4);
  }
}

}  // namespace redsea
