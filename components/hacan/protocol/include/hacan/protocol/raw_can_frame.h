#pragma once

#include <array>
#include <cstdint>

namespace hacan::protocol {

struct RawCanFrame {
  std::uint32_t can_id;
  bool extended;
  std::uint8_t dlc;
  std::array<std::uint8_t, 8> data;
};

}  // namespace hacan::protocol
