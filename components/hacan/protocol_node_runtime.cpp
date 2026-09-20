#include "protocol_node_runtime.h"

namespace hacan::protocol {
namespace {

std::uint32_t discovery_response_delay(const NodeUid& uid,
                                       const std::array<std::uint8_t, 8>& request) {
  std::uint32_t hash = static_cast<std::uint32_t>(request[4]) |
                       (static_cast<std::uint32_t>(request[5]) << 8U) |
                       (static_cast<std::uint32_t>(request[6]) << 16U) |
                       (static_cast<std::uint32_t>(request[7]) << 24U);
  for (const auto byte : uid) hash = (hash * 33U) ^ byte;
  const auto window = static_cast<std::uint16_t>(request[2]) |
                      (static_cast<std::uint16_t>(request[3]) << 8U);
  return hash % window;
}

std::uint32_t uid_hash(const NodeUid& uid) {
  std::uint32_t hash = 2166136261U;
  for (const auto byte : uid) hash = (hash ^ byte) * 16777619U;
  return hash;
}

std::uint32_t claim_start_delay(const NodeUid& uid) {
  return 1000U + (uid_hash(uid) % 1501U);
}

std::uint32_t claim_interval(const NodeUid& uid) {
  return 250U + (uid_hash(uid) % 251U);
}

}  // namespace

NodeRuntime::NodeRuntime(NodeUid uid, CommissioningRole role, IAddressStorage& storage,
                         IFrameTransmitter& transmitter, INodeEvents* events)
    : uid_(uid), role_(role), storage_(storage), transmitter_(transmitter),
      events_(events), address_(storage.load()) {
  if (!address_ && role_ == CommissioningRole::kPrimary) {
    const NodeAddress bootstrap_address{0x001};
    if (storage_.save(bootstrap_address)) address_ = bootstrap_address;
  }
  if (address_) allocated_[address_->value()] = true;
}

void NodeRuntime::receive(const RawCanFrame& raw, std::uint32_t now_ms) {
  const auto decoded = FrameCodec::decode(raw);
  if (const auto* frame = std::get_if<ProtocolFrame>(&decoded)) handle(*frame, now_ms);
}

void NodeRuntime::tick(std::uint32_t now_ms) {
  expire_offline(now_ms);
  if (pending_discovery_response_ && now_ms >= pending_discovery_response_->due_ms) {
    send_discovery_response(pending_discovery_response_->transaction,
                            pending_discovery_response_->requester);
    pending_discovery_response_.reset();
    drain_one();
    return;
  }
  if (address_ && !claim_schedule_initialized_) {
    next_claim_ms_ = now_ms + claim_start_delay(uid_);
    claim_schedule_initialized_ = true;
  }
  if (!address_ && now_ms >= next_discovery_ms_) {
    send_discovery_request(now_ms);
    drain_one();
    return;
  }
  if (address_ && claims_sent_ < 2 && now_ms >= next_claim_ms_) {
    send_claim();
    drain_one();
    return;
  }
  if (address_ && claims_sent_ == 2 && now_ms >= next_heartbeat_ms_) {
    send_heartbeat(now_ms);
    return;
  }
  while (owned_claim_index_ < owned_entities_.size() &&
         !owned_entities_[owned_claim_index_]) {
    ++owned_claim_index_;
  }
  if (address_ && claims_sent_ == 2 && owned_claim_index_ < owned_entities_.size()) {
    const auto& owned = owned_entities_[owned_claim_index_++];
    send_entity_claim(owned->entity, owned->endpoint);
    drain_one();
    return;
  }
  while (observed_resolve_index_ < observed_entities_.size() &&
         !observed_entities_[observed_resolve_index_]) {
    ++observed_resolve_index_;
  }
  if (address_ && claims_sent_ == 2 && observed_resolve_index_ < observed_entities_.size()) {
    const auto& observed = observed_entities_[observed_resolve_index_++];
    send_entity_resolve(*observed);
  }
  drain_one();
}

void NodeRuntime::send_discovery_request(std::uint32_t now_ms) {
  std::array<std::uint8_t, 8> bytes{};
  bytes[0] = sequence_++;
  bytes[1] = 0x02;  // Include unassigned nodes.
  bytes[2] = 0xE8;
  bytes[3] = 0x03;
  bytes[4] = uid_[0] ^ sequence_;
  bytes[5] = uid_[1];
  bytes[6] = uid_[2];
  bytes[7] = uid_[3];
  const auto payload = DiscoveryRequestFrame::decode(bytes);
  const auto id = CanIdentifier::create(Priority::kManagement,
                                        MessageKind::kDiscoveryRequest,
                                        NodeAddress{0x1FF}, NodeAddress{0}, 0);
  if (payload && id) enqueue(ProtocolFrame{*id, *payload});
  schedule_discovery_response(bytes, NodeAddress{0}, now_ms);
  constexpr std::array<std::uint32_t, 4> kRetry{2000, 4000, 8000, 16000};
  const auto delay = discovery_attempt_ < kRetry.size() ? kRetry[discovery_attempt_] : 30000;
  if (discovery_attempt_ < kRetry.size()) ++discovery_attempt_;
  next_discovery_ms_ = now_ms + delay;
}

void NodeRuntime::handle(const ProtocolFrame& frame, std::uint32_t now_ms) {
  const auto source = frame.identifier().source();
  if (source.value() != 0) {
    seen_[source.value()] = true;
    last_seen_[source.value()] = now_ms;
  }
  if (frame.identifier().kind() == MessageKind::kAddressClaim) {
    const auto& payload = std::get<AddressClaimFrame>(frame.payload()).bytes();
    allocated_[source.value()] = true;
    if (address_ && source.value() == address_->value()) {
      bool self = true;
      for (std::uint8_t index = 0; index < uid_.size(); ++index) {
        if (payload[index] != uid_[index]) self = false;
      }
      if (!self) {
        bool own_uid_wins = false;
        for (std::int8_t index = static_cast<std::int8_t>(uid_.size()) - 1;
             index >= 0; --index) {
          const auto byte_index = static_cast<std::uint8_t>(index);
          if (uid_[byte_index] == payload[byte_index]) continue;
          own_uid_wins = uid_[byte_index] < payload[byte_index];
          break;
        }
        if (own_uid_wins) {
          send_address_conflict(uid_);
        } else {
          if (events_) events_->address_conflict(*address_);
          withdraw_address();
        }
      }
    }
    return;
  }
  if (frame.identifier().kind() == MessageKind::kDiscoveryRequest) {
    schedule_discovery_response(std::get<DiscoveryRequestFrame>(frame.payload()).bytes(),
                                frame.identifier().source(), now_ms);
    return;
  }
  if (frame.identifier().kind() == MessageKind::kAddressConflict && address_ &&
      frame.identifier().destination().value() == address_->value()) {
    const auto& payload = std::get<AddressConflictFrame>(frame.payload()).bytes();
    bool winner_is_lower = false;
    for (std::int8_t index = static_cast<std::int8_t>(uid_.size()) - 1; index >= 0; --index) {
      if (payload[static_cast<std::uint8_t>(index)] == uid_[static_cast<std::uint8_t>(index)]) continue;
      winner_is_lower = payload[static_cast<std::uint8_t>(index)] < uid_[static_cast<std::uint8_t>(index)];
      break;
    }
    if (winner_is_lower) {
      if (events_) events_->address_conflict(*address_);
      withdraw_address();
    }
    return;
  }
  if (frame.identifier().kind() == MessageKind::kAddressAssign) {
    const auto& payload = std::get<AddressAssignFrame>(frame.payload()).bytes();
    bool matches = true;
    for (std::uint8_t index = 0; index < uid_.size(); ++index) matches &= payload[index] == uid_[index];
    if (matches) {
      const auto assigned = NodeAddress{static_cast<std::uint16_t>(
          static_cast<std::uint16_t>(payload[6]) |
          (static_cast<std::uint16_t>(payload[7] & 1U) << 8U))};
      if (storage_.save(assigned)) {
        address_ = assigned;
        allocated_[assigned.value()] = true;
        claims_sent_ = 0;
        claim_schedule_initialized_ = false;
        owned_claim_index_ = 0;
        observed_resolve_index_ = 0;
        next_claim_ms_ = now_ms;
      }
    }
    return;
  }
  if (frame.identifier().kind() == MessageKind::kEntityClaim) {
    learn_entity(source, std::get<EntityClaimFrame>(frame.payload()).bytes());
    return;
  }
  if (frame.identifier().kind() == MessageKind::kEntityResolve && address_) {
    const auto& bytes = std::get<EntityResolveFrame>(frame.payload()).bytes();
    const auto entity = EntityId{static_cast<std::uint32_t>(bytes[2]) |
                                 (static_cast<std::uint32_t>(bytes[3]) << 8U) |
                                 (static_cast<std::uint32_t>(bytes[4]) << 16U) |
                                 (static_cast<std::uint32_t>(bytes[5]) << 24U)};
    for (const auto& owned : owned_entities_) {
      if (owned && owned->entity.value() == entity.value()) {
        send_entity_claim(entity, owned->endpoint);
      }
    }
    return;
  }
  if (frame.identifier().kind() == MessageKind::kState) {
    const auto& bytes = std::get<StateFrame>(frame.payload()).bytes();
    const auto entity = entity_for(source, EndpointId{bytes[1]});
    if (!entity) return;
    const TypedValue value{static_cast<DataType>(bytes[2]),
                           {bytes[4], bytes[5], bytes[6], bytes[7]}};
    const auto quality = static_cast<StateQuality>(bytes[3]);
    for (auto* listener : state_listeners_) {
      if (listener && listener->observed_entity().value() == entity->value()) {
        listener->state(value, quality);
      }
    }
    return;
  }
  if (frame.identifier().kind() == MessageKind::kCommand && address_ &&
      frame.identifier().destination().value() == address_->value()) {
    const auto& bytes = std::get<CommandFrame>(frame.payload()).bytes();
    const TypedValue value{static_cast<DataType>(bytes[2]),
                           {bytes[4], bytes[5], bytes[6], bytes[7]}};
    Status status = Status::kUnsupported;
    const bool duplicate = is_duplicate(frame, now_ms, &status);
    if (!duplicate) {
      if (auto* endpoint = endpoint_handler(EndpointId{bytes[1]})) {
        status = endpoint->command(value);
      }
      set_duplicate_status(frame, status);
    }
    if ((bytes[3] & 1U) != 0) acknowledge(frame, status);
    return;
  }
  if (frame.identifier().kind() == MessageKind::kEvent && address_ &&
      frame.identifier().destination().value() == address_->value()) {
    const auto& bytes = std::get<EventFrame>(frame.payload()).bytes();
    if ((bytes[3] & 1U) != 0) acknowledge(frame, Status::kAccepted);
    return;
  }
  if (frame.identifier().kind() != MessageKind::kDiscoveryResponse ||
      role_ != CommissioningRole::kPrimary || !address_) return;
  if (frame.identifier().destination().value() != address_->value() &&
      !(frame.identifier().source().value() == 0 &&
        frame.identifier().destination().value() == 0)) return;
  const auto& payload = std::get<DiscoveryResponseFrame>(frame.payload()).bytes();
  assign(payload);
}

bool NodeRuntime::register_owned_entity(EntityId entity, EndpointId endpoint) {
  if (entity.value() == 0 || endpoint.value() == 0 || endpoint.value() == 0xFF) return false;
  for (auto& entry : owned_entities_) {
    if (!entry) {
      entry = EntityLocation{entity, NodeAddress{0}, endpoint};
      return true;
    }
  }
  return false;
}

void NodeRuntime::schedule_discovery_response(
    const std::array<std::uint8_t, 8>& request, NodeAddress requester,
    std::uint32_t now_ms) {
  pending_discovery_response_ = PendingDiscoveryResponse{
      request[0], requester, now_ms + discovery_response_delay(uid_, request)};
}

void NodeRuntime::send_discovery_response(std::uint8_t transaction,
                                          NodeAddress requester) {
  std::array<std::uint8_t, 8> bytes{};
  bytes[0] = transaction;
  for (std::uint8_t index = 0; index < uid_.size(); ++index) bytes[index + 1] = uid_[index];
  const auto payload = DiscoveryResponseFrame::decode(bytes);
  const auto source = address_.value_or(NodeAddress{0});
  const auto id = CanIdentifier::create(Priority::kManagement,
                                        MessageKind::kDiscoveryResponse,
                                        requester, source, 0);
  if (payload && id) enqueue(ProtocolFrame{*id, *payload});
}

bool NodeRuntime::register_observed_entity(EntityId entity) {
  if (entity.value() == 0) return false;
  for (const auto& entry : observed_entities_) {
    if (entry && entry->value() == entity.value()) return true;
  }
  for (auto& entry : observed_entities_) {
    if (!entry) {
      entry = entity;
      return true;
    }
  }
  return false;
}

bool NodeRuntime::register_endpoint(IEndpointHandler& endpoint) {
  const auto id = endpoint.endpoint().value();
  if (id == 0 || id == 0xFF) return false;
  for (auto*& registered : endpoint_handlers_) {
    if (!registered) {
      registered = &endpoint;
      return true;
    }
    if (registered->endpoint().value() == id) return registered == &endpoint;
  }
  return false;
}

bool NodeRuntime::register_state_listener(IStateListener& listener) {
  const auto entity = listener.observed_entity();
  if (entity.value() == 0) return false;
  for (auto*& registered : state_listeners_) {
    if (registered == &listener) return true;
  }
  for (auto*& registered : state_listeners_) {
    if (!registered) {
      if (!register_observed_entity(entity)) return false;
      registered = &listener;
      return true;
    }
  }
  return false;
}

bool NodeRuntime::publish_state(EndpointId endpoint, TypedValue value,
                                StateQuality quality) {
  if (!address_ || !owns_endpoint(endpoint)) return false;
  const auto encoded_payload = StatePayload::create(sequence_++, endpoint, value, quality);
  const auto id = CanIdentifier::create(Priority::kStateQuery, MessageKind::kState,
                                        NodeAddress{0x1FF}, *address_, 0);
  if (!encoded_payload || !id) return false;
  const auto payload = StateFrame::decode(encoded_payload->encode());
  if (!payload) return false;
  return enqueue(ProtocolFrame{*id, *payload});
}

bool NodeRuntime::enqueue(const ProtocolFrame& frame) {
  const auto encoded = FrameCodec::encode(frame);
  const auto* raw = std::get_if<RawCanFrame>(&encoded);
  if (!raw) return false;
  auto add = [&raw](auto& queue, std::uint8_t& count) {
    if (count == queue.size()) return false;
    queue[count++] = *raw;
    return true;
  };
  switch (frame.identifier().priority()) {
    case Priority::kControl: return add(control_queue_, control_count_);
    case Priority::kEvent: return add(event_queue_, event_count_);
    case Priority::kStateQuery:
      if (frame.identifier().kind() == MessageKind::kState) {
        const auto endpoint = std::get<StateFrame>(frame.payload()).bytes()[1];
        for (std::uint8_t index = 0; index < state_count_; ++index) {
          const auto decoded = FrameCodec::decode(state_queue_[index]);
          if (const auto* pending = std::get_if<ProtocolFrame>(&decoded);
              pending && pending->identifier().kind() == MessageKind::kState &&
              pending->identifier().source().value() == frame.identifier().source().value() &&
              pending->identifier().destination().value() == frame.identifier().destination().value() &&
              std::get<StateFrame>(pending->payload()).bytes()[1] == endpoint) {
            state_queue_[index] = *raw;
            return true;
          }
        }
      }
      return add(state_queue_, state_count_);
    case Priority::kManagement: return add(management_queue_, management_count_);
    case Priority::kDiagnostic: return add(diagnostic_queue_, diagnostic_count_);
    default: return false;
  }
}

bool NodeRuntime::drain_one() {
  auto drain = [this](auto& queue, std::uint8_t& count) {
    if (count == 0) return false;
    const auto frame = queue[0];
    for (std::uint8_t i = 1; i < count; ++i) queue[i - 1] = queue[i];
    --count;
    return transmitter_.transmit(frame);
  };
  if (drain(control_queue_, control_count_)) return true;
  if (drain(event_queue_, event_count_)) return true;
  if (drain(state_queue_, state_count_)) return true;
  if (drain(management_queue_, management_count_)) return true;
  return drain(diagnostic_queue_, diagnostic_count_);
}

void NodeRuntime::send_heartbeat(std::uint32_t now_ms) {
  const std::array<std::uint8_t, 8> bytes{sequence_++, 0, 1, 0, 0, 0, 0, 0};
  const auto payload = HeartbeatFrame::decode(bytes);
  const auto id = CanIdentifier::create(Priority::kManagement, MessageKind::kHeartbeat,
                                        NodeAddress{0x1FF}, *address_, 0);
  if (payload && id) {
    const auto encoded = FrameCodec::encode(ProtocolFrame{*id, *payload});
    if (const auto* raw = std::get_if<RawCanFrame>(&encoded)) transmitter_.transmit(*raw);
  }
  next_heartbeat_ms_ = now_ms + 30000;
}

void NodeRuntime::learn_entity(NodeAddress source,
                               const std::array<std::uint8_t, 8>& bytes) {
  const auto value = static_cast<std::uint32_t>(bytes[2]) |
                     (static_cast<std::uint32_t>(bytes[3]) << 8U) |
                     (static_cast<std::uint32_t>(bytes[4]) << 16U) |
                     (static_cast<std::uint32_t>(bytes[5]) << 24U);
  const EntityLocation location{EntityId{value}, source, EndpointId{bytes[1]}};
  for (auto& entry : entities_) {
    if (entry && entry->entity.value() == value) {
      if (entry->node.value() == source.value() && entry->endpoint.value() == bytes[1]) return;
      entry.reset();
      if (events_) events_->entity_unavailable(EntityId{value});
      return;
    }
  }
  for (auto& entry : entities_) {
    if (!entry) {
      entry = location;
      if (events_) events_->entity_available(location.entity, source, location.endpoint);
      return;
    }
  }
}

void NodeRuntime::expire_offline(std::uint32_t now_ms) {
  for (std::uint16_t source = 1; source < 0x1FF; ++source) {
    if (!seen_[source] || now_ms - last_seen_[source] < 105000) continue;
    seen_[source] = false;
    for (auto& entry : entities_) {
      if (entry && entry->node.value() == source) {
        if (events_) events_->entity_unavailable(entry->entity);
        entry.reset();
      }
    }
  }
}

void NodeRuntime::withdraw_address() {
  address_.reset();
  next_heartbeat_ms_ = 0;
  claims_sent_ = 0;
  claim_schedule_initialized_ = false;
}

void NodeRuntime::send_claim() {
  if (!address_) return;
  std::array<std::uint8_t, 8> bytes{};
  for (std::uint8_t index = 0; index < uid_.size(); ++index) bytes[index] = uid_[index];
  bytes[6] = 1;
  bytes[7] = static_cast<std::uint8_t>(uid_[0] ^ uid_[5]);
  const auto payload = AddressClaimFrame::decode(bytes);
  const auto id = CanIdentifier::create(Priority::kManagement, MessageKind::kAddressClaim,
                                        NodeAddress{0x1FF}, *address_, 0);
  if (payload && id) enqueue(ProtocolFrame{*id, *payload});
  ++claims_sent_;
  next_claim_ms_ += claim_interval(uid_);
}

void NodeRuntime::send_entity_claim(EntityId entity, EndpointId endpoint) {
  if (!address_) return;
  std::array<std::uint8_t, 8> bytes{};
  bytes[0] = sequence_++;
  bytes[1] = endpoint.value();
  bytes[2] = static_cast<std::uint8_t>(entity.value());
  bytes[3] = static_cast<std::uint8_t>(entity.value() >> 8U);
  bytes[4] = static_cast<std::uint8_t>(entity.value() >> 16U);
  bytes[5] = static_cast<std::uint8_t>(entity.value() >> 24U);
  const auto payload = EntityClaimFrame::decode(bytes);
  const auto id = CanIdentifier::create(Priority::kManagement, MessageKind::kEntityClaim,
                                        NodeAddress{0x1FF}, *address_, 0);
  if (payload && id) enqueue(ProtocolFrame{*id, *payload});
}

void NodeRuntime::send_entity_resolve(EntityId entity) {
  if (!address_) return;
  std::array<std::uint8_t, 8> bytes{};
  bytes[0] = sequence_++;
  bytes[2] = static_cast<std::uint8_t>(entity.value());
  bytes[3] = static_cast<std::uint8_t>(entity.value() >> 8U);
  bytes[4] = static_cast<std::uint8_t>(entity.value() >> 16U);
  bytes[5] = static_cast<std::uint8_t>(entity.value() >> 24U);
  bytes[6] = 0xE8;
  bytes[7] = 0x03;
  const auto payload = EntityResolveFrame::decode(bytes);
  const auto id = CanIdentifier::create(Priority::kManagement, MessageKind::kEntityResolve,
                                        NodeAddress{0x1FF}, *address_, 0);
  if (payload && id) enqueue(ProtocolFrame{*id, *payload});
}

void NodeRuntime::send_address_conflict(const NodeUid& winner) {
  if (!address_) return;
  std::array<std::uint8_t, 8> bytes{};
  for (std::uint8_t index = 0; index < winner.size(); ++index) bytes[index] = winner[index];
  bytes[6] = 1;
  bytes[7] = sequence_++;
  const auto payload = AddressConflictFrame::decode(bytes);
  const auto id = CanIdentifier::create(Priority::kControl,
                                        MessageKind::kAddressConflict,
                                        *address_, *address_, 0);
  if (payload && id) enqueue(ProtocolFrame{*id, *payload});
}

bool NodeRuntime::is_duplicate(const ProtocolFrame& frame, std::uint32_t now_ms,
                               Status *previous_status) {
  const auto bytes = frame.identifier().kind() == MessageKind::kCommand
                         ? std::get<CommandFrame>(frame.payload()).bytes()
                         : std::get<EventFrame>(frame.payload()).bytes();
  for (auto& record : duplicates_) {
    if (record && record->source.value() == frame.identifier().source().value() &&
        record->endpoint.value() == bytes[1] && record->transaction == bytes[0] &&
        record->kind == frame.identifier().kind() && now_ms - record->seen_ms <= 2000) {
      if (previous_status != nullptr) *previous_status = record->status;
      return true;
    }
  }
  for (auto& record : duplicates_) {
    if (!record) {
      record = Duplicate{frame.identifier().source(), EndpointId{bytes[1]}, bytes[0],
                         frame.identifier().kind(), now_ms, Status::kUnsupported};
      return false;
    }
  }
  duplicates_[0] = Duplicate{frame.identifier().source(), EndpointId{bytes[1]}, bytes[0],
                             frame.identifier().kind(), now_ms, Status::kUnsupported};
  return false;
}

void NodeRuntime::set_duplicate_status(const ProtocolFrame& frame, Status status) {
  const auto& bytes = std::get<CommandFrame>(frame.payload()).bytes();
  for (auto& record : duplicates_) {
    if (record && record->source.value() == frame.identifier().source().value() &&
        record->endpoint.value() == bytes[1] && record->transaction == bytes[0] &&
        record->kind == frame.identifier().kind()) {
      record->status = status;
      return;
    }
  }
}

void NodeRuntime::acknowledge(const ProtocolFrame& frame, Status status) {
  if (!address_) return;
  const auto bytes = frame.identifier().kind() == MessageKind::kCommand
                         ? std::get<CommandFrame>(frame.payload()).bytes()
                         : std::get<EventFrame>(frame.payload()).bytes();
  const std::array<std::uint8_t, 8> ack{bytes[0], bytes[1],
      static_cast<std::uint8_t>(frame.identifier().kind()), static_cast<std::uint8_t>(status),
      0, 0, 0, 0};
  const auto payload = AckFrame::decode(ack);
  const auto id = CanIdentifier::create(Priority::kControl, MessageKind::kAck,
                                        frame.identifier().source(), *address_, 0);
  if (payload && id) enqueue(ProtocolFrame{*id, *payload});
}

IEndpointHandler* NodeRuntime::endpoint_handler(EndpointId endpoint) const {
  for (auto* handler : endpoint_handlers_) {
    if (handler && handler->endpoint().value() == endpoint.value()) return handler;
  }
  return nullptr;
}

bool NodeRuntime::owns_endpoint(EndpointId endpoint) const {
  for (const auto& owned : owned_entities_) {
    if (owned && owned->endpoint.value() == endpoint.value()) return true;
  }
  return false;
}

std::optional<EntityId> NodeRuntime::entity_for(NodeAddress source,
                                                 EndpointId endpoint) const {
  if (address_ && source.value() == address_->value()) {
    for (const auto& owned : owned_entities_) {
      if (owned && owned->endpoint.value() == endpoint.value()) return owned->entity;
    }
  }
  for (const auto& entity : entities_) {
    if (entity && entity->node.value() == source.value() &&
        entity->endpoint.value() == endpoint.value()) {
      return entity->entity;
    }
  }
  return std::nullopt;
}

std::optional<EntityLocation> NodeRuntime::resolve(EntityId entity) const {
  for (const auto& entry : entities_) {
    if (entry && entry->entity.value() == entity.value()) return entry;
  }
  return std::nullopt;
}

bool NodeRuntime::is_online(NodeAddress address) const {
  return address.value() != 0 && address.value() != 0x1FF && seen_[address.value()];
}

std::optional<NodeAddress> NodeRuntime::next_free_address() const {
  for (std::uint16_t value = 1; value < 0x1FF; ++value) {
    if (!allocated_[value]) return NodeAddress{value};
  }
  return std::nullopt;
}

void NodeRuntime::assign(const std::array<std::uint8_t, 8>& discovery) {
  const auto candidate = next_free_address();
  if (!candidate || !address_) return;
  std::array<std::uint8_t, 8> bytes{};
  for (std::uint8_t index = 0; index < 6; ++index) bytes[index] = discovery[index + 1];
  bytes[6] = static_cast<std::uint8_t>(candidate->value());
  bytes[7] = static_cast<std::uint8_t>((candidate->value() >> 8U) | 0x02U);
  const auto payload = AddressAssignFrame::decode(bytes);
  const auto id = CanIdentifier::create(Priority::kManagement, MessageKind::kAddressAssign,
                                        NodeAddress{0x1FF}, *address_, 0);
  if (!payload || !id) return;
  const ProtocolFrame frame{*id, *payload};
  const auto encoded = FrameCodec::encode(frame);
  if (const auto* raw = std::get_if<RawCanFrame>(&encoded)) {
    if (transmitter_.transmit(*raw)) allocated_[candidate->value()] = true;
  }
}

}  // namespace hacan::protocol
