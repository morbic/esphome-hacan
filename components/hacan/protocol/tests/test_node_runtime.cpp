#include <doctest/doctest.h>

#include "protocol_node_runtime.h"
#include "protocol_payload_command.h"

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
  bool transmit(const RawCanFrame& frame) override {
    last = frame;
    frames[count++] = frame;
    return true;
  }
  RawCanFrame last{};
  std::array<RawCanFrame, 32> frames{};
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

class CapturingEndpoint final : public IEndpointHandler {
 public:
  explicit CapturingEndpoint(EndpointId endpoint) : endpoint_(endpoint) {}

  EndpointId endpoint() const override { return endpoint_; }
  Status command(TypedValue value) override {
    ++commands;
    last_value = value;
    return result;
  }

  EndpointId endpoint_;
  Status result{Status::kAccepted};
  std::uint8_t commands{0};
  TypedValue last_value{DataType::kNull, {0, 0, 0, 0}};
};

class CapturingStateListener final : public IStateListener {
 public:
  explicit CapturingStateListener(EntityId entity) : entity_(entity) {}

  EntityId observed_entity() const override { return entity_; }
  void state(TypedValue value, StateQuality quality) override {
    ++updates;
    last_value = value;
    last_quality = quality;
  }

  EntityId entity_;
  std::uint8_t updates{0};
  TypedValue last_value{DataType::kNull, {0, 0, 0, 0}};
  StateQuality last_quality{StateQuality::kValid};
};

class CapturingEventListener final : public IEventListener {
 public:
  explicit CapturingEventListener(EntityId entity) : entity_(entity) {}

  EntityId observed_entity() const override { return entity_; }
  void event(TypedValue value, std::uint8_t flags) override {
    ++updates;
    last_value = value;
    last_flags = flags;
  }

  EntityId entity_;
  std::uint8_t updates{0};
  TypedValue last_value{DataType::kNull, {0, 0, 0, 0}};
  std::uint8_t last_flags{0};
};

TEST_CASE("primary manager assigns an address to a discovered unassigned node") {
  MemoryStorage storage;
  storage.value = NodeAddress{0x001};
  CapturingTransmitter transmitter;
  NodeRuntime runtime{{1, 2, 3, 4, 5, 6}, CommissioningRole::kPrimary,
                      storage, transmitter};

  const auto identifier = CanIdentifier::create(Priority::kManagement,
      MessageKind::kDiscoveryResponse, NodeAddress{0}, NodeAddress{0}, 0);
  REQUIRE(identifier);
  const RawCanFrame response{identifier->to_raw(), true, 8,
      {1, 8, 7, 6, 5, 4, 3, 0}};

  runtime.receive(response, 100);
  REQUIRE(transmitter.count == 1);
  const auto assigned = FrameCodec::decode(transmitter.last);
  REQUIRE(std::holds_alternative<ProtocolFrame>(assigned));
  CHECK(std::get<ProtocolFrame>(assigned).identifier().kind() == MessageKind::kAddressAssign);
}

TEST_CASE("fresh primary persists and claims the reserved bootstrap address") {
  MemoryStorage storage;
  CapturingTransmitter transmitter;
  NodeRuntime runtime{{1, 2, 3, 4, 5, 6}, CommissioningRole::kPrimary,
                      storage, transmitter};

  REQUIRE(storage.value);
  CHECK(storage.value->value() == 0x001);
  REQUIRE(runtime.address());
  CHECK(runtime.address()->value() == 0x001);
}

