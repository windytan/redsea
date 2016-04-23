#ifndef TMC_H_
#define TMC_H_

#include <vector>

#include "rdsstring.h"

namespace redsea {
namespace tmc {

struct MessagePart {
  MessagePart() {};
  MessagePart(bool _is_received, std::vector<uint16_t> _data) : is_received(_is_received),
    data(_data) {};
  bool is_received;
  std::vector<uint16_t> data;
};

class TMC {
  public:
    TMC();
    void systemGroup(uint16_t message);
    void userGroup(uint16_t x, uint16_t y, uint16_t z);
  private:
    void newMessage(bool,std::vector<MessagePart>);
    bool is_initialized_;
    bool is_encrypted_;
    bool has_encid_;
    uint16_t ltn_;
    uint16_t sid_;
    uint16_t encid_;
    uint16_t ltnbe_;
    uint16_t current_ci_;
    std::vector<MessagePart> multi_group_buffer_;
    RDSString ps_;
};

class Message {
  public:
    Message();
    uint16_t duration;
    bool divertadv;
    uint16_t direction;
    uint16_t extent;
    std::vector<uint16_t> events;
    uint16_t location;
    bool is_complete;
};

} // namespace tmc
} // namespace redsea
#endif // TMC_H_
