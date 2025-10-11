#ifndef MAYBE_H_
#define MAYBE_H_

namespace redsea {

template <typename T>
struct Maybe {
  T value;
  bool has_value{};

  Maybe() = default;

  explicit Maybe(const T& v) : value(v), has_value(true) {}
  Maybe(const T& v, bool valid) : value(v), has_value(valid) {}

  ~Maybe() = default;

  Maybe(const Maybe<T>& other)     = default;
  Maybe(Maybe<T>&& other) noexcept = default;

  Maybe<T>& operator=(const Maybe<T>& other)     = default;
  Maybe<T>& operator=(Maybe<T>&& other) noexcept = default;

  Maybe<T>& operator=(const T& v) {
    value     = v;
    has_value = true;
    return *this;
  }
};

}  // namespace redsea

#endif  // MAYBE_H_
