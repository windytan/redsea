#ifndef RDSSTRING_H_
#define RDSSTRING_H_

#include <map>
#include <string>
#include <vector>

namespace redsea {

class LCDchar {
  public:
    LCDchar();
    LCDchar(uint8_t _code);
    uint8_t code() const;
    std::string toString() const;
    void setCodeTable(int codetable);

  private:
    uint8_t code_;
    int codetable_;
};

class RDSString {
 public:
  RDSString(int len=8);
  void setAt(int pos, LCDchar chr);
  void setAt(int pos, LCDchar chr1, LCDchar chr2);
  void setRepertoire(int pos, int codetable);
  int repertoireAt(int pos) const;
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
  void resize(int n);

 private:
  std::vector<LCDchar> chars_;
  std::vector<LCDchar> last_complete_chars_;
  std::vector<bool> is_char_sequential_;
  int prev_pos_;
  std::string last_complete_string_;
  std::map<int,int> repertoire_;

};

} // namespace redsea
#endif // RDSSTRING_H_
