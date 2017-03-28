#include "src/rdsstring.h"

#include <string>
#include <utility>

#include "src/common.h"
#include "src/tables.h"

namespace redsea {

namespace {

std::string rtrim(std::string s) {
  return s.erase(s.find_last_not_of(' ')+1);
}

}

RDSChar::RDSChar() : code_(0), codetable_(G0) {
}

RDSChar::RDSChar(uint8_t _code) : code_(_code) {
}

void RDSChar::setCodeTable(eCodeTable codetable) {
  codetable_ = codetable;
}

uint8_t RDSChar::code() const {
  return code_;
}

std::string RDSChar::str() const {
  return RDSCharString(code_, codetable_);
}

RDSString::RDSString(int len) : chars_(len), is_char_sequential_(len),
  prev_pos_(-1), last_complete_string_(str()) {
}

void RDSString::set(int pos, RDSChar chr) {
  if (pos < 0 || pos >= static_cast<int>(chars_.size()))
    return;

  chr.setCodeTable(repertoireAt(pos));

  chars_.at(pos) = chr;

  if (pos != prev_pos_ + 1) {
    for (size_t i=0; i < is_char_sequential_.size(); i++)
      is_char_sequential_[i] = false;
  }

  is_char_sequential_.at(pos) = true;

  if (complete()) {
    last_complete_string_ = str();
    last_complete_chars_ = chars();
  }

  prev_pos_ = pos;
}

void RDSString::set(int pos, RDSChar chr1, RDSChar chr2) {
  if (chr1.code() == 0x0F && chr2.code() == 0x0F) {
    setRepertoire(pos, G0);
  } else if (chr1.code() == 0x0E && chr2.code() == 0x0E) {
    setRepertoire(pos, G1);
  } else if (chr1.code() == 0x1B && chr2.code() == 0x6E) {
    setRepertoire(pos, G2);
  } else {
    set(pos, chr1);
    set(pos + 1, chr2);
  }
}

void RDSString::setRepertoire(int pos, eCodeTable codetable) {
  if (pos >= 0)
    repertoire_[pos] = codetable;
}

eCodeTable RDSString::repertoireAt(int pos) const {
  eCodeTable codetable = G0;
  for (std::pair<int, eCodeTable> r : repertoire_)
    if (pos >= r.first)
      codetable = r.second;
  return codetable;
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
  chars_.resize(n, RDSChar(0x20));
}

std::string RDSString::str() const {
  std::string result;
  std::vector<RDSChar> _chars = chars();
  for (size_t pos=0; pos < _chars.size(); pos++) {
    result += _chars[pos].str();
  }

  return result;
}

std::vector<RDSChar> RDSString::chars() const {
  std::vector<RDSChar> result;
  size_t len = lengthExpected();
  for (size_t i=0; i < len; i++) {
    result.push_back(is_char_sequential_[i] ? chars_[i] : RDSChar(0x20));
  }

  return result;
}

std::string RDSString::getTrimmedString() const {
  return rtrim(str());
}

std::string RDSString::last_complete_string() const {
  return last_complete_string_;
}

std::string RDSString::last_complete_string(int start, int len) const {
  std::string result;
  for (int i=start; i < start+len; i++) {
    result += (i < static_cast<int>(last_complete_chars_.size()) ?
        last_complete_chars_[i].str() : " ");
  }

  return result;
}

std::string RDSString::last_complete_string_trimmed() const {
  return rtrim(last_complete_string_);
}

std::string RDSString::last_complete_string_trimmed(int start, int len) const {
  return rtrim(last_complete_string(start, len));
}

bool RDSString::has_chars(int start, int len) const {
  return start+len <= static_cast<int>(last_complete_chars_.size());
}

bool RDSString::complete() const {
  return lengthReceived() >= lengthExpected();
}

void RDSString::clear() {
  for (size_t i=0; i < chars_.size(); i++) {
    is_char_sequential_[i] = false;
  }
  last_complete_string_ = str();
  last_complete_chars_.clear();
  repertoire_.clear();
}

}  // namespace redsea
