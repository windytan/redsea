#ifndef REDSEA_OUTPUT_HH
#define REDSEA_OUTPUT_HH

#include <iosfwd>

namespace redsea {

class Group;
struct Options;

void printAsHex(const Group& group, const Options& options, std::ostream& output_stream);

}  // namespace redsea

#endif  // REDSEA_OUTPUT_HH
