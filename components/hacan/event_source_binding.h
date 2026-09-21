#pragma once

#include <cstdint>

#include "protocol_button_event.h"
#include "protocol_node_runtime.h"

namespace esphome::hacan_esphome {

enum class EventAction : std::uint8_t {
  kToggle = 0,
  kTurnOn = 1,
  kTurnOff = 2,
};

class IEventActionTarget {
 public:
  virtual ~IEventActionTarget() = default;
  virtual void apply_event_action(EventAction action) = 0;
};

class EventSourceBinding final : public ::hacan::protocol::IEventListener {
 public:
  void configure(::hacan::protocol::EntityId entity, ::hacan::protocol::ButtonEvent event,
                 EventAction action, IEventActionTarget *target);
  [[nodiscard]] ::hacan::protocol::EntityId observed_entity() const override { return entity_; }
  void event(::hacan::protocol::TypedValue value, std::uint8_t flags) override;

 protected:
  ::hacan::protocol::EntityId entity_{0};
  ::hacan::protocol::ButtonEvent event_{::hacan::protocol::ButtonEvent::kPress};
  EventAction action_{EventAction::kToggle};
  IEventActionTarget *target_{nullptr};
};

}  // namespace esphome::hacan_esphome
