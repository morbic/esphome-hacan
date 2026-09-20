#pragma once

#include <array>
#include <variant>

#include "protocol_can_identifier.h"
#include "protocol_wire_payload.h"

namespace hacan::protocol {

using PayloadVariant = std::variant<
    ProtocolErrorFrame, AnnounceFrame, HeartbeatFrame, DiscoveryRequestFrame,
    DiscoveryResponseFrame, AddressClaimFrame, AddressAssignFrame,
    AddressConflictFrame, StateFrame, EventFrame, CommandFrame, RequestFrame,
    ResponseFrame, AckFrame, ConfigGetFrame, ConfigSetFrame, ConfigValueFrame,
    DiagnosticRequestFrame, DiagnosticResponseFrame, TimeSyncFrame,
    EntityClaimFrame, EntityResolveFrame>;

class ProtocolFrame {
 public:
  constexpr ProtocolFrame(CanIdentifier identifier, PayloadVariant payload)
      : identifier_(identifier), payload_(payload) {}

  [[nodiscard]] constexpr const CanIdentifier& identifier() const { return identifier_; }
  [[nodiscard]] constexpr const PayloadVariant& payload() const {
    return payload_;
  }

 private:
  CanIdentifier identifier_;
  PayloadVariant payload_;
};

}  // namespace hacan::protocol
