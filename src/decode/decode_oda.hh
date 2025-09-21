#ifndef DECODE_ODA_HH_
#define DECODE_ODA_HH_

#include <cstdint>

#include "src/simplemap.hh"

namespace redsea {

class Group;
class GroupType;
class ObjectTree;
class RadioText;

namespace tmc {
class TMCService;
}

void decodeType3A(const Group& group, ObjectTree& tree,
                  SimpleMap<GroupType, std::uint16_t>& oda_app_for_group, RadioText& radiotext,
                  RadioText& ert, tmc::TMCService& tmc);
void decodeODAGroup(const Group& group, ObjectTree& tree,
                    const SimpleMap<GroupType, std::uint16_t>& oda_app_for_group,
                    RadioText& radiotext, RadioText& ert, tmc::TMCService& tmc);
void decodeEnhancedRT(const Group& group, ObjectTree& tree, RadioText& ert);
void decodeDAB(const Group& group, ObjectTree& tree);
void decodeRadioTextPlus(const Group& group, RadioText& rt, ObjectTree& tree_el);

}  // namespace redsea

#endif  // DECODE_ODA_HH_
