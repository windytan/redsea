#ifndef DATA_H_
#define DATA_H_

#include <string>

#include "src/common.h"

namespace redsea {

std::string RDSCharString(uint8_t code, eCodeTable codetable = G0);
std::string PTYNameString(int pty, bool is_rbds = false);
std::string CountryString(uint16_t pi, uint16_t ecc);
std::string LanguageString(uint16_t code);
std::string AppNameString(uint16_t aid);
std::string RTPlusContentTypeString(uint16_t content_type);
std::string DICodeString(uint16_t di);

}  // namespace redsea
#endif  // DATA_H_
