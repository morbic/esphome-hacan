#pragma once
#include <array>
#include <optional>
#include "protocol_typed_value.h"
namespace hacan::protocol { class EventPayload { public: static std::optional<EventPayload> create(std::uint8_t, EndpointId, TypedValue, std::uint8_t); std::array<std::uint8_t, 8> encode() const; private: constexpr EventPayload(std::uint8_t s, EndpointId e, TypedValue v, std::uint8_t f): sequence_(s), endpoint_(e), value_(v), flags_(f) {} std::uint8_t sequence_; EndpointId endpoint_; TypedValue value_; std::uint8_t flags_; }; }
