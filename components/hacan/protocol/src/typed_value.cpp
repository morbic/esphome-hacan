#include <hacan/protocol/typed_value.h>

namespace hacan::protocol {
namespace {

std::uint8_t value_width(DataType type) {
  switch (type) {
    case DataType::kNull:
    case DataType::kInvalid:
      return 0;
    case DataType::kBool:
    case DataType::kUInt8:
    case DataType::kInt8:
    case DataType::kEnum8:
      return 1;
    case DataType::kUInt16:
    case DataType::kInt16:
      return 2;
    case DataType::kUInt32:
    case DataType::kInt32:
    case DataType::kFloat32:
    case DataType::kFixed16_16:
    case DataType::kBitset32:
      return 4;
  }
  return 0xFF;
}

bool is_signaling_nan(const std::array<std::uint8_t, 4>& bytes) {
  const std::uint32_t bits = static_cast<std::uint32_t>(bytes[0]) |
                             (static_cast<std::uint32_t>(bytes[1]) << 8U) |
                             (static_cast<std::uint32_t>(bytes[2]) << 16U) |
                             (static_cast<std::uint32_t>(bytes[3]) << 24U);
  const bool exponent_is_all_ones = (bits & 0x7F800000U) == 0x7F800000U;
  const bool has_payload = (bits & 0x007FFFFFU) != 0;
  const bool quiet_bit_is_clear = (bits & 0x00400000U) == 0;
  return exponent_is_all_ones && has_payload && quiet_bit_is_clear;
}

}  // namespace

ValueError TypedValue::validate() const {
  const std::uint8_t width = value_width(type_);
  if (width == 0xFF) {
    return ValueError::kUnknownDataType;
  }
  if (type_ == DataType::kBool && bytes_[0] > 1) {
    return ValueError::kInvalidBoolean;
  }
  for (std::uint8_t index = width; index < bytes_.size(); ++index) {
    if (bytes_[index] != 0) {
      return ValueError::kNonZeroUnusedBytes;
    }
  }
  if (type_ == DataType::kFloat32 && is_signaling_nan(bytes_)) {
    return ValueError::kSignalingNan;
  }
  return ValueError::kNone;
}

}  // namespace hacan::protocol
