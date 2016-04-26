#include "tmc.h"

#include "tmc_events.h"
#include "util.h"

namespace redsea {

namespace tmc {

TMC::TMC() : is_initialized_(false), has_encid_(false), ps_(8), multi_group_buffer_(5) {

}

void TMC::systemGroup(uint16_t message) {

  if (bits(message, 14, 1) == 0) {
    printf(", tmc: { system_info: { ");

    is_initialized_ = true;
    ltn_ = bits(message, 6, 6);
    is_encrypted_ = (ltn_ == 0);

    printf("is_encrypted: %s", is_encrypted_ ? "true" : "false");

    if (!is_encrypted_)
      printf(", location_table: \"0x%02x\"", ltn_);

    bool afi   = bits(message, 5, 1);
    bool m     = bits(message, 4, 1);
    bool mgs_i = bits(message, 3, 1);
    bool mgs_n = bits(message, 2, 1);
    bool mgs_r = bits(message, 1, 1);
    bool mgs_u = bits(message, 0, 1);

    printf(", is_on_alt_freqs: %s", afi ? "true" : "false");

    std::vector<std::string> scope;
    if (mgs_i)
      scope.push_back("\"inter-road\"");
    if (mgs_n)
      scope.push_back("\"national\"");
    if (mgs_r)
      scope.push_back("\"regional\"");
    if (mgs_u)
      scope.push_back("\"urban\"");

    printf(", scope: [ %s ]", commaJoin(scope).c_str());

    printf(" } }");
  }

}

void TMC::userGroup(uint16_t x, uint16_t y, uint16_t z) {

  if (!is_initialized_)
    return;

  bool t = bits(x, 4, 1);

  // Encryption administration group
  if (bits(x, 0, 5) == 0x00) {
    sid_   = bits(y, 5, 6);
    encid_ = bits(y, 0, 5);
    ltnbe_ = bits(z, 10, 6);
    has_encid_ = true;

    printf(", tmc: { service_id: \"0x%02x\", encryption_id: \"0x%02x\", location_table: \"0x%02x\" }",
        sid_, encid_, ltnbe_);

  // Tuning information
  } else if (t) {
    uint16_t variant = bits(x, 0, 4);

    if (variant == 4 || variant == 5) {

      int pos = 4 * (variant - 4);

      ps_.setAt(pos,   bits(y, 8, 8));
      ps_.setAt(pos+1, bits(y, 0, 8));
      ps_.setAt(pos+2, bits(z, 8, 8));
      ps_.setAt(pos+3, bits(z, 0, 8));

      if (ps_.isComplete())
        printf(", tmc: { service_provider: \"%s\" }", ps_.getLastCompleteString().c_str());

    } else {
      printf(", tmc: { /* TODO: tuning info variant %d */ }", variant);
    }

  // User message
  } else {

    if (is_encrypted_ && !has_encid_)
      return;

    bool f = bits(x, 3, 1);

    // Single-group message
    if (f) {
      newMessage(false, {{true, {x, y, z}}});
      current_ci_ = 0;

    // Part of multi-group message
    } else {

      uint16_t ci = bits(x, 0, 3);
      bool     fg = bits(y, 15, 1);

      if (ci != current_ci_ /* TODO 15-second limit */) {
        newMessage(true, multi_group_buffer_);
        multi_group_buffer_[0].is_received = false;
        current_ci_ = ci;
      }

      int cur_grp;

      if (fg)
        cur_grp = 0;
      else if (bits(y, 14, 1))
        cur_grp = 1;
      else
        cur_grp = 4 - bits(y, 12, 2);

      multi_group_buffer_.at(cur_grp) = {true, {y, z}};

      //printf(", tmc: { /* TODO multi-group message */ }");
    }
  }

}

void TMC::newMessage(bool is_multi, std::vector<MessagePart> parts) {

  Message message;

  // single-group
  if (!is_multi) {
    message.duration  = bits(parts[0].data[0], 0, 3);
    message.divertadv = bits(parts[0].data[1], 15, 1);
    message.direction = bits(parts[0].data[1], 14, 1);
    message.extent    = bits(parts[0].data[1], 11, 3);
    message.events.push_back(bits(parts[0].data[1], 0, 11));
    message.location  = parts[0].data[2];
    message.is_complete = true;

  // multi-group
  } else {

    // Need at least the first group
    if (!parts[0].is_received)
      return;

    // First group
    message.direction = bits(parts[0].data[0], 14, 1);
    message.extent    = bits(parts[0].data[0], 11, 3);
    message.events.push_back(bits(parts[0].data[0], 0, 11));
    message.location  = parts[0].data[1];

    // Subsequent parts, TODO
    if (parts[1].is_received) {

      uint16_t sg_gsi = bits(parts[1].data[0], 12, 2);
      //printf("tmc: sg_gsi=%d\n",sg_gsi);

      for (int i=0; i<parts.size(); i++) {
        //printf("  tmc: %d=%d\n", i,parts[i].is_received);
      }

    }
  }

  printf(", tmc: { traffic_message: { ");
  if (message.events.size() > 1) {
    printf("events: [ %s ]", commaJoin(message.events).c_str());
  } else {
    printf("event: { code: %d, description: \"%s\" }", message.events[0], getEvent(message.events[0]).description.c_str());
  }

  printf(", %slocation: \"0x%02x\", direction: \"%s\", "
         "extent: %d, diversion_advised: %s } }",
         (is_encrypted_ ? "encrypted_" : ""), message.location,
         message.direction ? "negative" : "positive",
         message.extent, message.divertadv ? "true" : "false" );

}

Message::Message() : events() {

}

} // namespace tmc
} // namespace redsea
