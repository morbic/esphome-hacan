#include "hacan.h"

#include <variant>

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::hacan_esphome {

static const char *const TAG = "hacan";

void HacanComponent::set_commissioning_role(uint8_t role) {
  role_ = static_cast<::hacan::protocol::CommissioningRole>(role);
}

void HacanComponent::add_owned_endpoint(
    uint32_t entity, uint8_t endpoint, ::hacan::protocol::IEndpointHandler *handler) {
  for (auto &configured : owned_endpoints_) {
    if (!configured) continue;
    if (configured->entity.value() == entity && configured->endpoint.value() == endpoint) {
      if (handler != nullptr) configured->handler = handler;
      return;
    }
    if (configured->entity.value() == entity || configured->endpoint.value() == endpoint) {
      ESP_LOGE(TAG, "Conflicting HACAN endpoint entity=0x%08X endpoint=0x%02X",
               static_cast<unsigned>(entity), endpoint);
      return;
    }
  }
  for (auto &configured : owned_endpoints_) {
    if (!configured) {
      configured = OwnedEndpoint{::hacan::protocol::EntityId{entity},
                                 ::hacan::protocol::EndpointId{endpoint}, handler};
      return;
    }
  }
  ESP_LOGE(TAG, "Too many HACAN endpoints; maximum is %u",
           static_cast<unsigned>(kMaxConfiguredEntities));
}

void HacanComponent::add_state_listener(::hacan::protocol::IStateListener *listener) {
  for (auto &configured : state_listeners_) {
    if (configured == nullptr) {
      configured = listener;
      return;
    }
  }
  ESP_LOGE(TAG, "Too many HACAN state listeners; maximum is %u",
           static_cast<unsigned>(kMaxConfiguredEntities));
}

void HacanComponent::add_event_listener(::hacan::protocol::IEventListener *listener) {
  if (listener == nullptr) {
    ESP_LOGE(TAG, "Cannot register a null HACAN event listener");
    return;
  }
  for (auto *configured : event_listeners_) {
    if (configured == listener) return;
  }
  for (auto &configured : event_listeners_) {
    if (configured == nullptr) {
      configured = listener;
      return;
    }
  }
  ESP_LOGE(TAG, "Too many HACAN event listeners; maximum is %u",
           static_cast<unsigned>(kMaxConfiguredEntities));
}

bool HacanComponent::publish_bool_state(uint8_t endpoint, bool value) {
  if (!runtime_) return false;
  return runtime_->publish_state(::hacan::protocol::EndpointId{endpoint},
                                 ::hacan::protocol::TypedValue{
                                     ::hacan::protocol::DataType::kBool,
                                     {static_cast<uint8_t>(value), 0, 0, 0}});
}

bool HacanComponent::publish_button_event(uint8_t endpoint,
                                          ::hacan::protocol::ButtonEvent event) {
  if (!runtime_) return false;
  return runtime_->publish_event(::hacan::protocol::EndpointId{endpoint},
                                 ::hacan::protocol::TypedValue{
                                     ::hacan::protocol::DataType::kEnum8,
                                     {static_cast<uint8_t>(event), 0, 0, 0}});
}

void HacanComponent::setup() {
  get_mac_address_raw(uid_.data());
  ::hacan::protocol::NodeUid uid{};
  for (uint8_t index = 0; index < uid.size(); ++index) uid[index] = uid_[index];
  address_preference_ = global_preferences->make_preference<uint16_t>(0x48414341U);
  runtime_.emplace(uid, role_, *this, *this, this);
  for (const auto &endpoint : owned_endpoints_) {
    if (!endpoint) continue;
    runtime_->register_owned_entity(endpoint->entity, endpoint->endpoint);
    if (endpoint->handler) runtime_->register_endpoint(*endpoint->handler);
  }
  for (const auto &listener : state_listeners_) {
    if (listener != nullptr) runtime_->register_state_listener(*listener);
  }
  for (auto *listener : event_listeners_) {
    if (listener != nullptr) runtime_->register_event_listener(*listener);
  }
  canbus_->add_callback([this](uint32_t can_id, bool extended, bool remote,
                               const std::vector<uint8_t> &data) {
    if (remote || data.size() != 8) {
      ESP_LOGI(TAG, "RX rejected can_id=0x%08X extended=%s rtr=%s dlc=%u",
               static_cast<unsigned>(can_id), extended ? "true" : "false",
               remote ? "true" : "false", static_cast<unsigned>(data.size()));
      ++malformed_frames_;
      return;
    }
    ::hacan::protocol::RawCanFrame frame{can_id, extended, static_cast<uint8_t>(data.size()), {}};
    for (uint8_t index = 0; index < 8; ++index) frame.data[index] = data[index];
    const auto decoded = ::hacan::protocol::FrameCodec::decode(frame);
    if (!std::holds_alternative<::hacan::protocol::ProtocolFrame>(decoded)) {
      log_frame("RX", frame, "rejected");
      ++malformed_frames_;
      return;
    }
    log_frame("RX", frame, "accepted");
    ++rx_frames_;
    runtime_->receive(frame, millis());
  });
}

void HacanComponent::log_frame(const char *direction,
                               const ::hacan::protocol::RawCanFrame &frame,
                               const char *result) {
  const auto priority = static_cast<unsigned>((frame.can_id >> 26U) & 0x07U);
  const auto kind = static_cast<unsigned>((frame.can_id >> 21U) & 0x1FU);
  const auto destination = static_cast<unsigned>((frame.can_id >> 12U) & 0x1FFU);
  const auto source = static_cast<unsigned>((frame.can_id >> 3U) & 0x1FFU);
  const auto hop = static_cast<unsigned>(frame.can_id & 0x07U);
  ESP_LOGI(TAG,
           "%s %s can_id=0x%08X extended=%s rtr=false dlc=%u priority=%u kind=0x%02X "
           "destination=0x%03X source=0x%03X hop=%u data=%02X%02X%02X%02X%02X%02X%02X%02X",
           direction, result, static_cast<unsigned>(frame.can_id),
           frame.extended ? "true" : "false", static_cast<unsigned>(frame.dlc), priority, kind,
           destination, source, hop, frame.data[0], frame.data[1], frame.data[2], frame.data[3],
           frame.data[4], frame.data[5], frame.data[6], frame.data[7]);
}

std::optional<::hacan::protocol::NodeAddress> HacanComponent::load() {
  uint16_t value{};
  if (!address_preference_.load(&value) || value == 0 || value >= 0x1FF) return std::nullopt;
  return ::hacan::protocol::NodeAddress{value};
}

bool HacanComponent::save(::hacan::protocol::NodeAddress address) {
  const auto value = address.value();
  const bool saved = address_preference_.save(&value);
  if (saved) ESP_LOGI(TAG, "Stored commissioned address: 0x%03X", value);
  return saved;
}

void HacanComponent::entity_available(::hacan::protocol::EntityId entity,
                                      ::hacan::protocol::NodeAddress node,
                                      ::hacan::protocol::EndpointId endpoint) {
  ESP_LOGI(TAG, "Entity 0x%08X is available at 0x%03X endpoint 0x%02X",
           static_cast<unsigned>(entity.value()), static_cast<unsigned>(node.value()),
           static_cast<unsigned>(endpoint.value()));
}

void HacanComponent::entity_unavailable(::hacan::protocol::EntityId entity) {
  ESP_LOGW(TAG, "Entity 0x%08X is unavailable", static_cast<unsigned>(entity.value()));
}

void HacanComponent::address_conflict(::hacan::protocol::NodeAddress address) {
  ESP_LOGE(TAG, "Address conflict at 0x%03X", static_cast<unsigned>(address.value()));
}

void HacanComponent::loop() {
  if (runtime_) runtime_->tick(millis());
}

void HacanComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "HACAN base component");
  ESP_LOGCONFIG(TAG, "  Node UID: %02X:%02X:%02X:%02X:%02X:%02X", uid_[0], uid_[1], uid_[2], uid_[3], uid_[4], uid_[5]);
  const auto address = load();
  if (address) {
    ESP_LOGCONFIG(TAG, "  Commissioned address: 0x%03X", address->value());
  } else {
    ESP_LOGCONFIG(TAG, "  Commissioned address: unassigned");
  }
  uint8_t endpoint_count = 0;
  uint8_t state_listener_count = 0;
  for (const auto &endpoint : owned_endpoints_) endpoint_count += endpoint.has_value();
  for (const auto *listener : state_listeners_) state_listener_count += listener != nullptr;
  ESP_LOGCONFIG(TAG, "  Owned endpoint profiles: %u", static_cast<unsigned>(endpoint_count));
  ESP_LOGCONFIG(TAG, "  State source subscriptions: %u",
                static_cast<unsigned>(state_listener_count));
  ESP_LOGCONFIG(TAG, "  CAN frames RX/TX/malformed: %u/%u/%u",
                static_cast<unsigned>(rx_frames_), static_cast<unsigned>(tx_frames_),
                static_cast<unsigned>(malformed_frames_));
}

bool HacanComponent::transmit(const ::hacan::protocol::RawCanFrame &frame) {
  std::vector<uint8_t> data(frame.data.begin(), frame.data.end());
  const bool sent = canbus_->send_data(frame.can_id, frame.extended, data) == canbus::ERROR_OK;
  log_frame("TX", frame, sent ? "sent" : "rejected");
  if (sent) ++tx_frames_;
  return sent;
}

}  // namespace esphome::hacan_esphome
