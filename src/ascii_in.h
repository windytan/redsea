#ifndef ASCII_IN_H_
#define ASCII_IN_H_

#include <cstdint>
#include <vector>

#include "src/groups.h"

namespace redsea {

class AsciiBits {
 public:
  AsciiBits(bool feed_thru = false);
  ~AsciiBits();
  int getNextBit();
  bool isEOF() const;

 private:
  bool is_eof_;
  bool feed_thru_;
};

Group getNextGroupRSpy(bool feed_thru = false);

}  // namespace redsea
#endif // ASCII_IN_H_
