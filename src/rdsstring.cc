#include "rdsstring.h"

#include <string>

#include "tables.h"

namespace redsea {

namespace {

std::string rtrim(std::string s) {
  return s.erase(s.find_last_not_of(' ')+1);
}

}

LCDchar::LCDchar() : code_(0) {

}

LCDchar::LCDchar(uint8_t _code) : code_(_code) {

}

uint8_t LCDchar::getCode() const {
  return code_;
}

std::string LCDchar::toString() const {
  return getLCDchar(code_);
}

RDSString::RDSString(int len) : chars_(len), is_char_sequential_(len),
  prev_pos_(-1), last_complete_string_(getString()) {

}

void RDSString::setAt(int pos, LCDchar chr) {
  if (pos < 0 || pos >= (int)chars_.size())
    return;

  chars_.at(pos) = chr;

  if (pos != prev_pos_ + 1) {
    for (size_t i=0; i<is_char_sequential_.size(); i++)
      is_char_sequential_[i] = false;
  }

  is_char_sequential_.at(pos) = true;

  if (isComplete()) {
    last_complete_string_ = getString();
    last_complete_chars_ = getChars();
  }

  prev_pos_ = pos;

}

std::string RDSString::charAt(int pos) const {
  return (pos < (int)last_complete_chars_.size() ?
      last_complete_chars_[pos].toString() : " ");
}

size_t RDSString::lengthReceived() const {

  size_t result = 0;
  for (size_t i=0; i<is_char_sequential_.size(); i++) {
    if (!is_char_sequential_[i])
      break;
    result = i+1;
  }

  return result;
}

size_t RDSString::lengthExpected() const {

  size_t result = chars_.size();

  for (size_t i=0; i<chars_.size(); i++) {
    if (chars_[i].getCode() == 0x0D) {
      result = i;
      break;
    }
  }

  return result;
}

std::string RDSString::getString() const {

  std::string result;
  for (LCDchar chr : getChars()) {
    result += chr.toString();
  }

  return result;
}

std::vector<LCDchar> RDSString::getChars() const {
  std::vector<LCDchar> result;
  size_t len = lengthExpected();
  for (size_t i=0; i<len; i++) {
    result.push_back(is_char_sequential_[i] ? chars_[i] : 32);
  }

  return result;
}

std::string RDSString::getTrimmedString() const {
  return rtrim(getString());
}

std::string RDSString::getLastCompleteString() const {
  return last_complete_string_;
}

std::string RDSString::getLastCompleteString(int start, int len) const {

  std::string result;
  for (int i=start; i<start+len; i++) {
    result += (i < (int)last_complete_chars_.size() ?
        last_complete_chars_[i].toString() : " ");
  }

  return result;
}

std::string RDSString::getLastCompleteStringTrimmed() const {
  return rtrim(last_complete_string_);
}

std::string RDSString::getLastCompleteStringTrimmed(int start, int len) const {
  return rtrim(getLastCompleteString(start, len));
}

bool RDSString::hasChars(int start, int len) const {
  return start+len <= (int)last_complete_chars_.size();
}

bool RDSString::isComplete() const {
  return lengthReceived() >= lengthExpected();
}

void RDSString::clear() {
  for (size_t i=0; i<chars_.size(); i++) {
    is_char_sequential_[i] = false;
  }
  last_complete_string_ = getString();
  last_complete_chars_.clear();
}

} // namespace redsea
