#pragma once

#include <array>
#include <optional>

#include "esphome/components/canbus/canbus.h"
#include "esphome/core/component.h"
#include "esphome/core/preferences.h"
#include "protocol_node_runtime.h"

namespace esphome::hacan_esphome {

class HacanComponent : public Component, public ::hacan::protocol::IFrameTransmitter,
                       public ::hacan::protocol::IAddressStorage,
                       public ::hacan::protocol::INodeEvents {
 public:
  void set_canbus(canbus::Canbus *canbus) { canbus_ = canbus; }
  void set_commissioning_role(uint8_t role);
  void add_owned_entity(uint32_t entity, uint8_t endpoint);
  void add_observed_entity(uint32_t entity);
  void add_owned_endpoint(uint32_t entity, uint8_t endpoint,
                          ::hacan::protocol::IEndpointHandler *handler);
  bool publish_bool_state(uint8_t endpoint, bool value);
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }
  bool transmit(const ::hacan::protocol::RawCanFrame &frame) override;
  std::optional<::hacan::protocol::NodeAddress> load() override;
  bool save(::hacan::protocol::NodeAddress address) override;
  void entity_available(::hacan::protocol::EntityId entity,
                        ::hacan::protocol::NodeAddress node,
                        ::hacan::protocol::EndpointId endpoint) override;
  void entity_unavailable(::hacan::protocol::EntityId entity) override;
  void address_conflict(::hacan::protocol::NodeAddress address) override;
  [[nodiscard]] uint32_t received_frames() const { return rx_frames_; }
  [[nodiscard]] uint32_t malformed_frames() const { return malformed_frames_; }
  [[nodiscard]] uint32_t transmitted_frames() const { return tx_frames_; }

 protected:
  static constexpr uint8_t kMaxConfiguredEntities = 16;

  static void log_frame(const char *direction, const ::hacan::protocol::RawCanFrame &frame,
                        const char *result);

  canbus::Canbus *canbus_{nullptr};
  ::hacan::protocol::CommissioningRole role_{::hacan::protocol::CommissioningRole::kNone};
  std::array<std::optional<::hacan::protocol::EntityLocation>, kMaxConfiguredEntities> owned_{};
  std::array<std::optional<::hacan::protocol::EntityId>, kMaxConfiguredEntities> observed_{};
  struct OwnedEndpoint {
    ::hacan::protocol::EntityId entity;
    ::hacan::protocol::EndpointId endpoint;
    ::hacan::protocol::IEndpointHandler *handler;
  };
  std::array<std::optional<OwnedEndpoint>, kMaxConfiguredEntities> owned_endpoints_{};
  std::optional<::hacan::protocol::NodeRuntime> runtime_{};
  ESPPreferenceObject address_preference_{};
  std::array<uint8_t, 6> uid_{};
  uint32_t rx_frames_{0};
  uint32_t malformed_frames_{0};
  uint32_t tx_frames_{0};
};

}  // namespace esphome::hacan_esphome
