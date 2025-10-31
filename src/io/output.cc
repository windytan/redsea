#include "output.hh"

#include "src/group.hh"
#include "src/options.hh"
#include "src/util/tree.hh"
#include "src/util/util.hh"

#include <iomanip>
#include <ostream>
#include <sstream>
#include <variant>

#include <nlohmann/json.hpp>

namespace redsea {

namespace {

// Convert an ObjectTree to nlohmann::json.
// NOLINTNEXTLINE(misc-no-recursion)
nlohmann::ordered_json toJson(const ObjectTree& lj) {
  const auto& v = lj.get();
  if (std::holds_alternative<std::nullptr_t>(v))
    return nullptr;
  if (std::holds_alternative<bool>(v))
    return std::get<bool>(v);
  if (std::holds_alternative<int>(v))
    return std::get<int>(v);
  if (std::holds_alternative<double>(v))
    return std::get<double>(v);
  if (std::holds_alternative<std::string>(v))
    return std::get<std::string>(v);
  if (std::holds_alternative<ObjectTree::object_t>(v)) {
    nlohmann::ordered_json j = nlohmann::ordered_json::object();
    for (auto& [k, child] : std::get<ObjectTree::object_t>(v)) j[k] = toJson(child);
    return j;
  }
  if (std::holds_alternative<ObjectTree::array_t>(v)) {
    nlohmann::ordered_json j = nlohmann::ordered_json::array();
    for (auto& child : std::get<ObjectTree::array_t>(v)) j.push_back(toJson(child));
    return j;
  }
  return {};  // should not happen
}

}  // namespace

void printAsHex(const Group& group, const Options& options, std::ostream& output_ostream) {
  if (!group.isEmpty()) {
    if (group.getDataStream() > 0) {
      output_ostream << "#S" << group.getDataStream() << ' ';
    }
    output_ostream << group.asHex();
    if (options.timestamp) {
      output_ostream << ' ' << getTimePointString(group.getRxTime().value, options.time_format);
    }
    if (options.time_from_start && group.getTimeFromStart().has_value) {
      output_ostream << ' ' << std::fixed << std::setprecision(6) << group.getTimeFromStart().value;
    }
    output_ostream << '\n' << std::flush;
  }
}

void printAsJson(const ObjectTree& tree, std::ostream& output_ostream) {
  const nlohmann::ordered_json json = toJson(tree);

  try {
    // nlohmann::operator<< throws if a string contains non-UTF8 data!
    // It's better to throw while writing to a stringstream; otherwise
    // incomplete JSON objects could get printed.
    std::stringstream output_proxy_stream;
    output_proxy_stream << json;
    output_ostream << output_proxy_stream.str() << std::endl;
  } catch (const std::exception& e) {
    nlohmann::ordered_json json_from_exception;
    json_from_exception["debug"] = std::string(e.what());
    output_ostream << json_from_exception << std::endl;
  }
}

}  // namespace redsea
