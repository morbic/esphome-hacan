#pragma once

#include "esphome/components/output/binary_output.h"
#include "esphome/components/switch/switch.h"
#include "esphome/core/component.h"

#include "../hacan.h"

namespace esphome::hacan_esphome {

class HacanSwitch final : public switch_::Switch,
                          public Component,
                          public ::hacan::protocol::IEndpointHandler {
 public:
  void set_hacan(HacanComponent *hacan) { hacan_ = hacan; }
  void set_output(output::BinaryOutput *output) { output_ = output; }
  void set_endpoint(uint8_t endpoint) { endpoint_ = endpoint; }
  [[nodiscard]] ::hacan::protocol::EndpointId endpoint() const override {
    return ::hacan::protocol::EndpointId{endpoint_};
  }
  ::hacan::protocol::Status command(::hacan::protocol::TypedValue value) override;
  void dump_config() override;

 protected:
  void write_state(bool state) override;
  void apply_state(bool state);

  HacanComponent *hacan_{nullptr};
  output::BinaryOutput *output_{nullptr};
  uint8_t endpoint_{0};
};

}  // namespace esphome::hacan_esphome
