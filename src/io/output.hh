#ifndef REDSEA_OUTPUT_HH
#define REDSEA_OUTPUT_HH

#include <iosfwd>

namespace redsea {

class Group;
class ObjectTree;
struct Options;

std::string formatHex(const Group& group);
void printAsHex(const Group& group, const Options& options, std::ostream& output_stream);
void printAsJson(const ObjectTree& tree, std::ostream& stream);

}  // namespace redsea

#endif  // REDSEA_OUTPUT_HH
