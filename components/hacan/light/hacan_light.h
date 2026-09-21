#pragma once

#include <array>

#include "esphome/components/light/light_output.h"
#include "esphome/components/light/light_state.h"
#include "esphome/components/output/binary_output.h"
#include "esphome/core/component.h"

#include "../event_source_binding.h"
#include "../hacan.h"

namespace esphome::hacan_esphome {

class HacanLightOutput final : public light::LightOutput,
                               public Component,
                               public ::hacan::protocol::IEndpointHandler,
                               public IEventActionTarget {
 public:
  void set_hacan(HacanComponent *hacan) { hacan_ = hacan; }
  void set_output(output::BinaryOutput *output) { output_ = output; }
  void set_endpoint(uint8_t endpoint) { endpoint_ = endpoint; }
  light::LightTraits get_traits() override;
  void setup_state(light::LightState *state) override { state_ = state; }
  void write_state(light::LightState *state) override;
  [[nodiscard]] ::hacan::protocol::EndpointId endpoint() const override {
    return ::hacan::protocol::EndpointId{endpoint_};
  }
  ::hacan::protocol::Status command(::hacan::protocol::TypedValue value) override;
  ::hacan::protocol::IEventListener *add_event_source(uint32_t entity, uint8_t event,
                                                       uint8_t action);
  void apply_event_action(EventAction action) override;
  void dump_config() override;

 protected:
  HacanComponent *hacan_{nullptr};
  output::BinaryOutput *output_{nullptr};
  light::LightState *state_{nullptr};
  uint8_t endpoint_{0};
  std::array<EventSourceBinding, 8> event_sources_{};
  uint8_t event_source_count_{0};
};

}  // namespace esphome::hacan_esphome