TEST_CASE("commissioned node delays its first address claim") {
  MemoryStorage storage;
  storage.value = NodeAddress{1};
  CapturingTransmitter transmitter;
  NodeRuntime runtime{{1, 2, 3, 4, 5, 6}, CommissioningRole::kNone,
                      storage, transmitter};

  runtime.tick(0);
  CHECK(transmitter.count == 0);
  for (std::uint32_t now = 1; now <= 2000 && transmitter.count == 0; ++now) {
    runtime.tick(now);
  }

  REQUIRE(transmitter.count == 1);
  const auto decoded = FrameCodec::decode(transmitter.last);
  REQUIRE(std::holds_alternative<ProtocolFrame>(decoded));
  CHECK(std::get<ProtocolFrame>(decoded).identifier().kind() == MessageKind::kAddressClaim);
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

TEST_CASE("unassigned node responds to its own discovery request") {
  MemoryStorage storage;
  CapturingTransmitter transmitter;
  NodeRuntime runtime{{1, 2, 3, 4, 5, 6}, CommissioningRole::kNone,
                      storage, transmitter};

  runtime.tick(0);
  REQUIRE(transmitter.count == 1);
  for (std::uint32_t now = 1; now <= 1000 && transmitter.count == 1; ++now) {
    runtime.tick(now);
  }

  REQUIRE(transmitter.count == 2);
  const auto decoded = FrameCodec::decode(transmitter.last);
  REQUIRE(std::holds_alternative<ProtocolFrame>(decoded));
  const auto& frame = std::get<ProtocolFrame>(decoded);
  CHECK(frame.identifier().kind() == MessageKind::kDiscoveryResponse);
  CHECK(frame.identifier().source().value() == 0);
  CHECK(frame.identifier().destination().value() == 0);
  const auto& bytes = std::get<DiscoveryResponseFrame>(frame.payload()).bytes();
  CHECK(bytes[1] == 1);
  CHECK(bytes[6] == 6);
}

TEST_CASE("newly commissioned owner advertises each configured entity") {
  MemoryStorage storage;
  CapturingTransmitter transmitter;
  NodeRuntime runtime{{1, 2, 3, 4, 5, 6}, CommissioningRole::kNone,
                      storage, transmitter};
  REQUIRE(runtime.register_owned_entity(EntityId{0x01010001}, EndpointId{1}));
  const auto assign_id = CanIdentifier::create(Priority::kManagement,
                                                MessageKind::kAddressAssign,
                                                NodeAddress{0x1FF}, NodeAddress{1}, 0);
  REQUIRE(assign_id);
  runtime.receive({assign_id->to_raw(), true, 8, {1, 2, 3, 4, 5, 6, 2, 2}}, 0);

  bool claimed = false;
  for (std::uint32_t now = 0; now <= 5000 && !claimed; ++now) {
    runtime.tick(now);
    const auto decoded = FrameCodec::decode(transmitter.last);
    if (const auto* frame = std::get_if<ProtocolFrame>(&decoded);
        frame && frame->identifier().kind() == MessageKind::kEntityClaim) {
      CHECK(frame->identifier().source().value() == 2);
      claimed = true;
    }
  }
  CHECK(claimed);
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
  bool heartbeat = false;
  for (std::uint32_t now = 0; now <= 5000 && !heartbeat; ++now) {
    runtime.tick(now);
    const auto decoded = FrameCodec::decode(transmitter.last);
    if (const auto* frame = std::get_if<ProtocolFrame>(&decoded)) {
      heartbeat = frame->identifier().kind() == MessageKind::kHeartbeat;
    }
  }
  CHECK(heartbeat);
}

TEST_CASE("runtime publishes an owned boolean state") {
  MemoryStorage storage;
  storage.value = NodeAddress{1};
  CapturingTransmitter transmitter;
  NodeRuntime runtime{{1, 2, 3, 4, 5, 6}, CommissioningRole::kNone, storage, transmitter};
  REQUIRE(runtime.register_owned_entity(EntityId{0x01010001}, EndpointId{3}));

  CHECK(runtime.publish_state(EndpointId{3}, TypedValue{DataType::kBool, {1, 0, 0, 0}}));
  REQUIRE(runtime.drain_one());

  const auto decoded = FrameCodec::decode(transmitter.last);
  REQUIRE(std::holds_alternative<ProtocolFrame>(decoded));
  const auto& frame = std::get<ProtocolFrame>(decoded);
  CHECK(frame.identifier().kind() == MessageKind::kState);
  CHECK(frame.identifier().source().value() == 1);
  const auto& bytes = std::get<StateFrame>(frame.payload()).bytes();
  CHECK(bytes[1] == 3);
  CHECK(bytes[2] == static_cast<std::uint8_t>(DataType::kBool));
  CHECK(bytes[4] == 1);
}

TEST_CASE("runtime delivers an observed state to every listener for its entity") {
  MemoryStorage storage;
  storage.value = NodeAddress{1};
  CapturingTransmitter transmitter;
  NodeRuntime runtime{{1, 2, 3, 4, 5, 6}, CommissioningRole::kNone, storage, transmitter};
  CapturingStateListener first{EntityId{0x01010001}};
  CapturingStateListener second{EntityId{0x01010001}};
  REQUIRE(runtime.register_state_listener(first));
  REQUIRE(runtime.register_state_listener(second));

  const auto claim_id = CanIdentifier::create(Priority::kManagement, MessageKind::kEntityClaim,
                                              NodeAddress{0x1FF}, NodeAddress{2}, 0);
  REQUIRE(claim_id);
  runtime.receive({claim_id->to_raw(), true, 8, {1, 3, 1, 0, 1, 1, 0, 0}}, 100);

  const auto state_id = CanIdentifier::create(Priority::kStateQuery, MessageKind::kState,
                                              NodeAddress{0x1FF}, NodeAddress{2}, 0);
  REQUIRE(state_id);
  runtime.receive({state_id->to_raw(), true, 8,
                   {2, 3, static_cast<std::uint8_t>(DataType::kBool),
                    static_cast<std::uint8_t>(StateQuality::kValid), 1, 0, 0, 0}},
                  101);

  CHECK(first.updates == 1);
  CHECK(second.updates == 1);
  CHECK(first.last_value.type() == DataType::kBool);
  CHECK(first.last_value.bytes()[0] == 1);
}

TEST_CASE("runtime delivers a broadcast event to every listener for its entity") {
  MemoryStorage storage;
  storage.value = NodeAddress{1};
  CapturingTransmitter transmitter;
  NodeRuntime runtime{{1, 2, 3, 4, 5, 6}, CommissioningRole::kNone, storage, transmitter};
  CapturingEventListener first{EntityId{0x01010001}};
  CapturingEventListener second{EntityId{0x01010001}};
  REQUIRE(runtime.register_event_listener(first));
  REQUIRE(runtime.register_event_listener(second));

  const auto claim_id = CanIdentifier::create(Priority::kManagement, MessageKind::kEntityClaim,
                                              NodeAddress{0x1FF}, NodeAddress{2}, 0);
  REQUIRE(claim_id);
  runtime.receive({claim_id->to_raw(), true, 8, {1, 3, 1, 0, 1, 1, 0, 0}}, 100);

  const auto event_id = CanIdentifier::create(Priority::kEvent, MessageKind::kEvent,
                                              NodeAddress{0x1FF}, NodeAddress{2}, 0);
  REQUIRE(event_id);
  runtime.receive({event_id->to_raw(), true, 8,
                   {2, 3, static_cast<std::uint8_t>(DataType::kEnum8), 0, 3, 0, 0, 0}},
                  101);

  CHECK(first.updates == 1);
  CHECK(second.updates == 1);
  CHECK(first.last_value.type() == DataType::kEnum8);
  CHECK(first.last_value.bytes()[0] == 3);
  CHECK(first.last_flags == 0);
}

TEST_CASE("runtime publishes every owned event without coalescing") {
  MemoryStorage storage;
  storage.value = NodeAddress{1};
  CapturingTransmitter transmitter;
  NodeRuntime runtime{{1, 2, 3, 4, 5, 6}, CommissioningRole::kNone, storage, transmitter};
  REQUIRE(runtime.register_owned_entity(EntityId{0x01010001}, EndpointId{3}));
  const TypedValue click{DataType::kEnum8, {3, 0, 0, 0}};
  REQUIRE(runtime.publish_event(EndpointId{3}, click));
  REQUIRE(runtime.publish_event(EndpointId{3}, click));

  REQUIRE(runtime.drain_one());
  const auto first = FrameCodec::decode(transmitter.last);
  REQUIRE(std::holds_alternative<ProtocolFrame>(first));
  CHECK(std::get<ProtocolFrame>(first).identifier().kind() == MessageKind::kEvent);
  CHECK(std::get<ProtocolFrame>(first).identifier().priority() == Priority::kEvent);
  CHECK(std::get<ProtocolFrame>(first).identifier().destination().value() == 0x1FF);
  CHECK(std::get<EventFrame>(std::get<ProtocolFrame>(first).payload()).bytes()[1] == 3);

  REQUIRE(runtime.drain_one());
  const auto second = FrameCodec::decode(transmitter.last);
  REQUIRE(std::holds_alternative<ProtocolFrame>(second));
  CHECK(std::get<ProtocolFrame>(second).identifier().kind() == MessageKind::kEvent);
  CHECK(std::get<EventFrame>(std::get<ProtocolFrame>(second).payload()).bytes()[0] !=
        std::get<EventFrame>(std::get<ProtocolFrame>(first).payload()).bytes()[0]);
}

TEST_CASE("runtime dispatches a boolean command to a registered endpoint") {
  MemoryStorage storage;
  storage.value = NodeAddress{1};
  CapturingTransmitter transmitter;
  NodeRuntime runtime{{1, 2, 3, 4, 5, 6}, CommissioningRole::kNone, storage, transmitter};
  CapturingEndpoint endpoint{EndpointId{3}};
  REQUIRE(runtime.register_endpoint(endpoint));

  const auto identifier = CanIdentifier::create(Priority::kControl, MessageKind::kCommand,
                                                 NodeAddress{1}, NodeAddress{2}, 0);
  const auto payload = CommandPayload::create(0x42, EndpointId{3},
                                              TypedValue{DataType::kBool, {1, 0, 0, 0}}, 1);
  REQUIRE(identifier);
  REQUIRE(payload);
  const auto wire_payload = CommandFrame::decode(payload->encode());
  REQUIRE(wire_payload);
  const auto encoded = FrameCodec::encode(ProtocolFrame{*identifier, *wire_payload});
  REQUIRE(std::holds_alternative<RawCanFrame>(encoded));

  runtime.receive(std::get<RawCanFrame>(encoded), 100);

  REQUIRE(endpoint.commands == 1);
  CHECK(endpoint.last_value.type() == DataType::kBool);
  CHECK(endpoint.last_value.bytes()[0] == 1);
  REQUIRE(runtime.drain_one());
  const auto decoded = FrameCodec::decode(transmitter.last);
  REQUIRE(std::holds_alternative<ProtocolFrame>(decoded));
  const auto& ack = std::get<ProtocolFrame>(decoded);
  CHECK(ack.identifier().kind() == MessageKind::kAck);
  CHECK(std::get<AckFrame>(ack.payload()).bytes()[3] == static_cast<std::uint8_t>(Status::kAccepted));
}

TEST_CASE("runtime repeats the original acknowledgement for a duplicate command") {
  MemoryStorage storage;
  storage.value = NodeAddress{1};
  CapturingTransmitter transmitter;
  NodeRuntime runtime{{1, 2, 3, 4, 5, 6}, CommissioningRole::kNone, storage, transmitter};
  CapturingEndpoint endpoint{EndpointId{3}};
  REQUIRE(runtime.register_endpoint(endpoint));

  const auto identifier = CanIdentifier::create(Priority::kControl, MessageKind::kCommand,
                                                 NodeAddress{1}, NodeAddress{2}, 0);
  const auto payload = CommandPayload::create(0x42, EndpointId{3},
                                              TypedValue{DataType::kBool, {1, 0, 0, 0}}, 1);
  REQUIRE(identifier);
  REQUIRE(payload);
  const auto wire_payload = CommandFrame::decode(payload->encode());
  REQUIRE(wire_payload);
  const auto encoded = FrameCodec::encode(ProtocolFrame{*identifier, *wire_payload});
  REQUIRE(std::holds_alternative<RawCanFrame>(encoded));

  runtime.receive(std::get<RawCanFrame>(encoded), 100);
  REQUIRE(runtime.drain_one());
  runtime.receive(std::get<RawCanFrame>(encoded), 101);

  CHECK(endpoint.commands == 1);
  REQUIRE(runtime.drain_one());
  const auto decoded = FrameCodec::decode(transmitter.last);
  REQUIRE(std::holds_alternative<ProtocolFrame>(decoded));
  CHECK(std::get<AckFrame>(std::get<ProtocolFrame>(decoded).payload()).bytes()[3] ==
        static_cast<std::uint8_t>(Status::kAccepted));
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

TEST_CASE("runtime resolves every configured observed entity after commissioning") {
  MemoryStorage storage;
  storage.value = NodeAddress{1};
  CapturingTransmitter transmitter;
  NodeRuntime runtime{{1, 2, 3, 4, 5, 6}, CommissioningRole::kNone, storage, transmitter};
  REQUIRE(runtime.register_observed_entity(EntityId{0x01020001}));
  REQUIRE(runtime.register_observed_entity(EntityId{0x01020002}));

  std::array<std::uint8_t, 2> resolved{};
  std::uint8_t resolved_count = 0;
  for (std::uint32_t now = 0; now <= 5000 && resolved_count < resolved.size(); ++now) {
    runtime.tick(now);
    const auto decoded = FrameCodec::decode(transmitter.last);
    if (const auto* frame = std::get_if<ProtocolFrame>(&decoded);
        frame && frame->identifier().kind() == MessageKind::kEntityResolve) {
      resolved[resolved_count++] =
          std::get<EntityResolveFrame>(frame->payload()).bytes()[2];
    }
  }
  REQUIRE(resolved_count == 2);
  CHECK(resolved[0] == 0x01);
  CHECK(resolved[1] == 0x02);
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

TEST_CASE("address claim conflict retains the lower uid and broadcasts the winner") {
  MemoryStorage storage;
  storage.value = NodeAddress{3};
  CapturingTransmitter transmitter;
  NodeRuntime runtime{{1, 0, 0, 0, 0, 0}, CommissioningRole::kNone, storage, transmitter};
  const auto id = CanIdentifier::create(Priority::kManagement, MessageKind::kAddressClaim,
                                        NodeAddress{0x1FF}, NodeAddress{3}, 0);
  REQUIRE(id);

  runtime.receive({id->to_raw(), true, 8, {2, 0, 0, 0, 0, 0, 1, 0}}, 10);

  REQUIRE(runtime.address());
  CHECK(runtime.address()->value() == 3);
  REQUIRE(runtime.drain_one());
  const auto decoded = FrameCodec::decode(transmitter.last);
  REQUIRE(std::holds_alternative<ProtocolFrame>(decoded));
  CHECK(std::get<ProtocolFrame>(decoded).identifier().kind() == MessageKind::kAddressConflict);
}

TEST_CASE("address claim conflict withdraws the higher uid") {
  MemoryStorage storage;
  storage.value = NodeAddress{3};
  CapturingTransmitter transmitter;
  NodeRuntime runtime{{2, 0, 0, 0, 0, 0}, CommissioningRole::kNone, storage, transmitter};
  const auto id = CanIdentifier::create(Priority::kManagement, MessageKind::kAddressClaim,
                                        NodeAddress{0x1FF}, NodeAddress{3}, 0);
  REQUIRE(id);

  runtime.receive({id->to_raw(), true, 8, {1, 0, 0, 0, 0, 0, 1, 0}}, 10);

  CHECK_FALSE(runtime.address());
}

TEST_CASE("address claim conflict compares node uid as a little endian integer") {
  MemoryStorage storage;
  storage.value = NodeAddress{3};
  CapturingTransmitter transmitter;
  NodeRuntime runtime{{0, 0, 0, 0, 0, 1}, CommissioningRole::kNone, storage, transmitter};
  const auto id = CanIdentifier::create(Priority::kManagement, MessageKind::kAddressClaim,
                                        NodeAddress{0x1FF}, NodeAddress{3}, 0);
  REQUIRE(id);

  runtime.receive({id->to_raw(), true, 8, {0xFF, 0, 0, 0, 0, 0, 1, 0}}, 10);

  CHECK_FALSE(runtime.address());
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
