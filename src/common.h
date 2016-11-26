#ifndef COMMON_H_
#define COMMON_H_

namespace redsea {

enum eInputType {
  INPUT_MPX, INPUT_ASCIIBITS, INPUT_RDSSPY
};

enum eOutputType {
  OUTPUT_HEX, OUTPUT_JSON
};

struct Options {
  Options() : rbds(false), feed_thru(false), show_partial(false),
              just_exit(false), input_type(INPUT_MPX),
              output_type(OUTPUT_JSON) {}
  bool rbds;
  bool feed_thru;
  bool show_partial;
  bool just_exit;
  eInputType input_type;
  eOutputType output_type;
};

}  // namespace redsea
#endif  // COMMON_H_
