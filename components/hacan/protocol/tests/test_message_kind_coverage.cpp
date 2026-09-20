#include <doctest/doctest.h>

#include "protocol_frame_codec.h"

namespace hacan::protocol {
namespace {

TEST_CASE("frame codec accepts one valid vector for every version one message kind") {
  struct Vector {
    MessageKind kind;
    Priority priority;
    NodeAddress destination;
    NodeAddress source;
    std::array<std::uint8_t, 8> bytes;
  };
  const std::array<Vector, 22> vectors{{
      {MessageKind::kProtocolError, Priority::kControl, NodeAddress{1}, NodeAddress{2}, {0, 0, 10, 0, 0, 0, 0, 0}},
      {MessageKind::kAnnounce, Priority::kManagement, NodeAddress{0x1FF}, NodeAddress{1}, {0, 1, 0, 0, 1, 0, 0, 0}},
      {MessageKind::kHeartbeat, Priority::kManagement, NodeAddress{0x1FF}, NodeAddress{1}, {0, 0, 1, 0, 0, 0, 0, 0}},
      {MessageKind::kDiscoveryRequest, Priority::kManagement, NodeAddress{0x1FF}, NodeAddress{1}, {0, 0, 0xE8, 3, 0, 0, 0, 0}},
      {MessageKind::kDiscoveryResponse, Priority::kManagement, NodeAddress{1}, NodeAddress{0}, {0, 1, 2, 3, 4, 5, 6, 0}},
      {MessageKind::kAddressClaim, Priority::kManagement, NodeAddress{0x1FF}, NodeAddress{1}, {1, 2, 3, 4, 5, 6, 1, 0}},
      {MessageKind::kAddressAssign, Priority::kManagement, NodeAddress{0x1FF}, NodeAddress{1}, {1, 2, 3, 4, 5, 6, 2, 2}},
      {MessageKind::kAddressConflict, Priority::kControl, NodeAddress{1}, NodeAddress{2}, {1, 2, 3, 4, 5, 6, 1, 0}},
      {MessageKind::kState, Priority::kStateQuery, NodeAddress{1}, NodeAddress{2}, {0, 1, 1, 1, 1, 0, 0, 0}},
      {MessageKind::kEvent, Priority::kEvent, NodeAddress{1}, NodeAddress{2}, {0, 1, 1, 0, 1, 0, 0, 0}},
      {MessageKind::kCommand, Priority::kControl, NodeAddress{1}, NodeAddress{2}, {0, 1, 1, 0, 1, 0, 0, 0}},
      {MessageKind::kRequest, Priority::kStateQuery, NodeAddress{1}, NodeAddress{2}, {0, 0, 2, 0, 0, 0, 0, 0}},
      {MessageKind::kResponse, Priority::kStateQuery, NodeAddress{1}, NodeAddress{2}, {0, 0, 2, 0, 0, 0, 0, 0}},
      {MessageKind::kAck, Priority::kControl, NodeAddress{1}, NodeAddress{2}, {0, 1, 10, 0, 0, 0, 0, 0}},
      {MessageKind::kConfigGet, Priority::kManagement, NodeAddress{1}, NodeAddress{2}, {0, 0, 0, 0, 0, 0, 0, 0}},
      {MessageKind::kConfigSet, Priority::kManagement, NodeAddress{1}, NodeAddress{2}, {0, 0, 0, 1, 1, 0, 0, 0}},
      {MessageKind::kConfigValue, Priority::kManagement, NodeAddress{1}, NodeAddress{2}, {0, 0, 0, 1, 1, 0, 0, 0}},
      {MessageKind::kDiagnosticRequest, Priority::kDiagnostic, NodeAddress{1}, NodeAddress{2}, {0, 0, 0, 0, 0, 0, 0, 0}},
      {MessageKind::kDiagnosticResponse, Priority::kDiagnostic, NodeAddress{1}, NodeAddress{2}, {0, 0, 0, 0, 0, 0, 0, 0}},
      {MessageKind::kTimeSync, Priority::kManagement, NodeAddress{0x1FF}, NodeAddress{1}, {0, 0, 0, 0, 0, 0, 0, 0}},
      {MessageKind::kEntityClaim, Priority::kManagement, NodeAddress{0x1FF}, NodeAddress{1}, {0, 1, 1, 0, 0, 0, 0, 0}},
      {MessageKind::kEntityResolve, Priority::kManagement, NodeAddress{0x1FF}, NodeAddress{1}, {0, 0, 1, 0, 0, 0, 0xE8, 3}},
  }};
  for (const auto& vector : vectors) {
    const auto identifier = CanIdentifier::create(vector.priority, vector.kind,
                                                  vector.destination, vector.source, 0);
    REQUIRE(identifier);
    const auto decoded = FrameCodec::decode({identifier->to_raw(), true, 8, vector.bytes});
    INFO("kind=" << static_cast<int>(vector.kind));
    REQUIRE(std::holds_alternative<ProtocolFrame>(decoded));
    const auto encoded = FrameCodec::encode(std::get<ProtocolFrame>(decoded));
    REQUIRE(std::holds_alternative<RawCanFrame>(encoded));
    CHECK(std::get<RawCanFrame>(encoded).data == vector.bytes);
  }
}

}  // namespace
}  // namespace hacan::protocol
