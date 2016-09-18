#ifndef RDSSTRING_H_
#define RDSSTRING_H_

#include <string>
#include <vector>

namespace redsea {

class LCDchar {
  public:
    LCDchar();
    LCDchar(uint8_t _code);
    uint8_t getCode() const;
    std::string toString() const;

  private:
    uint8_t code_;
};

class RDSString {
  public:
  RDSString(int len=8);
  void setAt(int pos, LCDchar chr);
  std::string charAt(int pos) const;
  size_t lengthReceived() const;
  size_t lengthExpected() const;
  std::vector<LCDchar> getChars() const;
  std::string getString() const;
  std::string getTrimmedString() const;
  std::string getLastCompleteString() const;
  std::string getLastCompleteString(int start, int len) const;
  std::string getLastCompleteStringTrimmed() const;
  std::string getLastCompleteStringTrimmed(int start, int len) const;
  bool hasChars(int start, int len) const;
  bool isComplete() const;
  void clear();

  private:
  std::vector<LCDchar> chars_;
  std::vector<LCDchar> last_complete_chars_;
  std::vector<bool> is_char_sequential_;
  int prev_pos_;
  std::string last_complete_string_;

};

} // namespace redsea
#endif // RDSSTRING_H_
