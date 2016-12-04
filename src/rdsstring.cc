#include "src/rdsstring.h"

#include <string>

#include "src/tables.h"

namespace redsea {

namespace {

std::string rtrim(std::string s) {
  return s.erase(s.find_last_not_of(' ')+1);
}

}

LCDchar::LCDchar() : code_(0), codetable_(0) {
}

LCDchar::LCDchar(uint8_t _code) : code_(_code) {
}

void LCDchar::setCodeTable(int codetable) {
  codetable_ = codetable;
}

uint8_t LCDchar::code() const {
  return code_;
}

std::string LCDchar::toString() const {
  return getLCDchar(code_, codetable_);
}

RDSString::RDSString(int len) : chars_(len), is_char_sequential_(len),
  prev_pos_(-1), last_complete_string_(getString()) {
}

void RDSString::setAt(int pos, LCDchar chr) {
  if (pos < 0 || pos >= static_cast<int>(chars_.size()))
    return;

  chr.setCodeTable(repertoireAt(pos));

  chars_.at(pos) = chr;

  if (pos != prev_pos_ + 1) {
    for (size_t i=0; i < is_char_sequential_.size(); i++)
      is_char_sequential_[i] = false;
  }

  is_char_sequential_.at(pos) = true;

  if (isComplete()) {
    last_complete_string_ = getString();
    last_complete_chars_ = getChars();
  }

  prev_pos_ = pos;
}

void RDSString::setAt(int pos, LCDchar chr1, LCDchar chr2) {
  if (chr1.code() == 0x0F && chr2.code() == 0x0F) {
    setRepertoire(pos, 0);
  } else if (chr1.code() == 0x0E && chr2.code() == 0x0E) {
    setRepertoire(pos, 1);
  } else if (chr1.code() == 0x1B && chr2.code() == 0x6E) {
    setRepertoire(pos, 2);
  } else {
    setAt(pos, chr1);
    setAt(pos + 1, chr2);
  }
}

void RDSString::setRepertoire(int pos, int codetable) {
  if (pos >= 0)
    repertoire_[pos] = codetable;
}

int RDSString::repertoireAt(int pos) const {
  int codetable = 0;
  for (std::pair<int,int> r : repertoire_)
    if (pos >= r.first)
      codetable = r.second;
  return codetable;
}

std::string RDSString::charAt(int pos) const {
  return (pos < static_cast<int>(last_complete_chars_.size()) ?
      last_complete_chars_[pos].toString() : " ");
}

size_t RDSString::lengthReceived() const {
  size_t result = 0;
  for (size_t i=0; i < is_char_sequential_.size(); i++) {
    if (!is_char_sequential_[i])
      break;
    result = i+1;
  }

  return result;
}

size_t RDSString::lengthExpected() const {
  size_t result = chars_.size();

  for (size_t i=0; i < chars_.size(); i++) {
    if (chars_[i].code() == 0x0D) {
      result = i;
      break;
    }
  }

  return result;
}

void RDSString::resize(int n) {
  chars_.resize(n, 0x20);
}

std::string RDSString::getString() const {
  std::string result;
  std::vector<LCDchar> chars = getChars();
  for (size_t pos=0; pos < chars.size(); pos++) {
    result += chars[pos].toString();
  }

  return result;
}

std::vector<LCDchar> RDSString::getChars() const {
  std::vector<LCDchar> result;
  size_t len = lengthExpected();
  for (size_t i=0; i < len; i++) {
    result.push_back(is_char_sequential_[i] ? chars_[i] : 0x20);
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
  for (int i=start; i < start+len; i++) {
    result += (i < static_cast<int>(last_complete_chars_.size()) ?
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
  return start+len <= static_cast<int>(last_complete_chars_.size());
}

bool RDSString::isComplete() const {
  return lengthReceived() >= lengthExpected();
}

void RDSString::clear() {
  for (size_t i=0; i < chars_.size(); i++) {
    is_char_sequential_[i] = false;
  }
  last_complete_string_ = getString();
  last_complete_chars_.clear();
  repertoire_.clear();
}

}  // namespace redsea
