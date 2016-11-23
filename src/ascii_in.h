#ifndef ASCII_IN_H_
#define ASCII_IN_H_

#include <cstdint>
#include <vector>

#include "groups.h"

namespace redsea {

class AsciiBits {

  public:
    AsciiBits(bool has_echo=false);
    ~AsciiBits();
    int getNextBit();
    bool isEOF() const;

  private:
    bool is_eof_;
    bool echo_stdout_;

};

Group getNextGroupRSpy();

} // namespace redsea
#endif // ASCII_IN_H_
