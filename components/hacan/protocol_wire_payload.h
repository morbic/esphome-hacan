#pragma once

#include <array>
#include <cstdint>
#include <optional>

#include "protocol_can_identifier.h"
#include "protocol_typed_value.h"

namespace hacan::protocol {

enum class FrameValidationError : std::uint8_t {
  kNone,
  kInvalidPriority,
  kInvalidAddressing,
  kInvalidPayload,
};

// A fixed-format payload value. Every MessageKind instantiates this type
// separately, so a ProtocolFrame always retains its concrete wire type.
template <MessageKind Kind>
class WirePayload {
 public:
  [[nodiscard]] static constexpr MessageKind kind() { return Kind; }
  [[nodiscard]] static std::optional<WirePayload> decode(
      const std::array<std::uint8_t, 8>& bytes) {
    return WirePayload{bytes};
  }
  [[nodiscard]] constexpr const std::array<std::uint8_t, 8>& bytes() const {
    return bytes_;
  }
  [[nodiscard]] constexpr std::array<std::uint8_t, 8> encode() const {
    return bytes_;
  }
  [[nodiscard]] FrameValidationError validate(
      const CanIdentifier& identifier) const;

 private:
  explicit constexpr WirePayload(std::array<std::uint8_t, 8> bytes)
      : bytes_(bytes) {}
  std::array<std::uint8_t, 8> bytes_;
};

using ProtocolErrorFrame = WirePayload<MessageKind::kProtocolError>;
using AnnounceFrame = WirePayload<MessageKind::kAnnounce>;
using HeartbeatFrame = WirePayload<MessageKind::kHeartbeat>;
using DiscoveryRequestFrame = WirePayload<MessageKind::kDiscoveryRequest>;
using DiscoveryResponseFrame = WirePayload<MessageKind::kDiscoveryResponse>;
using AddressClaimFrame = WirePayload<MessageKind::kAddressClaim>;
using AddressAssignFrame = WirePayload<MessageKind::kAddressAssign>;
using AddressConflictFrame = WirePayload<MessageKind::kAddressConflict>;
using StateFrame = WirePayload<MessageKind::kState>;
using EventFrame = WirePayload<MessageKind::kEvent>;
using CommandFrame = WirePayload<MessageKind::kCommand>;
using RequestFrame = WirePayload<MessageKind::kRequest>;
using ResponseFrame = WirePayload<MessageKind::kResponse>;
using AckFrame = WirePayload<MessageKind::kAck>;
using ConfigGetFrame = WirePayload<MessageKind::kConfigGet>;
using ConfigSetFrame = WirePayload<MessageKind::kConfigSet>;
using ConfigValueFrame = WirePayload<MessageKind::kConfigValue>;
using DiagnosticRequestFrame = WirePayload<MessageKind::kDiagnosticRequest>;
using DiagnosticResponseFrame = WirePayload<MessageKind::kDiagnosticResponse>;
using TimeSyncFrame = WirePayload<MessageKind::kTimeSync>;
using EntityClaimFrame = WirePayload<MessageKind::kEntityClaim>;
using EntityResolveFrame = WirePayload<MessageKind::kEntityResolve>;

namespace detail {

constexpr bool is_endpoint(std::uint8_t value) { return value > 0 && value < 0xFF; }
constexpr bool is_status(std::uint8_t value) { return value <= 7; }
constexpr bool is_kind(std::uint8_t value) { return value <= 0x15; }
constexpr bool is_nonzero_u32(const std::array<std::uint8_t, 8>& bytes,
                              std::uint8_t offset) {
  return bytes[offset] != 0 || bytes[offset + 1] != 0 ||
         bytes[offset + 2] != 0 || bytes[offset + 3] != 0;
}
constexpr std::uint16_t u16(const std::array<std::uint8_t, 8>& bytes,
                            std::uint8_t offset) {
  return static_cast<std::uint16_t>(bytes[offset]) |
         (static_cast<std::uint16_t>(bytes[offset + 1]) << 8U);
}

inline bool valid_typed_value(const std::array<std::uint8_t, 8>& bytes,
                              std::uint8_t type_offset,
                              std::uint8_t value_offset) {
  std::array<std::uint8_t, 4> value{};
  for (std::uint8_t index = 0; index < value.size(); ++index) {
    value[index] = bytes[value_offset + index];
  }
  return TypedValue{static_cast<DataType>(bytes[type_offset]), value}.validate() ==
         ValueError::kNone;
}

template <MessageKind Kind>
constexpr Priority priority() {
  if constexpr (Kind == MessageKind::kProtocolError ||
                Kind == MessageKind::kAddressConflict ||
                Kind == MessageKind::kCommand || Kind == MessageKind::kAck) {
    return Priority::kControl;
  } else if constexpr (Kind == MessageKind::kEvent) {
    return Priority::kEvent;
  } else if constexpr (static_cast<std::uint8_t>(Kind) >=
                           static_cast<std::uint8_t>(MessageKind::kState) &&
                       static_cast<std::uint8_t>(Kind) <=
                           static_cast<std::uint8_t>(MessageKind::kResponse)) {
    return Priority::kStateQuery;
  } else if constexpr (Kind == MessageKind::kDiagnosticRequest ||
                       Kind == MessageKind::kDiagnosticResponse) {
    return Priority::kDiagnostic;
  }
  return Priority::kManagement;
}

template <MessageKind Kind>
bool valid_identifier(const CanIdentifier& identifier) {
  const auto source = identifier.source().value();
  const auto destination = identifier.destination().value();
  const bool broadcast = destination == 0x1FF;
  if (identifier.priority() != priority<Kind>()) return false;
  if (source == 0x1FF || destination == 0) return false;
  if constexpr (Kind == MessageKind::kDiscoveryResponse) return !broadcast;
  if constexpr (Kind == MessageKind::kDiscoveryRequest) return broadcast;
  if (source == 0) return false;
  if constexpr (Kind == MessageKind::kProtocolError ||
                Kind == MessageKind::kAddressConflict || Kind == MessageKind::kCommand ||
                Kind == MessageKind::kRequest || Kind == MessageKind::kResponse ||
                Kind == MessageKind::kAck || Kind == MessageKind::kConfigGet ||
                Kind == MessageKind::kConfigSet || Kind == MessageKind::kConfigValue ||
                Kind == MessageKind::kDiagnosticRequest ||
                Kind == MessageKind::kDiagnosticResponse) return !broadcast;
  if constexpr (Kind == MessageKind::kState || Kind == MessageKind::kEvent) return true;
  return broadcast;
}

template <MessageKind Kind>
bool valid_bytes(const std::array<std::uint8_t, 8>& b) {
  if constexpr (Kind == MessageKind::kProtocolError) {
    return is_kind(b[2]) && is_status(b[3]);
  } else if constexpr (Kind == MessageKind::kAnnounce) {
    return b[1] == 1 && (b[3] & 0xE0U) == 0 && is_nonzero_u32(b, 4);
  } else if constexpr (Kind == MessageKind::kHeartbeat) {
    return (b[1] & 0xE0U) == 0 && is_nonzero_u32(b, 2) &&
           (b[6] <= 100 || b[6] == 0xFF) && b[7] == 0;
  } else if constexpr (Kind == MessageKind::kDiscoveryRequest) {
    const auto window = u16(b, 2);
    return (b[1] & 0xF8U) == 0 && window >= 1000 && window <= 10000;
  } else if constexpr (Kind == MessageKind::kDiscoveryResponse) {
    return true;
  } else if constexpr (Kind == MessageKind::kAddressClaim) {
    return b[6] == 1;
  } else if constexpr (Kind == MessageKind::kAddressAssign) {
    const auto address = static_cast<std::uint16_t>(b[6]) |
                         (static_cast<std::uint16_t>(b[7] & 1U) << 8U);
    return address > 0 && address < 0x1FF && (b[7] & 0xFCU) == 0 &&
           (b[7] & 0x02U) != 0;
  } else if constexpr (Kind == MessageKind::kAddressConflict) {
    return b[6] == 1;
  } else if constexpr (Kind == MessageKind::kState) {
    return is_endpoint(b[1]) && (b[3] & 0xE0U) == 0 && valid_typed_value(b, 2, 4);
  } else if constexpr (Kind == MessageKind::kEvent || Kind == MessageKind::kCommand) {
    return is_endpoint(b[1]) && (b[3] & 0xFCU) == 0 && valid_typed_value(b, 2, 4);
  } else if constexpr (Kind == MessageKind::kRequest) {
    if (b[2] < 1 || b[2] > 3 || b[4] || b[5] || b[6] || b[7]) return false;
    return (b[2] == 1 && (is_endpoint(b[1]) || b[1] == 0xFF)) ||
           (b[2] == 2 && b[1] == 0) || (b[2] == 3 && is_endpoint(b[1]));
  } else if constexpr (Kind == MessageKind::kResponse) {
    return is_status(b[3]);
  } else if constexpr (Kind == MessageKind::kAck) {
    return is_kind(b[2]) && is_status(b[3]);
  } else if constexpr (Kind == MessageKind::kConfigGet) {
    return b[1] != 0xFF && b[3] == 0 && b[4] == 0 && b[5] == 0 && b[6] == 0 && b[7] == 0;
  } else if constexpr (Kind == MessageKind::kConfigSet || Kind == MessageKind::kConfigValue) {
    return b[1] != 0xFF && valid_typed_value(b, 3, 4);
  } else if constexpr (Kind == MessageKind::kDiagnosticRequest) {
    return b[1] != 0xFF && b[4] == 0 && b[5] == 0 && b[6] == 0 && b[7] == 0;
  } else if constexpr (Kind == MessageKind::kDiagnosticResponse) {
    return b[1] != 0xFF && is_status(b[4]);
  } else if constexpr (Kind == MessageKind::kTimeSync) {
    return b[1] <= 3 && u16(b, 6) < 1000;
  } else if constexpr (Kind == MessageKind::kEntityClaim) {
    return is_endpoint(b[1]) && is_nonzero_u32(b, 2) && b[6] == 0 && b[7] == 0;
  } else if constexpr (Kind == MessageKind::kEntityResolve) {
    const auto window = u16(b, 6);
    return b[1] == 0 && is_nonzero_u32(b, 2) && window >= 1000 && window <= 10000;
  }
  return false;
}
}  // namespace detail

template <MessageKind Kind>
FrameValidationError WirePayload<Kind>::validate(const CanIdentifier& identifier) const {
  if (identifier.kind() != Kind) return FrameValidationError::kInvalidPayload;
  if (identifier.priority() != detail::priority<Kind>()) {
    return FrameValidationError::kInvalidPriority;
  }
  if (!detail::valid_identifier<Kind>(identifier)) {
    return FrameValidationError::kInvalidAddressing;
  }
  return detail::valid_bytes<Kind>(bytes_) ? FrameValidationError::kNone
                                             : FrameValidationError::kInvalidPayload;
}

}  // namespace hacan::protocol
