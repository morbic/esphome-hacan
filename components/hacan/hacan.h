#pragma once

#include <array>
#include <memory>
#include <vector>

#include "esphome/components/canbus/canbus.h"
#include "esphome/core/component.h"
#include "esphome/core/preferences.h"
#include "protocol_node_runtime.h"

namespace esphome::hacan_esphome {

class HacanComponent : public Component, public ::hacan::protocol::IFrameTransmitter,
                       public ::hacan::protocol::IAddressStorage {
 public:
  void set_canbus(canbus::Canbus *canbus) { canbus_ = canbus; }
  void set_commissioning_role(uint8_t role);
  void add_owned_entity(uint32_t entity, uint8_t endpoint);
  void add_observed_entity(uint32_t entity);
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }
  bool transmit(const ::hacan::protocol::RawCanFrame &frame) override;
  std::optional<::hacan::protocol::NodeAddress> load() override;
  bool save(::hacan::protocol::NodeAddress address) override;

 protected:
  canbus::Canbus *canbus_{nullptr};
  ::hacan::protocol::CommissioningRole role_{::hacan::protocol::CommissioningRole::kNone};
  std::vector<::hacan::protocol::EntityLocation> owned_{};
  std::vector<::hacan::protocol::EntityId> observed_{};
  std::unique_ptr<::hacan::protocol::NodeRuntime> runtime_{};
  ESPPreferenceObject address_preference_{};
  std::array<uint8_t, 6> uid_{};
  uint32_t rx_frames_{0};
  uint32_t tx_frames_{0};
};

}  // namespace esphome::hacan_esphome
