#ifndef DATA_H_
#define DATA_H_

#include <string>

namespace redsea {

std::string getLCDchar(uint8_t code);
std::string getPTYname(int pty, bool is_rbds=false);
std::string getCountryString(uint16_t pi, uint16_t ecc);
std::string getLanguageString(uint16_t code);
std::string getAppName(uint16_t aid);
std::string getRTPlusContentTypeName(uint16_t content_type);

} // namespace redsea
#endif // DATA_H_
