#ifndef RFT_BASE64_H_
#define RFT_BASE64_H_

#include <cstddef>
#include <string>

namespace redsea {

std::string asBase64(const void* data, std::size_t input_length);

}  // namespace redsea

#endif  // RFT_BASE64_H_
