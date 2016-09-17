#ifndef RDSSTRING_H_
#define RDSSTRING_H_

#include <string>
#include <vector>

namespace redsea {

class RDSString {
  public:
  RDSString(int len=8);
  void setAt(int, int);
  std::string charAt(int) const;
  size_t lengthReceived() const;
  size_t lengthExpected() const;
  std::string getString() const;
  std::string getTrimmedString() const;
  std::string getLastCompleteString() const;
  std::string getLastCompleteStringTrimmed() const;
  bool isComplete() const;
  void clear();

  private:
  std::vector<int> chars_;
  std::vector<int> last_complete_chars_;
  std::vector<bool> is_char_sequential_;
  int prev_pos_;
  std::string last_complete_string_;

};

} // namespace redsea
#endif // RDSSTRING_H_
