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
    Message(bool is_multi, bool is_loc_encrypted, std::vector<MessagePart> parts);
    std::string toString() const;
    void print() const;
    bool is_encrypted;
    uint16_t duration;
    uint16_t duration_type;
    bool divertadv;
    uint16_t direction;
    uint16_t extent;
    std::vector<uint16_t> events;
    std::vector<uint16_t> supplementary;
    uint16_t location;
    bool is_complete;
    bool has_length_affected;
    uint16_t length_affected;
    bool has_time_until;
    uint16_t time_until;
    bool has_time_starts;
    uint16_t time_starts;
};

} // namespace tmc
} // namespace redsea
#endif // TMC_H_
