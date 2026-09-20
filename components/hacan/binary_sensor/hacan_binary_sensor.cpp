#include "hacan_binary_sensor.h"

#include "esphome/core/log.h"

namespace esphome::hacan_esphome {

static const char *const TAG = "hacan.binary_sensor";

void HacanBinarySensor::setup() {
  if (source_ == nullptr || hacan_ == nullptr) {
    ESP_LOGE(TAG, "Source binary sensor or HACAN component is not configured");
    mark_failed();
    return;
  }
  source_->add_on_state_callback([this](bool state) {
    publish_state(state);
    if (!hacan_->publish_bool_state(endpoint_, state)) {
      ESP_LOGW(TAG, "Could not publish state for endpoint 0x%02X", endpoint_);
    }
  });
}

void HacanBinarySensor::dump_config() {
  ESP_LOGCONFIG(TAG, "HACAN binary sensor");
  LOG_BINARY_SENSOR("  ", "HACAN binary sensor", this);
  ESP_LOGCONFIG(TAG, "  Endpoint: 0x%02X", endpoint_);
}

}  // namespace esphome::hacan_esphome
