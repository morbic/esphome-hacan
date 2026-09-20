#pragma once

#include <array>
#include <cstdint>

#include "protocol_types.h"

namespace hacan::protocol {

enum class ValueError : std::uint8_t {
  kNone,
  kUnknownDataType,
  kInvalidBoolean,
  kNonZeroUnusedBytes,
  kSignalingNan,
};

class TypedValue {
 public:
  constexpr TypedValue(DataType type, std::array<std::uint8_t, 4> bytes)
      : type_(type), bytes_(bytes) {}

  [[nodiscard]] constexpr DataType type() const { return type_; }
  [[nodiscard]] constexpr const std::array<std::uint8_t, 4>& bytes() const {
    return bytes_;
  }
  [[nodiscard]] ValueError validate() const;

 private:
  DataType type_;
  std::array<std::uint8_t, 4> bytes_;
};

}  // namespace hacan::protocol
