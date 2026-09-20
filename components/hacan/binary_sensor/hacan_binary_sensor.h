#pragma once

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/core/component.h"

#include "../hacan.h"

namespace esphome::hacan_esphome {

class HacanBinarySensor final : public binary_sensor::BinarySensor, public Component {
 public:
  void set_hacan(HacanComponent *hacan) { hacan_ = hacan; }
  void set_source(binary_sensor::BinarySensor *source) { source_ = source; }
  void set_endpoint(uint8_t endpoint) { endpoint_ = endpoint; }
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA - 1.0f; }

 protected:
  HacanComponent *hacan_{nullptr};
  binary_sensor::BinarySensor *source_{nullptr};
  uint8_t endpoint_{0};
};

}  // namespace esphome::hacan_esphome
