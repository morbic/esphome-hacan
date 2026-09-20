#pragma once
#include <array>
#include <optional>
#include "protocol_types.h"
namespace hacan::protocol { enum class RequestOperation : std::uint8_t { kGetState=1, kGetNodeInfo=2, kGetEndpointInfo=3 }; class RequestPayload { public: static std::optional<RequestPayload> create(std::uint8_t, EndpointId, RequestOperation, std::uint8_t); std::array<std::uint8_t, 8> encode() const; private: constexpr RequestPayload(std::uint8_t t, EndpointId e, RequestOperation o, std::uint8_t a): transaction_(t), endpoint_(e), operation_(o), argument_(a) {} std::uint8_t transaction_; EndpointId endpoint_; RequestOperation operation_; std::uint8_t argument_; }; }
