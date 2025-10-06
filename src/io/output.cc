#include "output.hh"

#include "src/groups.hh"
#include "src/options.hh"

#include <iomanip>
#include <ostream>

namespace redsea {

void printAsHex(const Group& group, const Options& options, std::ostream& output_stream) {
  if (!group.isEmpty()) {
    if (group.getDataStream() > 0) {
      output_stream << "#S" << group.getDataStream() << " ";
    }
    group.printHex(output_stream);
    if (options.timestamp) {
      output_stream << ' ' << getTimePointString(group.getRxTime(), options.time_format);
    }
    if (options.time_from_start && group.getTimeFromStart().valid) {
      output_stream << ' ' << std::fixed << std::setprecision(6) << group.getTimeFromStart().data;
    }
    output_stream << '\n' << std::flush;
  }
}

}  // namespace redsea
