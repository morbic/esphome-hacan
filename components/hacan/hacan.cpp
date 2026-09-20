#include "hacan.h"

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::hacan_esphome {

static const char *const TAG = "hacan";

void HacanComponent::set_commissioning_role(uint8_t role) {
  role_ = static_cast<::hacan::protocol::CommissioningRole>(role);
}

void HacanComponent::add_owned_entity(uint32_t entity, uint8_t endpoint) {
  owned_.push_back({::hacan::protocol::EntityId{entity}, ::hacan::protocol::NodeAddress{0},
                    ::hacan::protocol::EndpointId{endpoint}});
}

void HacanComponent::add_observed_entity(uint32_t entity) {
  observed_.push_back(::hacan::protocol::EntityId{entity});
}

void HacanComponent::setup() {
  get_mac_address_raw(uid_.data());
  ::hacan::protocol::NodeUid uid{};
  for (uint8_t index = 0; index < uid.size(); ++index) uid[index] = uid_[index];
  address_preference_ = global_preferences->make_preference<uint16_t>(0x48414341U);
  runtime_ = std::make_unique<::hacan::protocol::NodeRuntime>(uid, role_, *this, *this);
  for (const auto &entity : owned_) runtime_->register_owned_entity(entity.entity, entity.endpoint);
  canbus_->add_callback([this](uint32_t can_id, bool extended, bool,
                               const std::vector<uint8_t> &data) {
    if (data.size() != 8) return;
    ::hacan::protocol::RawCanFrame frame{can_id, extended, static_cast<uint8_t>(data.size()), {}};
    for (uint8_t index = 0; index < 8; ++index) frame.data[index] = data[index];
    ++rx_frames_;
    runtime_->receive(frame, millis());
  });
}

std::optional<::hacan::protocol::NodeAddress> HacanComponent::load() {
  uint16_t value{};
  if (!address_preference_.load(&value) || value == 0 || value >= 0x1FF) return std::nullopt;
  return ::hacan::protocol::NodeAddress{value};
}

bool HacanComponent::save(::hacan::protocol::NodeAddress address) {
  const auto value = address.value();
  return address_preference_.save(&value);
}

void HacanComponent::loop() { runtime_->tick(millis()); }

void HacanComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "HACAN base component");
  ESP_LOGCONFIG(TAG, "  Node UID: %02X:%02X:%02X:%02X:%02X:%02X", uid_[0], uid_[1], uid_[2], uid_[3], uid_[4], uid_[5]);
  const auto address = load();
  ESP_LOGCONFIG(TAG, "  Commissioned address: %s", address ? "yes" : "no");
  ESP_LOGCONFIG(TAG, "  Owned entities: %u", static_cast<unsigned>(owned_.size()));
  ESP_LOGCONFIG(TAG, "  Observed entities: %u", static_cast<unsigned>(observed_.size()));
  ESP_LOGCONFIG(TAG, "  CAN frames RX/TX: %u/%u", static_cast<unsigned>(rx_frames_), static_cast<unsigned>(tx_frames_));
}

bool HacanComponent::transmit(const ::hacan::protocol::RawCanFrame &frame) {
  std::vector<uint8_t> data(frame.data.begin(), frame.data.end());
  const bool sent = canbus_->send_data(frame.can_id, frame.extended, data) == canbus::ERROR_OK;
  if (sent) ++tx_frames_;
  return sent;
}

}  // namespace esphome::hacan_esphome
