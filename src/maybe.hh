#ifndef MAYBE_H_
#define MAYBE_H_

namespace redsea {

template <typename T>
struct Maybe {
  T data;
  bool valid{};
};

}  // namespace redsea

#endif  // MAYBE_H_
