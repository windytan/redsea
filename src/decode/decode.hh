#ifndef DECODE_HH_
#define DECODE_HH_

#include <array>
#include <cstdint>

#include "src/simplemap.hh"

namespace redsea {

class AltFreqList;
class Group;
class LongPS;
class ObjectTree;
class ProgramServiceName;
class PTYName;
class RadioText;
class RDSString;
class RFTFile;

struct SlowLabelingCodes {
  bool linkage_la{};
  bool has_country{};
  int tmc_id{};
  std::uint16_t ecc{};
  std::uint16_t cc{};
  std::uint16_t pin{};
};

void decodeBasics(const Group& group, ObjectTree& tree, bool rbds);
void decodeType0(const Group& group, ObjectTree& tree, AltFreqList& alt_freq_list,
                 ProgramServiceName& ps, bool show_partial);
void decodeType1(const Group& group, ObjectTree& tree, SlowLabelingCodes& slc, std::uint16_t pi);
void decodeType2(const Group& group, ObjectTree& tree, RadioText& radiotext, bool show_partial);
void decodeType4A(const Group& group, ObjectTree& tree);
void decodeType5(const Group& group, ObjectTree& tree, RDSString& full_tdc);
void decodeType6(const Group& group, ObjectTree& tree);
void decodeType7A(const Group& group, ObjectTree& tree);
void decodeType9A(const Group& group, ObjectTree& tree);
void decodeType10A(const Group& group, ObjectTree& tree, PTYName& ptyname);
void decodeType14(const Group& group, ObjectTree& tree,
                  SimpleMap<std::uint16_t, RDSString>& eon_ps_names,
                  SimpleMap<std::uint16_t, AltFreqList>& eon_alt_freqs, bool rbds);
void decodeType15A(const Group& group, ObjectTree& tree, LongPS& long_ps, bool show_partial);
void decodeType15B(const Group& group, ObjectTree& tree);
void decodeC(const Group& group, ObjectTree& tree,
             SimpleMap<std::uint16_t, std::uint16_t>& oda_app_for_pipe,
             std::array<RFTFile, 16>& rft_file);
void decodeEnhancedRT(const Group& group, ObjectTree& tree, RadioText& ert);
void decodeDAB(const Group& group, ObjectTree& tree);

bool decodePIN(std::uint16_t pin, ObjectTree& tree);

void decodeRadioTextPlus(const Group& group, RadioText& rt, ObjectTree& tree_el);

}  // namespace redsea

#endif  // DECODE_HH_
