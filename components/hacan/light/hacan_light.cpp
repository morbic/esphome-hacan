#include "hacan_light.h"

#include "esphome/core/log.h"

namespace esphome::hacan_esphome {

static const char *const TAG = "hacan.light";

light::LightTraits HacanLightOutput::get_traits() {
  auto traits = light::LightTraits();
  traits.set_supported_color_modes({light::ColorMode::ON_OFF});
  return traits;
}

void HacanLightOutput::write_state(light::LightState *state) {
  if (output_ == nullptr || hacan_ == nullptr) {
    ESP_LOGE(TAG, "Output or HACAN component is not configured");
    return;
  }
  bool value = false;
  state->current_values_as_binary(&value);
  output_->set_state(value);
  if (!hacan_->publish_bool_state(endpoint_, value)) {
    ESP_LOGW(TAG, "Could not publish state for endpoint 0x%02X", endpoint_);
  }
}

::hacan::protocol::Status HacanLightOutput::command(::hacan::protocol::TypedValue value) {
  if (value.type() != ::hacan::protocol::DataType::kBool ||
      value.validate() != ::hacan::protocol::ValueError::kNone) {
    return ::hacan::protocol::Status::kInvalidValue;
  }
  if (state_ == nullptr) return ::hacan::protocol::Status::kFault;
  if (value.bytes()[0] == 0) {
    state_->turn_off().perform();
  } else {
    state_->turn_on().perform();
  }
  return ::hacan::protocol::Status::kAccepted;
}

::hacan::protocol::IEventListener *HacanLightOutput::add_event_source(
    uint32_t entity, uint8_t event, uint8_t action) {
  if (event_source_count_ == event_sources_.size()) {
    ESP_LOGE(TAG, "Too many event sources for endpoint 0x%02X", endpoint_);
    return nullptr;
  }
  auto &binding = event_sources_[event_source_count_++];
  binding.configure(::hacan::protocol::EntityId{entity},
                    static_cast<::hacan::protocol::ButtonEvent>(event),
                    static_cast<EventAction>(action), this);
  return &binding;
}

void HacanLightOutput::apply_event_action(EventAction action) {
  if (state_ == nullptr) return;
  switch (action) {
    case EventAction::kToggle: state_->toggle().perform(); break;
    case EventAction::kTurnOn: state_->turn_on().perform(); break;
    case EventAction::kTurnOff: state_->turn_off().perform(); break;
  }
}

void HacanLightOutput::dump_config() {
  ESP_LOGCONFIG(TAG, "HACAN binary light");
  ESP_LOGCONFIG(TAG, "  Endpoint: 0x%02X", endpoint_);
}

}  // namespace esphome::hacan_esphome
