#pragma once

#include <array>
#include <optional>

#include "protocol_typed_value.h"

namespace hacan::protocol {
enum class StateQuality : std::uint8_t { kValid = 1U << 0U, kStale = 1U << 1U, kFault = 1U << 2U, kOverrange = 1U << 3U, kInitial = 1U << 4U };
class StatePayload { public: static std::optional<StatePayload> create(std::uint8_t, EndpointId, TypedValue, StateQuality); std::array<std::uint8_t, 8> encode() const; private: constexpr StatePayload(std::uint8_t s, EndpointId e, TypedValue v, StateQuality q): sequence_(s), endpoint_(e), value_(v), quality_(q) {} std::uint8_t sequence_; EndpointId endpoint_; TypedValue value_; StateQuality quality_; };
}  // namespace hacan::protocol
