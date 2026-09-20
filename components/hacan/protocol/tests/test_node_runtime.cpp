#include <doctest/doctest.h>

#include "protocol_node_runtime.h"

namespace hacan::protocol {
namespace {

class MemoryStorage final : public IAddressStorage {
 public:
  std::optional<NodeAddress> load() override { return value; }
  bool save(NodeAddress address) override { value = address; return true; }
  std::optional<NodeAddress> value;
};

class CapturingTransmitter final : public IFrameTransmitter {
 public:
  bool transmit(const RawCanFrame& frame) override { last = frame; ++count; return true; }
  RawCanFrame last{};
  std::uint8_t count{0};
};

class CapturingEvents final : public INodeEvents {
 public:
  void entity_available(EntityId, NodeAddress, EndpointId) override { ++available; }
  void entity_unavailable(EntityId) override { ++unavailable; }
  void address_conflict(NodeAddress) override { ++conflicts; }
  std::uint8_t available{0};
  std::uint8_t unavailable{0};
  std::uint8_t conflicts{0};
};

TEST_CASE("primary manager assigns an address to a discovered unassigned node") {
  MemoryStorage storage;
  storage.value = NodeAddress{0x001};
  CapturingTransmitter transmitter;
  NodeRuntime runtime{{1, 2, 3, 4, 5, 6}, CommissioningRole::kPrimary,
                      storage, transmitter};

  const auto identifier = CanIdentifier::create(Priority::kManagement,
      MessageKind::kDiscoveryResponse, NodeAddress{0x001}, NodeAddress{0}, 0);
  REQUIRE(identifier);
  const RawCanFrame response{identifier->to_raw(), true, 8,
      {1, 8, 7, 6, 5, 4, 3, 0}};

  runtime.receive(response, 100);
  REQUIRE(transmitter.count == 1);
  const auto assigned = FrameCodec::decode(transmitter.last);
  REQUIRE(std::holds_alternative<ProtocolFrame>(assigned));
  CHECK(std::get<ProtocolFrame>(assigned).identifier().kind() == MessageKind::kAddressAssign);
}

TEST_CASE("unassigned node starts discovery with source address zero") {
  MemoryStorage storage;
  CapturingTransmitter transmitter;
  NodeRuntime runtime{{1, 2, 3, 4, 5, 6}, CommissioningRole::kNone,
                      storage, transmitter};
  runtime.tick(0);
  REQUIRE(transmitter.count == 1);
  const auto decoded = FrameCodec::decode(transmitter.last);
  REQUIRE(std::holds_alternative<ProtocolFrame>(decoded));
  const auto& frame = std::get<ProtocolFrame>(decoded);
  CHECK(frame.identifier().kind() == MessageKind::kDiscoveryRequest);
  CHECK(frame.identifier().source().value() == 0);
}

TEST_CASE("runtime expires an entity mapping when its owner is offline") {
  MemoryStorage storage;
  storage.value = NodeAddress{1};
  CapturingTransmitter transmitter;
  CapturingEvents events;
  NodeRuntime runtime{{1, 2, 3, 4, 5, 6}, CommissioningRole::kNone,
                      storage, transmitter, &events};
  const auto identifier = CanIdentifier::create(Priority::kManagement,
      MessageKind::kEntityClaim, NodeAddress{0x1FF}, NodeAddress{2}, 0);
  REQUIRE(identifier);
  runtime.receive({identifier->to_raw(), true, 8, {1, 3, 0x34, 0x12, 0, 0, 0, 0}}, 10);
  REQUIRE(runtime.resolve(EntityId{0x1234}));
  runtime.tick(105010);
  CHECK_FALSE(runtime.resolve(EntityId{0x1234}));
  CHECK(events.unavailable == 1);
}

TEST_CASE("runtime sends a heartbeat for a commissioned node") {
  MemoryStorage storage;
  storage.value = NodeAddress{1};
  CapturingTransmitter transmitter;
  NodeRuntime runtime{{1, 2, 3, 4, 5, 6}, CommissioningRole::kNone,
                      storage, transmitter};
  runtime.tick(0);
  runtime.tick(375);
  runtime.tick(750);
  REQUIRE(transmitter.count == 3);
  const auto decoded = FrameCodec::decode(transmitter.last);
  REQUIRE(std::holds_alternative<ProtocolFrame>(decoded));
  CHECK(std::get<ProtocolFrame>(decoded).identifier().kind() == MessageKind::kHeartbeat);
}

TEST_CASE("runtime answers entity resolution with a fresh claim") {
  MemoryStorage storage;
  storage.value = NodeAddress{1};
  CapturingTransmitter transmitter;
  NodeRuntime runtime{{1, 2, 3, 4, 5, 6}, CommissioningRole::kNone, storage, transmitter};
  REQUIRE(runtime.register_owned_entity(EntityId{0x1234}, EndpointId{3}));
  transmitter.count = 0;
  const auto id = CanIdentifier::create(Priority::kManagement, MessageKind::kEntityResolve,
                                        NodeAddress{0x1FF}, NodeAddress{2}, 0);
  REQUIRE(id);
  runtime.receive({id->to_raw(), true, 8, {1, 0, 0x34, 0x12, 0, 0, 0xE8, 3}}, 10);
  runtime.drain_one();
  REQUIRE(transmitter.count == 1);
  const auto decoded = FrameCodec::decode(transmitter.last);
  REQUIRE(std::holds_alternative<ProtocolFrame>(decoded));
  CHECK(std::get<ProtocolFrame>(decoded).identifier().kind() == MessageKind::kEntityClaim);
}

TEST_CASE("runtime drains control traffic before management traffic") {
  MemoryStorage storage;
  storage.value = NodeAddress{1};
  CapturingTransmitter transmitter;
  NodeRuntime runtime{{1, 2, 3, 4, 5, 6}, CommissioningRole::kNone, storage, transmitter};
  const auto management_id = CanIdentifier::create(Priority::kManagement, MessageKind::kHeartbeat,
                                                    NodeAddress{0x1FF}, NodeAddress{1}, 0);
  const auto control_id = CanIdentifier::create(Priority::kControl, MessageKind::kAck,
                                                 NodeAddress{2}, NodeAddress{1}, 0);
  REQUIRE(management_id); REQUIRE(control_id);
  const auto heartbeat = HeartbeatFrame::decode({0, 0, 1, 0, 0, 0, 0, 0});
  const auto ack = AckFrame::decode({0, 1, 10, 0, 0, 0, 0, 0});
  REQUIRE(heartbeat); REQUIRE(ack);
  REQUIRE(runtime.enqueue(ProtocolFrame{*management_id, *heartbeat}));
  REQUIRE(runtime.enqueue(ProtocolFrame{*control_id, *ack}));
  REQUIRE(runtime.drain_one());
  const auto decoded = FrameCodec::decode(transmitter.last);
  REQUIRE(std::holds_alternative<ProtocolFrame>(decoded));
  CHECK(std::get<ProtocolFrame>(decoded).identifier().kind() == MessageKind::kAck);
}

TEST_CASE("address conflict withdraws the higher uid claimant") {
  MemoryStorage storage;
  storage.value = NodeAddress{3};
  CapturingTransmitter transmitter;
  CapturingEvents events;
  NodeRuntime runtime{{2, 0, 0, 0, 0, 0}, CommissioningRole::kNone, storage, transmitter, &events};
  const auto id = CanIdentifier::create(Priority::kControl, MessageKind::kAddressConflict,
                                        NodeAddress{3}, NodeAddress{1}, 0);
  REQUIRE(id);
  runtime.receive({id->to_raw(), true, 8, {1, 0, 0, 0, 0, 0, 1, 0}}, 10);
  CHECK_FALSE(runtime.address());
  CHECK(events.conflicts == 1);
}

TEST_CASE("newer pending state replaces an older state for the same endpoint") {
  MemoryStorage storage;
  storage.value = NodeAddress{1};
  CapturingTransmitter transmitter;
  NodeRuntime runtime{{1, 2, 3, 4, 5, 6}, CommissioningRole::kNone, storage, transmitter};
  const auto id = CanIdentifier::create(Priority::kStateQuery, MessageKind::kState,
                                        NodeAddress{2}, NodeAddress{1}, 0);
  REQUIRE(id);
  const auto old_state = StateFrame::decode({1, 3, 1, 1, 0, 0, 0, 0});
  const auto new_state = StateFrame::decode({2, 3, 1, 1, 1, 0, 0, 0});
  REQUIRE(old_state); REQUIRE(new_state);
  REQUIRE(runtime.enqueue(ProtocolFrame{*id, *old_state}));
  REQUIRE(runtime.enqueue(ProtocolFrame{*id, *new_state}));
  REQUIRE(runtime.drain_one());
  const auto decoded = FrameCodec::decode(transmitter.last);
  REQUIRE(std::holds_alternative<ProtocolFrame>(decoded));
  CHECK(std::get<StateFrame>(std::get<ProtocolFrame>(decoded).payload()).bytes()[0] == 2);
}

TEST_CASE("duplicate acknowledged command receives a repeated acknowledgement") {
  MemoryStorage storage;
  storage.value = NodeAddress{1};
  CapturingTransmitter transmitter;
  NodeRuntime runtime{{1, 2, 3, 4, 5, 6}, CommissioningRole::kNone, storage, transmitter};
  const auto id = CanIdentifier::create(Priority::kControl, MessageKind::kCommand,
                                        NodeAddress{1}, NodeAddress{2}, 0);
  REQUIRE(id);
  const RawCanFrame command{id->to_raw(), true, 8, {9, 3, 1, 1, 1, 0, 0, 0}};
  runtime.receive(command, 10);
  REQUIRE(runtime.drain_one());
  runtime.receive(command, 11);
  REQUIRE(runtime.drain_one());
  const auto decoded = FrameCodec::decode(transmitter.last);
  REQUIRE(std::holds_alternative<ProtocolFrame>(decoded));
  CHECK(std::get<ProtocolFrame>(decoded).identifier().kind() == MessageKind::kAck);
}

}  // namespace
}  // namespace hacan::protocol
