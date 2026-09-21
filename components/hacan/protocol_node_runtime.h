#pragma once

#include <array>
#include <cstdint>
#include <optional>

#include "protocol_frame_codec.h"
#include "protocol_payload_state.h"

namespace hacan::protocol {

using NodeUid = std::array<std::uint8_t, 6>;

enum class CommissioningRole : std::uint8_t { kPrimary, kSecondary, kNone };

class IFrameTransmitter {
 public:
  virtual ~IFrameTransmitter() = default;
  virtual bool transmit(const RawCanFrame& frame) = 0;
};

class IAddressStorage {
 public:
  virtual ~IAddressStorage() = default;
  virtual std::optional<NodeAddress> load() = 0;
  virtual bool save(NodeAddress address) = 0;
};

class INodeEvents {
 public:
  virtual ~INodeEvents() = default;
  virtual void entity_available(EntityId entity, NodeAddress node,
                                EndpointId endpoint) = 0;
  virtual void entity_unavailable(EntityId entity) = 0;
  virtual void address_conflict(NodeAddress address) = 0;
};

class IEndpointHandler {
 public:
  virtual ~IEndpointHandler() = default;
  [[nodiscard]] virtual EndpointId endpoint() const = 0;
  virtual Status command(TypedValue value) = 0;
};

class IStateListener {
 public:
  virtual ~IStateListener() = default;
  [[nodiscard]] virtual EntityId observed_entity() const = 0;
  virtual void state(TypedValue value, StateQuality quality) = 0;
};

class IEventListener {
 public:
  virtual ~IEventListener() = default;
  [[nodiscard]] virtual EntityId observed_entity() const = 0;
  virtual void event(TypedValue value, std::uint8_t flags) = 0;
};

struct EntityLocation {
  EntityId entity{0};
  NodeAddress node{0};
  EndpointId endpoint{0};
};

// Portable commissioning core. Time is supplied by the caller, allowing
// deterministic execution on host tests and timer-free embedded operation.
class NodeRuntime {
 public:
  NodeRuntime(NodeUid uid, CommissioningRole role, IAddressStorage& storage,
              IFrameTransmitter& transmitter, INodeEvents* events = nullptr);

  void receive(const RawCanFrame& raw, std::uint32_t now_ms);
  void tick(std::uint32_t now_ms);
  bool register_owned_entity(EntityId entity, EndpointId endpoint);
  bool register_observed_entity(EntityId entity);
  bool register_endpoint(IEndpointHandler& endpoint);
  bool register_state_listener(IStateListener& listener);
  bool register_event_listener(IEventListener& listener);
  bool publish_state(EndpointId endpoint, TypedValue value,
                     StateQuality quality = StateQuality::kValid);
  bool publish_event(EndpointId endpoint, TypedValue value, std::uint8_t flags = 0);
  bool enqueue(const ProtocolFrame& frame);
  bool drain_one();
  [[nodiscard]] std::optional<NodeAddress> address() const { return address_; }
  [[nodiscard]] std::optional<EntityLocation> resolve(EntityId entity) const;
  [[nodiscard]] bool is_online(NodeAddress address) const;

 private:
  void handle(const ProtocolFrame& frame, std::uint32_t now_ms);
  void assign(const std::array<std::uint8_t, 8>& discovery);
  void send_heartbeat(std::uint32_t now_ms);
  void learn_entity(NodeAddress source, const std::array<std::uint8_t, 8>& bytes);
  void expire_offline(std::uint32_t now_ms);
  void withdraw_address();
  void send_claim();
  void send_discovery_request(std::uint32_t now_ms);
  void schedule_discovery_response(const std::array<std::uint8_t, 8>& request,
                                   NodeAddress requester, std::uint32_t now_ms);
  void send_discovery_response(std::uint8_t transaction, NodeAddress requester);
  void send_entity_claim(EntityId entity, EndpointId endpoint);
  void send_entity_resolve(EntityId entity);
  void send_address_conflict(const NodeUid& winner);
  void acknowledge(const ProtocolFrame& frame, Status status);
  [[nodiscard]] IEndpointHandler* endpoint_handler(EndpointId endpoint) const;
  [[nodiscard]] bool owns_endpoint(EndpointId endpoint) const;
  [[nodiscard]] std::optional<EntityId> entity_for(NodeAddress source,
                                                    EndpointId endpoint) const;
  bool is_duplicate(const ProtocolFrame& frame, std::uint32_t now_ms,
                    Status *previous_status = nullptr);
  void set_duplicate_status(const ProtocolFrame& frame, Status status);
  [[nodiscard]] std::optional<NodeAddress> next_free_address() const;

  NodeUid uid_;
  CommissioningRole role_;
  IAddressStorage& storage_;
  IFrameTransmitter& transmitter_;
  INodeEvents* events_;
  std::optional<NodeAddress> address_;
  std::array<bool, 0x200> allocated_{};
  std::array<std::uint32_t, 0x200> last_seen_{};
  std::array<bool, 0x200> seen_{};
  std::array<std::optional<EntityLocation>, 32> entities_{};
  std::array<std::optional<EntityLocation>, 16> owned_entities_{};
  std::array<std::optional<EntityId>, 16> observed_entities_{};
  std::array<IEndpointHandler*, 16> endpoint_handlers_{};
  std::array<IStateListener*, 16> state_listeners_{};
  std::array<IEventListener*, 16> event_listeners_{};
  std::array<RawCanFrame, 8> control_queue_{};
  std::array<RawCanFrame, 8> event_queue_{};
  std::array<RawCanFrame, 8> state_queue_{};
  std::array<RawCanFrame, 8> management_queue_{};
  std::array<RawCanFrame, 8> diagnostic_queue_{};
  std::uint8_t control_count_{0};
  std::uint8_t event_count_{0};
  std::uint8_t state_count_{0};
  std::uint8_t management_count_{0};
  std::uint8_t diagnostic_count_{0};
  struct Duplicate {
    NodeAddress source{0};
    EndpointId endpoint{0};
    std::uint8_t transaction{0};
    MessageKind kind{MessageKind::kProtocolError};
    std::uint32_t seen_ms{0};
    Status status{Status::kUnsupported};
  };
  std::array<std::optional<Duplicate>, 16> duplicates_{};
  struct PendingDiscoveryResponse {
    std::uint8_t transaction{0};
    NodeAddress requester{0};
    std::uint32_t due_ms{0};
  };
  std::optional<PendingDiscoveryResponse> pending_discovery_response_{};
  std::uint32_t next_heartbeat_ms_{0};
  std::uint32_t next_claim_ms_{0};
  std::uint32_t next_discovery_ms_{0};
  std::uint8_t discovery_attempt_{0};
  std::uint8_t claims_sent_{0};
  bool claim_schedule_initialized_{false};
  std::uint8_t owned_claim_index_{0};
  std::uint8_t observed_resolve_index_{0};
  std::uint8_t sequence_{0};
};

}  // namespace hacan::protocol
