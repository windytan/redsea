#include "data.h"

#include <cassert>
#include <map>
#include <vector>

namespace redsea {

std::string getLCDchar(int code) {
  assert (code >= 32);
  const std::vector<std::string> char_map ({
      " ","!","\\\"","#","¤","%","&","'","(",")","*","+",",","-",".","/",
      "0","1","2","3","4","5","6","7","8","9",":",";","<","=",">","?",
      "@","A","B","C","D","E","F","G","H","I","J","K","L","M","N","O",
      "P","Q","R","S","T","U","V","W","X","Y","Z","[","\\","]","―","_",
      "‖","a","b","c","d","e","f","g","h","i","j","k","l","m","n","o",
      "p","q","r","s","t","u","v","w","x","y","z","{","|","}","¯"," ",
      "á","à","é","è","í","ì","ó","ò","ú","ù","Ñ","Ç","Ş","β","¡","Ĳ",
      "â","ä","ê","ë","î","ï","ô","ö","û","ü","ñ","ç","ş","ǧ","ı","ĳ",
      "ª","α","©","‰","Ǧ","ě","ň","ő","π","€","£","$","←","↑","→","↓",
      "º","¹","²","³","±","İ","ń","ű","µ","¿","÷","°","¼","½","¾","§",
      "Á","À","É","È","Í","Ì","Ó","Ò","Ú","Ù","Ř","Č","Š","Ž","Ð","Ŀ",
      "Â","Ä","Ê","Ë","Î","Ï","Ô","Ö","Û","Ü","ř","č","š","ž","đ","ŀ",
      "Ã","Å","Æ","Œ","ŷ","Ý","Õ","Ø","Þ","Ŋ","Ŕ","Ć","Ś","Ź","Ŧ","ð",
      "ã","å","æ","œ","ŵ","ý","õ","ø","þ","ŋ","ŕ","ć","ś","ź","ŧ"," "});
  return char_map[code - 32];
}

std::string getPTYname(int pty) {
  assert (pty >= 0 && pty <= 32);
  const std::vector<std::string> pty_names ({
   "No PTY", "News", "Current Affairs", "Information",
   "Sport", "Education", "Drama", "Cultures",
   "Science", "Varied Speech","Pop Music", "Rock Music",
   "Easy Listening","Light Classics M","Serious Classics","Other Music",
   "Weather & Metr", "Finance", "Children's Progs", "Social Affairs",
   "Religion", "Phone In", "Travel & Touring", "Leisure & Hobby",
   "Jazz Music", "Country Music", "National Music", "Oldies Music",
   "Folk Music", "Documentary", "Alarm Test", "Alarm - Alarm !"
  });
  return pty_names[pty];

}

std::string getCountryString(uint16_t pi, uint16_t ecc) {
  const std::map<uint16_t,std::vector<std::string>> country_codes ({
    {0xA0,{"us","us","us","us","us","us","us","us","us","us","us","--","us","us","--"}},
    {0xA1,{"--","--","--","--","--","--","--","--","--","--","ca","ca","ca","ca","gl"}},
    {0xA2,{"ai","ag","ec","fk","bb","bz","ky","cr","cu","ar","br","bm","an","gp","bs"}},
    {0xA3,{"bo","co","jm","mq","gf","py","ni","--","pa","dm","do","cl","gd","tc","gy"}},
    {0xA4,{"gt","hn","aw","--","ms","tt","pe","sr","uy","kn","lc","sv","ht","ve","--"}},
    {0xA5,{"--","--","--","--","--","--","--","--","--","--","mx","vc","mx","mx","mx"}},
    {0xA6,{"--","--","--","--","--","--","--","--","--","--","--","--","--","--","pm"}},
    {0xD0,{"cm","cf","dj","mg","ml","ao","gq","ga","gn","za","bf","cg","tg","bj","mw"}},
    {0xD1,{"na","lr","gh","mr","st","cv","sn","gm","bi","--","bw","km","tz","et","bg"}},
    {0xD2,{"sl","zw","mz","ug","sz","ke","so","ne","td","gw","zr","ci","tz","zm","--"}},
    {0xD3,{"--","--","eh","--","rw","ls","--","sc","--","mu","--","sd","--","--","--"}},
    {0xE0,{"de","dz","ad","il","it","be","ru","ps","al","at","hu","mt","de","--","eg"}},
    {0xE1,{"gr","cy","sm","ch","jo","fi","lu","bg","dk","gi","iq","gb","ly","ro","fr"}},
    {0xE2,{"ma","cz","pl","va","sk","sy","tn","--","li","is","mc","lt","yu","es","no"}},
    {0xE3,{"ie","ie","tr","mk","tj","--","--","nl","lv","lb","az","hr","kz","se","by"}},
    {0xE4,{"md","ee","kg","--","--","ua","--","pt","si","am","uz","ge","--","tm","ba"}},
    {0xF0,{"au","au","au","au","au","au","au","au","sa","af","mm","cn","kp","bh","my"}},
    {0xF1,{"ki","bt","bd","pk","fj","om","nr","ir","nz","sb","bn","lk","tw","kr","hk"}},
    {0xF2,{"kw","qa","kh","ws","in","mo","vn","ph","jp","sg","mv","id","ae","np","vu"}},
    {0xF3,{"la","th","to","--","--","--","--","--","pg","--","ye","--","--","fm","mn"}}
  });

  std::string result("--");

  uint16_t pi_cc = pi >> 12;

  if (country_codes.find(ecc) != country_codes.end() && pi_cc > 0) {
    result = country_codes.at(ecc).at(pi_cc-1);
  }

  return result;

}

std::string getLanguageString(uint16_t code) {
  std::vector<std::string> languages ({
    "Unknown","Albanian","Breton","Catalan",
    "Croatian","Welsh","Czech","Danish",
    "German","English","Spanish","Esperanto",
    "Estonian","Basque","Faroese","French",
    "Frisian","Irish","Gaelic","Galician",
    "Icelandic","Italian","Lappish","Latin",
    "Latvian","Luxembourgian","Lithuanian","Hungarian",
    "Maltese","Dutch","Norwegian","Occitan",
    "Polish","Portuguese","Romanian","Romansh",
    "Serbian","Slovak","Slovene","Finnish",
    "Swedish","Turkish","Flemish","Walloon",
    "","","","",
    "","","","",
    "","","","",
    "","","","",
    "","","","",
    "Background","","","",
    "","Zulu","Vietnamese","Uzbek",
    "Urdu","Ukrainian","Thai","Telugu",
    "Tatar","Tamil","Tadzhik","Swahili",
    "SrananTongo","Somali","Sinhalese","Shona",
    "Serbo-Croat","Ruthenian","Russian","Quechua",
    "Pushtu","Punjabi","Persian","Papamiento",
    "Oriya","Nepali","Ndebele","Marathi",
    "Moldovian","Malaysian","Malagasay","Macedonian",
    "Laotian","Korean","Khmer","Kazakh",
    "Kannada","Japanese","Indonesian","Hindi",
    "Hebrew","Hausa","Gurani","Gujurati",
    "Greek","Georgian","Fulani","Dari",
    "Churash","Chinese","Burmese","Bulgarian",
    "Bengali","Belorussian","Bambora","Azerbaijan",
    "Assamese","Armenian","Arabic","Amharic"
  });

  std::string result("");
  if (code < languages.size()) {
    result = languages[code];
  }

  return result;
}

std::string getAppName(uint16_t aid) {
  std::map<uint16_t,std::string> oda_apps({
    { 0x0000, "None" },
    { 0x0093, "Cross referencing DAB within RDS" },
    { 0x0BCB, "Leisure & Practical Info for Drivers" },
    { 0x0C24, "ELECTRABEL-DSM 7" },
    { 0x0CC1, "Wireless Playground broadcast control signal" },
    { 0x0D45, "RDS-TMC: ALERT-C / EN ISO 14819-1" },
    { 0x0D8B, "ELECTRABEL-DSM 18" },
    { 0x0E2C, "ELECTRABEL-DSM 3" },
    { 0x0E31, "ELECTRABEL-DSM 13" },
    { 0x0F87, "ELECTRABEL-DSM 2" },
    { 0x125F, "I-FM-RDS for fixed and mobile devices" },
    { 0x1BDA, "ELECTRABEL-DSM 1" },
    { 0x1C5E, "ELECTRABEL-DSM 20" },
    { 0x1C68, "ITIS In-vehicle data base" },
    { 0x1CB1, "ELECTRABEL-DSM 10" },
    { 0x1D47, "ELECTRABEL-DSM 4" },
    { 0x1DC2, "CITIBUS 4" },
    { 0x1DC5, "Encrypted TTI using ALERT-Plus" },
    { 0x1E8F, "ELECTRABEL-DSM 17" },
    { 0x4AA1, "RASANT" },
    { 0x4AB7, "ELECTRABEL-DSM 9" },
    { 0x4BA2, "ELECTRABEL-DSM 5" },
    { 0x4BD7, "RadioText+ (RT+)" },
    { 0x4C59, "CITIBUS 2" },
    { 0x4D87, "Radio Commerce System (RCS)" },
    { 0x4D95, "ELECTRABEL-DSM 16" },
    { 0x4D9A, "ELECTRABEL-DSM 11" },
    { 0x5757, "Personal weather station" },
    { 0x6552, "Enhanced RadioText (eRT)" },
    { 0x7373, "Enhanced early warning system" },
    { 0xC350, "NRSC Song Title and Artist" },
    { 0xC3A1, "Personal Radio Service" },
    { 0xC3B0, "iTunes Tagging" },
    { 0xC3C3, "NAVTEQ Traffic Plus" },
    { 0xC4D4, "eEAS" },
    { 0xC549, "Smart Grid Broadcast Channel" },
    { 0xC563, "ID Logic" },
    { 0xC6A7, "Veil Enabled Interactive Device" },
    { 0xC737, "Utility Message Channel (UMC)" },
    { 0xCB73, "CITIBUS 1" },
    { 0xCB97, "ELECTRABEL-DSM 14" },
    { 0xCC21, "CITIBUS 3" },
    { 0xCD46, "RDS-TMC: ALERT-C" },
    { 0xCD47, "RDS-TMC: ALERT-C" },
    { 0xCD9E, "ELECTRABEL-DSM 8" },
    { 0xCE6B, "Encrypted TTI using ALERT-Plus" },
    { 0xE123, "APS Gateway" },
    { 0xE1C1, "Action code" },
    { 0xE319, "ELECTRABEL-DSM 12" },
    { 0xE411, "Beacon downlink" },
    { 0xE440, "ELECTRABEL-DSM 15" },
    { 0xE4A6, "ELECTRABEL-DSM 19" },
    { 0xE5D7, "ELECTRABEL-DSM 6" },
    { 0xE911, "EAS open protocol" }
  });

  std::string result("(Unknown)");
  if (oda_apps.find(aid) != oda_apps.end()) {
    result = oda_apps[aid];
  }

  return result;
}

}
