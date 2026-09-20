#pragma once
#include <array>
#include <optional>
#include "protocol_types.h"
namespace hacan::protocol { class AckPayload { public: static std::optional<AckPayload> create(std::uint8_t, EndpointId, MessageKind, Status, std::uint32_t); std::array<std::uint8_t, 8> encode() const; private: constexpr AckPayload(std::uint8_t t, EndpointId e, MessageKind k, Status s, std::uint32_t d): transaction_(t), endpoint_(e), original_kind_(k), status_(s), detail_(d) {} std::uint8_t transaction_; EndpointId endpoint_; MessageKind original_kind_; Status status_; std::uint32_t detail_; }; }
