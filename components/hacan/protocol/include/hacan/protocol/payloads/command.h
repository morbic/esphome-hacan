#pragma once
#include <array>
#include <optional>
#include <hacan/protocol/typed_value.h>
namespace hacan::protocol { class CommandPayload { public: static std::optional<CommandPayload> create(std::uint8_t, EndpointId, TypedValue, std::uint8_t); std::array<std::uint8_t, 8> encode() const; private: constexpr CommandPayload(std::uint8_t t, EndpointId e, TypedValue v, std::uint8_t f): transaction_(t), endpoint_(e), value_(v), flags_(f) {} std::uint8_t transaction_; EndpointId endpoint_; TypedValue value_; std::uint8_t flags_; }; }
