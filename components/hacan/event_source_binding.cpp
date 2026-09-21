#include "event_source_binding.h"

namespace esphome::hacan_esphome {

void EventSourceBinding::configure(::hacan::protocol::EntityId entity,
                                   ::hacan::protocol::ButtonEvent event,
                                   EventAction action, IEventActionTarget *target) {
  entity_ = entity;
  event_ = event;
  action_ = action;
  target_ = target;
}

void EventSourceBinding::event(::hacan::protocol::TypedValue value, std::uint8_t) {
  if (target_ == nullptr || value.type() != ::hacan::protocol::DataType::kEnum8 ||
      value.validate() != ::hacan::protocol::ValueError::kNone ||
      value.bytes()[0] != static_cast<std::uint8_t>(event_)) {
    return;
  }
  target_->apply_event_action(action_);
}

}  // namespace esphome::hacan_esphome
