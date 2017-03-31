#include "src/tables.h"

#include <cassert>
#include <map>
#include <vector>

#include "src/common.h"

namespace redsea {

// EN 50067:1998, Annex E (pp. 73-76)
std::string RDSCharString(uint8_t code, eCodeTable codetable) {
  std::string result(" ");
  static const std::vector<std::string> codetable_G0({
      " ", "0", "@", "P", "‖", "p", "á", "â", "ª", "º", "Á", "Â", "Ã", "ã",
      "!", "1", "A", "Q", "a", "q", "à", "ä", "α", "¹", "À", "Ä", "Å", "å",
      "\"","2", "B", "R", "b", "r", "é", "ê", "©", "²", "É", "Ê", "Æ", "æ",
      "#", "3", "C", "S", "c", "s", "è", "ë", "‰", "³", "È", "Ë", "Œ", "œ",
      "¤", "4", "D", "T", "d", "t", "í", "î", "Ǧ", "±", "Í", "Î", "ŷ", "ŵ",
      "%", "5", "E", "U", "e", "u", "ì", "ï", "ě", "İ", "Ì", "Ï", "Ý", "ý",
      "&", "6", "F", "V", "f", "v", "ó", "ô", "ň", "ń", "Ó", "Ô", "Õ", "õ",
      "'", "7", "G", "W", "g", "w", "ò", "ö", "ő", "ű", "Ò", "Ö", "Ø", "ø",
      "(", "8", "H", "X", "h", "x", "ú", "û", "π", "µ", "Ú", "Û", "Þ", "þ",
      ")", "9", "I", "Y", "i", "y", "ù", "ü", "€", "¿", "Ù", "Ü", "Ŋ", "ŋ",
      "*", ":", "J", "Z", "j", "z", "Ñ", "ñ", "£", "÷", "Ř", "ř", "Ŕ", "ŕ",
      "+", ";", "K", "[", "k", "{", "Ç", "ç", "$", "°", "Č", "č", "Ć", "ć",
      ",", "<", "L", "\\","l", "|", "Ş", "ş", "←", "¼", "Š", "š", "Ś", "ś",
      "-", "=", "M", "]", "m", "}", "β", "ǧ", "↑", "½", "Ž", "ž", "Ź", "ź",
      ".", ">", "N", "―", "n", "¯", "¡", "ı", "→", "¾", "Ð", "đ", "Ŧ", "ŧ",
      "/", "?", "O", "_", "o", " ", "Ĳ", "ĳ", "↓", "§", "Ŀ", "ŀ", "ð" });

  static const std::vector<std::string> codetable_G1({
      " ", "0", "@", "P", "‖", "p", "á", "â", "ª", "º", "Є", "ý", "Π", "π",
      "!", "1", "A", "Q", "a", "q", "à", "ä", "ľ", "¹", "Я", "Љ", "α", "Ω",
      "\"","2", "B", "R", "b", "r", "é", "ê", "©", "²", "Б", "ď", "β", "ρ",
      "#", "3", "C", "S", "c", "s", "è", "ë", "‰", "³", "Ч", "Ш", "ψ", "σ",
      "¤", "4", "D", "T", "d", "t", "í", "î", "ǎ", "±", "Д", "Ц", "δ", "τ",
      "%", "5", "E", "U", "e", "u", "ì", "ï", "ě", "İ", "Э", "Ю", "ε", "ξ",
      "&", "6", "F", "V", "f", "v", "ó", "ô", "ň", "ń", "Ф", "Щ", "φ", "Θ",
      "'", "7", "G", "W", "g", "w", "ò", "ö", "ő", "ű", "Ѓ", "Њ", "γ", "Γ",
      "(", "8", "H", "X", "h", "x", "ú", "û", "ť", "ţ", "Ъ", "Џ", "η", "Ξ",
      ")", "9", "I", "Y", "i", "y", "ù", "ü", "€", "¿", "И", "Й", "ι", "υ",
      "*", ":", "J", "Z", "j", "z", "Ñ", "ñ", "£", "÷", "Ж", "З", "Σ", "ζ",
      "+", ";", "K", "[", "k", "{", "Ç", "ç", "$", "°", "Ќ", "č", "χ", "ς",
      ",", "<", "L", "\\","l", "|", "Ş", "ş", "←", "¼", "Л", "š", "λ", "Λ",
      "-", "=", "M", "]", "m", "}", "β", "ǧ", "↑", "½", "Ћ", "ž", "μ", "Ψ",
      ".", ">", "N", "―", "n", "¯", "¡", "ı", "→", "¾", "Ђ", "đ", "ν", "Δ",
      "/", "?", "O", "_", "o", " ", "Ĳ", "ĳ", "↓", "§", "Ы", "ć", "ω" });

  static const std::vector<std::string> codetable_G2({
      " ", "0", "@", "P", "‖", "p", "ب", "ظ", "א", "ן", "Є", "ý", "Π", "π",
      "!", "1", "A", "Q", "a", "q", "ت", "ع", "ב", "ס", "Я", "Љ", "α", "Ω",
      "\"","2", "B", "R", "b", "r", "ة", "غ", "ג", "ע", "Б", "ď", "β", "ρ",
      "#", "3", "C", "S", "c", "s", "ث", "ف", "ד", "פ", "Ч", "Ш", "ψ", "σ",
      "¤", "4", "D", "T", "d", "t", "ج", "ق", "ה", "ף", "Д", "Ц", "δ", "τ",
      "%", "5", "E", "U", "e", "u", "ح", "ك", "ו", "צ", "Э", "Ю", "ε", "ξ",
      "&", "6", "F", "V", "f", "v", "خ", "ل", "ז", "ץ", "Ф", "Щ", "φ", "Θ",
      "'", "7", "G", "W", "g", "w", "د", "م", "ח", "ץ", "Ѓ", "Њ", "γ", "Γ",
      "(", "8", "H", "X", "h", "x", "ذ", "ن", "ט", "ר", "Ъ", "Џ", "η", "Ξ",
      ")", "9", "I", "Y", "i", "y", "ر", "ه", "י", "ש", "И", "Й", "ι", "υ",
      "*", ":", "J", "Z", "j", "z", "ز", "و", "כ", "ת", "Ж", "З", "Σ", "ζ",
      "+", ";", "K", "[", "k", "{", "س", "ي", "ך", "°", "Ќ", "č", "χ", "ς",
      ",", "<", "L", "\\","l", "|", "ش", "←", "ל", "¼", "Л", "š", "λ", "Λ",
      "-", "=", "M", "]", "m", "}", "ص", "↑", "מ", "½", "Ћ", "ž", "μ", "Ψ",
      ".", ">", "N", "―", "n", "¯", "ض", "→", "ם", "¾", "Ђ", "đ", "ν", "Δ",
      "/", "?", "O", "_", "o", " ", "ط", "↓", "נ", "§", "Ы", "ć", "ω" });

  int row = code & 0xF;
  int col = code >> 4;
  int idx = row * 14 + (col - 2);
  if (col >= 2 && idx >= 0 && idx < static_cast<int>(codetable_G0.size())) {
    if (codetable == G0)
      result = codetable_G0[idx];
    else if (codetable == G1)
      result = codetable_G1[idx];
    else if (codetable == G2)
      result = codetable_G2[idx];
  }

  return result;
}

std::string PTYNameString(int pty, bool is_rbds) {
  assert(pty >= 0 && pty <= 32);

  // EN 50067:1998, Annex F (pp. 77-78)
  static const std::vector<std::string> pty_names({
    "No PTY",         "News",            "Current affairs",    "Information",
    "Sport",          "Education",       "Drama",              "Culture",
    "Science",        "Varied",          "Pop music",          "Rock music",
    "Easy listening", "Light classical", "Serious classical",  "Other music",
    "Weather",        "Finance",      "Children's programmes", "Social affairs",
    "Religion",       "Phone-in",        "Travel",             "Leisure",
    "Jazz music",     "Country music",   "National music",     "Oldies music",
    "Folk music",     "Documentary",     "Alarm test",         "Alarm" });

  // U.S. RBDS Standard, Annex F (pp. 95-96)
  static const std::vector<std::string> pty_names_rbds({
    "No PTY",           "News",                  "Information",  "Sports",
    "Talk",             "Rock",                  "Classic rock", "Adult hits",
    "Soft rock",        "Top 40",                "Country",      "Oldies",
    "Soft",             "Nostalgia",             "Jazz",         "Classical",
    "Rhythm and blues", "Soft rhythm and blues", "Language",  "Religious music",
    "Religious talk",   "Personality",           "Public",       "College",
    "Spanish talk",     "Spanish music",         "Hip hop",      "",
    "",                 "Weather",              "Emergency test", "Emergency"});

  return is_rbds ? pty_names_rbds[pty] : pty_names[pty];
}

// EN 50067:1998, Annex D, Table D.1 (p. 71)
// RDS Forum R08/008_7, Table D.2 (p. 75)
std::string CountryString(uint16_t pi, uint16_t ecc) {
  static const std::map<uint16_t, std::vector<std::string>> country_codes({
    {0xA0, {"us", "us", "us", "us", "us", "us", "us", "us",
            "us", "us", "us", "--", "us", "us", "--"}},
    {0xA1, {"--", "--", "--", "--", "--", "--", "--", "--",
            "--", "--", "ca", "ca", "ca", "ca", "gl"}},
    {0xA2, {"ai", "ag", "ec", "fk", "bb", "bz", "ky", "cr",
            "cu", "ar", "br", "bm", "an", "gp", "bs"}},
    {0xA3, {"bo", "co", "jm", "mq", "gf", "py", "ni", "--",
            "pa", "dm", "do", "cl", "gd", "tc", "gy"}},
    {0xA4, {"gt", "hn", "aw", "--", "ms", "tt", "pe", "sr",
            "uy", "kn", "lc", "sv", "ht", "ve", "--"}},
    {0xA5, {"--", "--", "--", "--", "--", "--", "--", "--",
            "--", "--", "mx", "vc", "mx", "mx", "mx"}},
    {0xA6, {"--", "--", "--", "--", "--", "--", "--", "--",
            "--", "--", "--", "--", "--", "--", "pm"}},
    {0xD0, {"cm", "cf", "dj", "mg", "ml", "ao", "gq", "ga",
            "gn", "za", "bf", "cg", "tg", "bj", "mw"}},
    {0xD1, {"na", "lr", "gh", "mr", "st", "cv", "sn", "gm",
            "bi", "--", "bw", "km", "tz", "et", "bg"}},
    {0xD2, {"sl", "zw", "mz", "ug", "sz", "ke", "so", "ne",
            "td", "gw", "zr", "ci", "tz", "zm", "--"}},
    {0xD3, {"--", "--", "eh", "--", "rw", "ls", "--", "sc",
            "--", "mu", "--", "sd", "--", "--", "--"}},
    {0xE0, {"de", "dz", "ad", "il", "it", "be", "ru", "ps",
            "al", "at", "hu", "mt", "de", "--", "eg"}},
    {0xE1, {"gr", "cy", "sm", "ch", "jo", "fi", "lu", "bg",
            "dk", "gi", "iq", "gb", "ly", "ro", "fr"}},
    {0xE2, {"ma", "cz", "pl", "va", "sk", "sy", "tn", "--",
            "li", "is", "mc", "lt", "rs", "es", "no"}},
    {0xE3, {"me", "ie", "tr", "mk", "--", "--", "--", "nl",
            "lv", "lb", "az", "hr", "kz", "se", "by"}},
    {0xE4, {"md", "ee", "kg", "--", "--", "ua", "ks", "pt",
            "si", "am", "--", "ge", "--", "--", "ba"}},
    {0xF0, {"au", "au", "au", "au", "au", "au", "au", "au",
            "sa", "af", "mm", "cn", "kp", "bh", "my"}},
    {0xF1, {"ki", "bt", "bd", "pk", "fj", "om", "nr", "ir",
            "nz", "sb", "bn", "lk", "tw", "kr", "hk"}},
    {0xF2, {"kw", "qa", "kh", "ws", "in", "mo", "vn", "ph",
            "jp", "sg", "mv", "id", "ae", "np", "vu"}},
    {0xF3, {"la", "th", "to", "--", "--", "--", "--", "--",
            "pg", "--", "ye", "--", "--", "fm", "mn"}} });

  std::string result("--");

  uint16_t pi_cc = pi >> 12;

  if (country_codes.find(ecc) != country_codes.end() && pi_cc > 0) {
    result = country_codes.at(ecc).at(pi_cc-1);
  }

  return result;
}

// EN 50067:1998, Annex J (p. 84)
std::string LanguageString(uint16_t code) {
  static const std::vector<std::string> languages({
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
    "Assamese",    "Armenian",      "Arabic",     "Amharic" });

  std::string result("");
  if (code < languages.size())
    result = languages[code];

  return result;
}

// RDS Forum R13/041_2 (2013-09-05)
std::string AppNameString(uint16_t aid) {
  static const std::map<uint16_t, std::string> oda_apps({
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
    {0x4AA1, "RASANT"},
    {0x4AB7, "ELECTRABEL-DSM 9"},
    {0x4BA2, "ELECTRABEL-DSM 5"},
    {0x4BD7, "RadioText+ (RT+)"},
    {0x4C59, "CITIBUS 2"},
    {0x4D87, "Radio Commerce System (RCS)"},
    {0x4D95, "ELECTRABEL-DSM 16"},
    {0x4D9A, "ELECTRABEL-DSM 11"},
    {0x5757, "Personal weather station"},
    {0x6552, "Enhanced RadioText (eRT)"},
    {0x7373, "Enhanced early warning system"},
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
    {0xE911, "EAS open protocol"} });

  std::string result("(Unknown)");
  if (oda_apps.find(aid) != oda_apps.end()) {
    result = oda_apps.at(aid);
  }

  return result;
}

// RDS Forum R06/040_1 (2006-07-21)
std::string RTPlusContentTypeString(uint16_t content_type) {
  static const std::vector<std::string> content_type_names({
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
      "get_data" });

  return (content_type < content_type_names.size() ?
      content_type_names[content_type] : "unknown");
}

std::string DICodeString(uint16_t di) {
  static const std::vector<std::string> di_codes({
      "stereo", "artificial_head", "compressed", "dynamic_pty" });

  assert (di < di_codes.size());
  return di_codes[di];
}

eOpenDataApp getODApp(uint16_t aid) {
  if (aid == 0xCD46 || aid == 0xCD47)
    return ODA_TMC;
  if (aid == 0x4BD7)
    return ODA_RTPLUS;
  return ODA_UNKNOWN;
}

}  // namespace redsea
