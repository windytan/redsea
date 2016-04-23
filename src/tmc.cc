#include "tmc.h"

#include "util.h"

namespace redsea {

TMC::TMC() : is_initialized_(false), ps_(8) {

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

    bool f = bits(x, 3, 1);

    // Single-group message
    if (f) {
      uint16_t duration  = bits(x, 0, 3);
      uint16_t divertadv = bits(y, 15, 1);
      uint16_t direction = bits(y, 14, 1);
      uint16_t extent    = bits(y, 11, 3);
      uint16_t events0   = bits(y, 0, 11);
      uint16_t location  = z;

      printf(", tmc: { traffic_message: { event: %d, %slocation: %d, direction: \"%s\", "
             "extent: %d, diversion_advised: %s }",
             events0, (is_encrypted_ ? "encrypted_" : ""), location, direction ? "negative" : "positive",
             extent, divertadv ? "true" : "false" );

    } else {
      printf(", tmc: { /* TODO multi-group message */ }");
    }
  }

}

} // namespace redsea
