#ifndef BITBUFFER_H
#define BITBUFFER_H

#include <array>
#include <chrono>
#include <vector>

namespace redsea {

struct BitBuffer {
  std::chrono::time_point<std::chrono::system_clock> time_received;
  // One vector for each data-stream
  std::array<std::vector<int>, 4> bits;
};

}  // namespace redsea

#endif  // BITBUFFER_H
