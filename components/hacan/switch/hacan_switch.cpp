#include "hacan_switch.h"

#include "esphome/core/log.h"

namespace esphome::hacan_esphome {

static const char *const TAG = "hacan.switch";

void HacanSwitch::write_state(bool state) { apply_state(state); }

::hacan::protocol::Status HacanSwitch::command(::hacan::protocol::TypedValue value) {
  if (value.type() != ::hacan::protocol::DataType::kBool ||
      value.validate() != ::hacan::protocol::ValueError::kNone) {
    return ::hacan::protocol::Status::kInvalidValue;
  }
  apply_state(value.bytes()[0] != 0);
  return ::hacan::protocol::Status::kAccepted;
}

void HacanSwitch::apply_state(bool state) {
  if (output_ == nullptr || hacan_ == nullptr) {
    ESP_LOGE(TAG, "Output or HACAN component is not configured");
    return;
  }
  output_->set_state(state);
  publish_state(state);
  if (!hacan_->publish_bool_state(endpoint_, state)) {
    ESP_LOGW(TAG, "Could not publish state for endpoint 0x%02X", endpoint_);
  }
}

void HacanSwitch::dump_config() {
  ESP_LOGCONFIG(TAG, "HACAN switch");
  LOG_SWITCH("  ", "HACAN switch", this);
  ESP_LOGCONFIG(TAG, "  Endpoint: 0x%02X", endpoint_);
}

}  // namespace esphome::hacan_esphome
