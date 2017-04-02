#ifndef COMMON_H_
#define COMMON_H_

#include <string>

namespace redsea {

const float kTargetSampleRate_Hz  = 171000.0f;

enum eInputType {
  INPUT_MPX_STDIN, INPUT_MPX_SNDFILE, INPUT_ASCIIBITS, INPUT_RDSSPY
};

enum eOutputType {
  OUTPUT_HEX, OUTPUT_JSON
};

enum eCodeTable {
  G0, G1, G2
};

enum eOpenDataApp {
  ODA_UNKNOWN, ODA_TMC, ODA_RTPLUS
};

struct Options {
  Options() : rbds(false), feed_thru(false), show_partial(false),
              just_exit(false), samplerate(kTargetSampleRate_Hz),
              input_type(INPUT_MPX_STDIN), output_type(OUTPUT_JSON) {}
  bool rbds;
  bool feed_thru;
  bool show_partial;
  bool just_exit;
  float samplerate;
  eInputType input_type;
  eOutputType output_type;
  std::string loctable_dir;
  std::string sndfilename;
};

}  // namespace redsea
#endif  // COMMON_H_
