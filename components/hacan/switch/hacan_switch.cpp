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

void HacanSwitch::state(::hacan::protocol::TypedValue value,
                        ::hacan::protocol::StateQuality) {
  if (value.type() != ::hacan::protocol::DataType::kBool ||
      value.validate() != ::hacan::protocol::ValueError::kNone) {
    ESP_LOGW(TAG, "Ignoring non-boolean state for endpoint 0x%02X", endpoint_);
    return;
  }
  apply_state(value.bytes()[0] != 0);
}

::hacan::protocol::IEventListener *HacanSwitch::add_event_source(
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

void HacanSwitch::apply_event_action(EventAction action) {
  switch (action) {
    case EventAction::kToggle: apply_state(!applied_state_.value_or(false)); break;
    case EventAction::kTurnOn: apply_state(true); break;
    case EventAction::kTurnOff: apply_state(false); break;
  }
}

void HacanSwitch::apply_state(bool state) {
  if (output_ == nullptr || hacan_ == nullptr) {
    ESP_LOGE(TAG, "Output or HACAN component is not configured");
    return;
  }
  if (applied_state_ && *applied_state_ == state) return;
  applied_state_ = state;
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
  if (state_source_entity_ != 0) {
    ESP_LOGCONFIG(TAG, "  State source entity: 0x%08X",
                  static_cast<unsigned>(state_source_entity_));
  }
}

}  // namespace esphome::hacan_esphome
