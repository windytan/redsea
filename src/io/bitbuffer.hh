#ifndef BITBUFFER_H
#define BITBUFFER_H

#include <array>
#include <chrono>
#include <vector>

namespace redsea {

struct TimedBit {
  // 1 / 0
  bool value;
  // Time offset of the bit, in seconds from the start of the input chunk
  float time_from_chunk_start;
};

// Bits as demodulated from MPX
struct BitBuffer {
  // Timestamp for when the *last* bit of the group was received, in system time
  std::chrono::time_point<std::chrono::system_clock> time_received;
  // Time offset of the first sample of the input chunk, in seconds
  double chunk_time_from_start{};
  // Number of data-streams (1 to 4)
  int n_streams{1};
  // One vector for each data-stream
  std::array<std::vector<TimedBit>, 4> bits;
};

}  // namespace redsea

#endif  // BITBUFFER_H
