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
#include "src/tables.hh"

#include <array>
#include <cstdint>
#include <string>
#include <string_view>

#include "src/simplemap.hh"

namespace redsea {

// Program Type names (RDS)
// EN 50067:1998, Annex F (pp. 77-78)
std::string_view getPTYNameString(std::uint16_t pty) {
  // clang-format off
  constexpr std::array<std::string_view, 64> pty_names{
    "No PTY",         "News",            "Current affairs",       "Information",
    "Sport",          "Education",       "Drama",                 "Culture",
    "Science",        "Varied",          "Pop music",             "Rock music",
    "Easy listening", "Light classical", "Serious classical",     "Other music",
    "Weather",        "Finance",         "Children's programmes", "Social affairs",
    "Religion",       "Phone-in",        "Travel",                "Leisure",
    "Jazz music",     "Country music",   "National music",        "Oldies music",
    "Folk music",     "Documentary",     "Alarm test",            "Alarm" };
  // clang-format on

  return pty < pty_names.size() ? pty_names[pty] : "Unknown";
}

// Program Type names (U.S. / RBDS)
// U.S. RBDS Standard 1998, Annex F (pp. 95-96)
std::string_view getPTYNameStringRBDS(std::uint16_t pty) {
  // clang-format off
  constexpr std::array<std::string_view, 64> pty_names_rbds{
    "No PTY",           "News",                  "Information",    "Sports",
    "Talk",             "Rock",                  "Classic rock",   "Adult hits",
    "Soft rock",        "Top 40",                "Country",        "Oldies",
    "Soft",             "Nostalgia",             "Jazz",           "Classical",
    "Rhythm and blues", "Soft rhythm and blues", "Language",       "Religious music",
    "Religious talk",   "Personality",           "Public",         "College",
    "Spanish talk",     "Spanish music",         "Hip hop",        "",
    "",                 "Weather",               "Emergency test", "Emergency" };
  // clang-format on

  return pty < pty_names_rbds.size() ? pty_names_rbds[pty] : "Unknown";
}

// 2-letter country codes
// EN 50067:1998, Annex D, Table D.1 (p. 71)
// RDS Forum R08/008_7, Table D.2 (p. 75)
std::string_view getCountryString(std::uint16_t cc, std::uint16_t ecc) {
  const SimpleMap<std::uint16_t, std::array<std::string_view, 15>> country_codes{
      {0xA0,
       {"us", "us", "us", "us", "us", "us", "us", "us", "us", "us", "us", "--", "us", "us", "--"}},
      {0xA1,
       {"--", "--", "--", "--", "--", "--", "--", "--", "--", "--", "ca", "ca", "ca", "ca", "gl"}},
      {0xA2,
       {"ai", "ag", "ec", "fk", "bb", "bz", "ky", "cr", "cu", "ar", "br", "bm", "an", "gp", "bs"}},
      {0xA3,
       {"bo", "co", "jm", "mq", "gf", "py", "ni", "--", "pa", "dm", "do", "cl", "gd", "tc", "gy"}},
      {0xA4,
       {"gt", "hn", "aw", "--", "ms", "tt", "pe", "sr", "uy", "kn", "lc", "sv", "ht", "ve", "--"}},
      {0xA5,
       {"--", "--", "--", "--", "--", "--", "--", "--", "--", "--", "mx", "vc", "mx", "mx", "mx"}},
      {0xA6,
       {"--", "--", "--", "--", "--", "--", "--", "--", "--", "--", "--", "--", "--", "--", "pm"}},
      {0xD0,
       {"cm", "cf", "dj", "mg", "ml", "ao", "gq", "ga", "gn", "za", "bf", "cg", "tg", "bj", "mw"}},
      {0xD1,
       {"na", "lr", "gh", "mr", "st", "cv", "sn", "gm", "bi", "--", "bw", "km", "tz", "et", "bg"}},
      {0xD2,
       {"sl", "zw", "mz", "ug", "sz", "ke", "so", "ne", "td", "gw", "zr", "ci", "tz", "zm", "--"}},
      {0xD3,
       {"--", "--", "eh", "--", "rw", "ls", "--", "sc", "--", "mu", "--", "sd", "--", "--", "--"}},
      {0xE0,
       {"de", "dz", "ad", "il", "it", "be", "ru", "ps", "al", "at", "hu", "mt", "de", "--", "eg"}},
      {0xE1,
       {"gr", "cy", "sm", "ch", "jo", "fi", "lu", "bg", "dk", "gi", "iq", "gb", "ly", "ro", "fr"}},
      {0xE2,
       {"ma", "cz", "pl", "va", "sk", "sy", "tn", "--", "li", "is", "mc", "lt", "rs", "es", "no"}},
      {0xE3,
       {"me", "ie", "tr", "mk", "--", "--", "--", "nl", "lv", "lb", "az", "hr", "kz", "se", "by"}},
      {0xE4,
       {"md", "ee", "kg", "--", "--", "ua", "ks", "pt", "si", "am", "--", "ge", "--", "--", "ba"}},
      {0xF0,
       {"au", "au", "au", "au", "au", "au", "au", "au", "sa", "af", "mm", "cn", "kp", "bh", "my"}},
      {0xF1,
       {"ki", "bt", "bd", "pk", "fj", "om", "nr", "ir", "nz", "sb", "bn", "lk", "tw", "kr", "hk"}},
      {0xF2,
       {"kw", "qa", "kh", "ws", "in", "mo", "vn", "ph", "jp", "sg", "mv", "id", "ae", "np", "vu"}},
      {0xF3,
       {"la", "th", "to", "--", "--", "--", "--", "--", "pg", "--", "ye", "--", "--", "fm", "mn"}}
  };

  return (cc > 0 && country_codes.contains(ecc)) ? country_codes.at(ecc).at(cc - 1) : "--";
}

// Program languages
// EN 50067:1998, Annex J (p. 84)
std::string_view getLanguageString(std::uint16_t code) {
  // clang-format off
  constexpr std::array<std::string_view, 128> languages{
    "Unknown",     "Albanian",      "Breton",     "Catalan",
    "Croatian",    "Welsh",         "Czech",      "Danish",
    "German",      "English",       "Spanish",    "Esperanto",
    "Estonian",    "Basque",        "Faroese",    "French",
    "Frisian",     "Irish",         "Gaelic",     "Galician",
    "Icelandic",   "Italian",       "Lappish",    "Latin",
    "Latvian",     "Luxembourgian", "Lithuanian", "Hungarian",
    "Maltese",     "Dutch",         "Norwegian",  "Occitan",
    "Polish",      "Portuguese",    "Romanian",   "Romansh",
    "Serbian",     "Slovak",        "Slovene",    "Finnish",
    "Swedish",     "Turkish",       "Flemish",    "Walloon",
    "",            "",              "",           "",
    "",            "",              "",           "",
    "",            "",              "",           "",
    "",            "",              "",           "",
    "",            "",              "",           "",
    "Background",  "",              "",           "",
    "",            "Zulu",          "Vietnamese", "Uzbek",
    "Urdu",        "Ukrainian",     "Thai",       "Telugu",
    "Tatar",       "Tamil",         "Tadzhik",    "Swahili",
    "SrananTongo", "Somali",        "Sinhalese",  "Shona",
    "Serbo-Croat", "Ruthenian",     "Russian",    "Quechua",
    "Pushtu",      "Punjabi",       "Persian",    "Papamiento",
    "Oriya",       "Nepali",        "Ndebele",    "Marathi",
    "Moldovian",   "Malaysian",     "Malagasay",  "Macedonian",
    "Laotian",     "Korean",        "Khmer",      "Kazakh",
    "Kannada",     "Japanese",      "Indonesian", "Hindi",
    "Hebrew",      "Hausa",         "Gurani",     "Gujurati",
    "Greek",       "Georgian",      "Fulani",     "Dari",
    "Churash",     "Chinese",       "Burmese",    "Bulgarian",
    "Bengali",     "Belorussian",   "Bambora",    "Azerbaijan",
    "Assamese",    "Armenian",      "Arabic",     "Amharic" };
  // clang-format on

  return code < languages.size() ? languages[code] : "";
}

// Open Data Application names
// RDS Forum R13/041_2 (2013-09-05) and later
// RDS Forum R17/032_1 (2017-07-20)
// DHL 7/14/2020
std::string_view getAppNameString(std::uint16_t aid) {
  // clang-format off
  const SimpleMap<std::uint16_t, std::string_view> oda_apps{
    {0x0000, "None"},
    {0x0093, "Cross referencing DAB within RDS"},
    {0x0BCB, "Leisure & Practical Info for Drivers"},
    {0x0C24, "ELECTRABEL-DSM 7"},
    {0x0CC1, "Wireless Playground broadcast control signal"},
    {0x0D45, "RDS-TMC: ALERT-C / EN ISO 14819-1"},
    {0x0D8B, "ELECTRABEL-DSM 18"},
    {0x0E2C, "ELECTRABEL-DSM 3"},
    {0x0E31, "ELECTRABEL-DSM 13"},
    {0x0F87, "ELECTRABEL-DSM 2"},
    {0x125F, "I-FM-RDS for fixed and mobile devices"},
    {0x1BDA, "ELECTRABEL-DSM 1"},
    {0x1C5E, "ELECTRABEL-DSM 20"},
    {0x1C68, "ITIS In-vehicle data base"},
    {0x1CB1, "ELECTRABEL-DSM 10"},
    {0x1D47, "ELECTRABEL-DSM 4"},
    {0x1DC2, "CITIBUS 4"},
    {0x1DC5, "Encrypted TTI using ALERT-Plus"},
    {0x1E8F, "ELECTRABEL-DSM 17"},
    {0x4400, "RDS-Light"},
    {0x4AA1, "RASANT"},
    {0x4AB7, "ELECTRABEL-DSM 9"},
    {0x4BA2, "ELECTRABEL-DSM 5"},
    {0x4BD7, "RadioText+ (RT+)"},
    {0x4BD8, "RadioText Plus / RT+ for eRT"},
    {0x4C59, "CITIBUS 2"},
    {0x4D87, "Radio Commerce System (RCS)"},
    {0x4D95, "ELECTRABEL-DSM 16"},
    {0x4D9A, "ELECTRABEL-DSM 11"},
    {0x50DD, "To warn people in case of disasters or emergency"},
    {0x5757, "Personal weather station"},
    {0x6363, "Hybradio RDS-Net(for testing use, only)"},
    {0x6365, "RDS2 – 9 bit AF lists ODA"},
    {0x6552, "Enhanced RadioText (eRT)"},
    {0x6A7A, "Warning receiver"},
    {0x7373, "Enhanced early warning system"},
    {0xA112, "NL _ Alert system"},
    {0xA911, "Data FM Selective Multipoint Messaging"},
    {0xABCF, "RF Power Monitoring"},
    {0xC350, "NRSC Song Title and Artist"},
    {0xC3A1, "Personal Radio Service"},
    {0xC3B0, "iTunes Tagging"},
    {0xC3C3, "NAVTEQ Traffic Plus"},
    {0xC4D4, "eEAS"},
    {0xC549, "Smart Grid Broadcast Channel"},
    {0xC563, "ID Logic"},
    {0xC6A7, "Veil Enabled Interactive Device"},
    {0xC737, "Utility Message Channel (UMC)"},
    {0xCB73, "CITIBUS 1"},
    {0xCB97, "ELECTRABEL-DSM 14"},
    {0xCC21, "CITIBUS 3"},
    {0xCD46, "RDS-TMC: ALERT-C"},
    {0xCD47, "RDS-TMC: ALERT-C"},
    {0xCD9E, "ELECTRABEL-DSM 8"},
    {0xCE6B, "Encrypted TTI using ALERT-Plus"},
    {0xE123, "APS Gateway"},
    {0xE1C1, "Action code"},
    {0xE319, "ELECTRABEL-DSM 12"},
    {0xE411, "Beacon downlink"},
    {0xE440, "ELECTRABEL-DSM 15"},
    {0xE4A6, "ELECTRABEL-DSM 19"},
    {0xE5D7, "ELECTRABEL-DSM 6"},
    {0xE911, "EAS open protocol"},
    {0xFF7F, "RFT: Station logo"},
    {0xFF80, "RFT+ (work title)"} };
  // clang-format on

  return oda_apps.contains(aid) ? oda_apps.at(aid) : "(Unknown)";
}

// RadioText+ content types
// RDS Forum R06/040_1 (2006-07-21)
std::string_view getRTPlusContentTypeString(std::uint32_t content_type) {
  // clang-format off
  constexpr std::array<std::string_view, 66> content_type_names{
      "dummy_class",          "item.title",         "item.album",
      "item.tracknumber",     "item.artist",        "item.composition",
      "item.movement",        "item.conductor",     "item.composer",
      "item.band",            "item.comment",       "item.genre",
      "info.news",            "info.news.local",    "info.stockmarket",
      "info.sport",           "info.lottery",       "info.horoscope",
      "info.daily_diversion", "info.health",        "info.event",
      "info.scene",           "info.cinema",        "info.tv",
      "info.date_time",       "info.weather",       "info.traffic",
      "info.alarm",           "info.advertisement", "info.url",
      "info.other",           "stationname.short",  "stationname.long",
      "programme.now",        "programme.next",     "programme.part",
      "programme.host",     "programme.editorial_staff", "programme.frequency",
      "programme.homepage", "programme.subchannel", "phone.hotline",
      "phone.studio",         "phone.other",        "sms.studio",
      "sms.other",            "email.hotline",      "email.studio",
      "email.other",          "mms.other",          "chat",
      "chat.centre",          "vote.question",      "vote.centre",
      "unknown",              "unknown",            "unknown",
      "unknown",              "unknown",            "place",
      "appointment",          "identifier",         "purchase",
      "get_data" };
  // clang-format on

  return content_type < content_type_names.size() ? content_type_names[content_type] : "unknown";
}

// Decoder Identification (DI) and Dynamic PTY Indicator (PTYI) codes
// EN 50067:1998, 3.2.1.5 (p. 41)
std::string_view getDICodeString(std::uint16_t di) {
  constexpr std::array<std::string_view, 4> di_codes{"dynamic_pty", "compressed", "artificial_head",
                                                     "stereo"};

  return di < di_codes.size() ? di_codes[di] : "unknown";
}

// Back-calculate callsign for a North American (RBDS) station
// NRSC-4-B (2011), page 18, D.7
std::string getCallsignFromPI(std::uint16_t pi) {
  // clang-format off
  const SimpleMap<std::uint16_t, std::string_view> three_letter_codes{
      {0x99A5, "KBW"}, {0x9992, "KOY"}, {0x9978, "WHO"}, {0x99A6, "KCY"},
      {0x9993, "KPQ"}, {0x999C, "WHP"}, {0x9990, "KDB"}, {0x9964, "KQV"},
      {0x999D, "WIL"}, {0x99A7, "KDF"}, {0x9994, "KSD"}, {0x997A, "WIP"},
      {0x9950, "KEX"}, {0x9965, "KSL"}, {0x99B3, "WIS"}, {0x9951, "KFH"},
      {0x9966, "KUJ"}, {0x997B, "WJR"}, {0x9952, "KFI"}, {0x9995, "KUT"},
      {0x99B4, "WJW"}, {0x9953, "KGA"}, {0x9967, "KVI"}, {0x99B5, "WJZ"},
      {0x9991, "KGB"}, {0x9968, "KWG"}, {0x997C, "WKY"}, {0x9954, "KGO"},
      {0x9996, "KXL"}, {0x997D, "WLS"}, {0x9955, "KGU"}, {0x9997, "KXO"},
      {0x997E, "WLW"}, {0x9956, "KGW"}, {0x996B, "KYW"}, {0x999E, "WMC"},
      {0x9957, "KGY"}, {0x9999, "WBT"}, {0x999F, "WMT"}, {0x99AA, "KHQ"},
      {0x996D, "WBZ"}, {0x9981, "WOC"}, {0x9958, "KID"}, {0x996E, "WDZ"},
      {0x99A0, "WOI"}, {0x9959, "KIT"}, {0x996F, "WEW"}, {0x9983, "WOL"},
      {0x995A, "KJR"}, {0x999A, "WGH"}, {0x9984, "WOR"}, {0x995B, "KLO"},
      {0x9971, "WGL"}, {0x99A1, "WOW"}, {0x995C, "KLZ"}, {0x9972, "WGN"},
      {0x99B9, "WRC"}, {0x995D, "KMA"}, {0x9973, "WGR"}, {0x99A2, "WRR"},
      {0x995E, "KMJ"}, {0x999B, "WGY"}, {0x99A3, "WSB"}, {0x995F, "KNX"},
      {0x9975, "WHA"}, {0x99A4, "WSM"}, {0x9960, "KOA"}, {0x9976, "WHB"},
      {0x9988, "WWJ"}, {0x99AB, "KOB"}, {0x9977, "WHK"}, {0x9989, "WWL"} };

  // TODO These should probably not be called "callsigns"
  const SimpleMap<std::uint16_t, std::string_view> linked_station_codes{
      {0xB001, "NPR-1"},
      {0xB002, "CBC English - Radio One"}, {0xB003, "CBC English - Radio Two"},
      {0xB004, "CBC French => Radio-Canada - Première Chaîne"},
      {0xB005, "CBC French => Radio-Canada - Espace Musique"},
      {0xB006, "CBC"},   {0xB007, "CBC"},   {0xB008, "CBC"},   {0xB009, "CBC"},
      {0xB00A, "NPR-2"}, {0xB00B, "NPR-3"}, {0xB00C, "NPR-4"},
      {0xB00D, "NPR-5"}, {0xB00E, "NPR-6"} };
  // clang-format on

  // Exceptions for zero nybbles

  if ((pi & 0xFFF0U) == 0xAFA0 && (pi & 0x000FU) < 0x000A) {
    // P1 0 0 0 --> A F A P1
    pi <<= 12U;

  } else if ((pi & 0xFF00U) == 0xAF00) {
    // P1 P2 0 0 --> A F P1 P2
    pi <<= 8U;

  } else if ((pi & 0xF000U) == 0xA000) {
    // P1 0 P3 P4 --> A P1 P3 P4
    pi = static_cast<std::uint16_t>(static_cast<std::uint16_t>(pi & 0x0F00U) << 4U) |
         static_cast<std::uint16_t>(pi & 0x00FFU);
  }

  std::string callsign;

  if (pi >= 0x9950 && pi <= 0x9EFF) {
    // Three-letter only
    if (three_letter_codes.contains(pi))
      callsign = three_letter_codes.at(pi);

  } else if (pi >> 12U == 0xB || pi >> 12U == 0xD || pi >> 12U == 0xE) {
    // Nationally-linked stations
    pi &= 0xF0FFU;

    if (linked_station_codes.contains(pi))
      callsign = linked_station_codes.at(pi);

  } else if (pi >= 0x1000 && pi <= 0x994F) {
    // Decode four-letter call sign
    callsign    = "____";
    callsign[0] = (pi <= 0x54A7 ? 'K' : 'W');
    pi -= (pi <= 0x54A7 ? 0x1000 : 0x54A8);

    constexpr std::uint32_t kNumLetters{26};

    callsign[1] = static_cast<char>('A' + (pi / (kNumLetters * kNumLetters)) % kNumLetters);
    callsign[2] = static_cast<char>('A' + (pi / kNumLetters) % kNumLetters);
    callsign[3] = static_cast<char>('A' + (pi % kNumLetters));
  }

  return callsign;
}

}  // namespace redsea
