#include "src/rft/base64.hh"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace redsea {

std::string asBase64(const void* data, std::size_t input_length) {
  constexpr std::array<char, 64> base64_table = {
      {'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
       'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
       'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
       'w', 'x', 'y', 'z', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '+', '/'}
  };

  if (data == nullptr || input_length == 0) {
    return "";
  }

  const auto* const bytes = static_cast<const std::uint8_t*>(data);

  const std::size_t output_length = ((input_length + 2) / 3) * 4;
  std::string encoded;
  encoded.reserve(output_length);

  // Process input in 3-byte chunks
  for (std::size_t i = 0; i < input_length; i += 3) {
    std::uint32_t chunk = 0;
    int nbytes_in_chunk = 0;

    for (const int j : {0, 1, 2}) {
      if (i + j < input_length) {
        chunk |= static_cast<std::uint32_t>(bytes[i + j]) << (16 - j * 8);
        ++nbytes_in_chunk;
      }
    }

    // Encode 4 Base64 characters
    encoded.push_back(base64_table[(chunk >> 18U) & 0x3FU]);
    encoded.push_back(base64_table[(chunk >> 12U) & 0x3FU]);

    if (nbytes_in_chunk > 1) {
      encoded.push_back(base64_table[(chunk >> 6U) & 0x3FU]);
    } else {
      encoded.push_back('=');
    }

    if (nbytes_in_chunk == 3) {
      encoded.push_back(base64_table[chunk & 0x3FU]);
    } else {
      encoded.push_back('=');
    }
  }

  return encoded;
}

}  // namespace redsea
