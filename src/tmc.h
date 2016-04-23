#ifndef TMC_H_
#define TMC_H_

#include "rdsstring.h"

namespace redsea {

class TMC {
  public:
    TMC();
    void systemGroup(uint16_t message);
    void userGroup(uint16_t x, uint16_t y, uint16_t z);
  private:
    bool is_initialized_;
    bool is_encrypted_;
    uint16_t ltn_;
    uint16_t sid_;
    uint16_t encid_;
    uint16_t ltnbe_;
    RDSString ps_;
};

}

#endif
