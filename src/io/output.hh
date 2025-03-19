#ifndef REDSEA_OUTPUT_HH
#define REDSEA_OUTPUT_HH

#include <ostream>

#include "src/groups.hh"
#include "src/options.hh"
#include "src/util.hh"

namespace redsea {

inline void printAsHex(const Group& group, const Options& options, std::ostream& output_stream) {
  if (!group.isEmpty()) {
    if (group.getDataStream() > 0)
      output_stream << "#S" << group.getDataStream() << " ";
    group.printHex(output_stream);
    if (options.timestamp)
      output_stream << ' ' << getTimePointString(group.getRxTime(), options.time_format);
    output_stream << '\n' << std::flush;
  }
}

}  // namespace redsea

#endif  // REDSEA_OUTPUT_HH
