#pragma once

#include <variant>

#include <hacan/protocol/protocol_frame.h>
#include <hacan/protocol/raw_can_frame.h>

namespace hacan::protocol {

enum class DecodeError : std::uint8_t {
  kNotExtended,
  kInvalidDlc,
  kInvalidIdentifier,
  kUnknownKind,
  kInvalidPriority,
  kInvalidAddressing,
  kInvalidPayload,
};

enum class EncodeError : std::uint8_t { kInvalidFrame };

class FrameCodec {
 public:
  [[nodiscard]] static std::variant<ProtocolFrame, DecodeError> decode(
      const RawCanFrame& raw);
  [[nodiscard]] static std::variant<RawCanFrame, EncodeError> encode(
      const ProtocolFrame& frame);
};

}  // namespace hacan::protocol
