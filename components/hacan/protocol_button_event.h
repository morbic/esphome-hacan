#pragma once

#include <cstdint>

namespace hacan::protocol {

enum class ButtonEvent : std::uint8_t {
  kPress = 0x01,
  kRelease = 0x02,
  kClick = 0x03,
  kDoubleClick = 0x04,
  kLongPress = 0x05,
};

}  // namespace hacan::protocol
