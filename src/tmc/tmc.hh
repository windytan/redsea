/*
 * Copyright (c) Oona Räisänen
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 */
#ifndef TMC_TMC_H_
#define TMC_TMC_H_

#include <cstdint>
#include <utility>
#include <vector>

#include "src/freq.hh"
#include "src/text/rdsstring.hh"
#include "src/tmc/message.hh"

namespace redsea {

class ObjectTree;
struct Options;

}  // namespace redsea

namespace redsea::tmc {

class Message;

class TMCService {
 public:
  explicit TMCService(const Options& options);
  void receiveSystemGroup(std::uint16_t message, ObjectTree& tree);
  void receiveUserGroup(std::uint16_t x, std::uint16_t y, std::uint16_t z, ObjectTree& tree);

 private:
  bool is_initialized_{false};
  bool is_encrypted_{false};
  bool has_encid_{false};
  std::uint16_t ltn_{0};
  std::uint16_t sid_{0};
  std::uint16_t encid_{0};
  std::uint16_t ltcc_{0};
  Message message_;
  std::vector<std::pair<std::uint16_t, ServiceKey>> service_key_table_;
  RDSString ps_;
  std::vector<std::pair<std::uint16_t, AltFreqList>> other_network_freqs_;
};

}  // namespace redsea::tmc

#endif  // TMC_H_
