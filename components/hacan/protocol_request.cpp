#include "protocol_payload_request.h"

namespace hacan::protocol {
namespace {
bool valid_endpoint(EndpointId endpoint) { return endpoint.value() != 0 && endpoint.value() != 0xFF; }
}  // namespace
std::optional<RequestPayload> RequestPayload::create(std::uint8_t transaction, EndpointId endpoint, RequestOperation operation, std::uint8_t argument) {
  const auto value = static_cast<std::uint8_t>(operation);
  if (value < 1 || value > 3 || (operation == RequestOperation::kGetNodeInfo && endpoint.value() != 0) || (operation == RequestOperation::kGetEndpointInfo && !valid_endpoint(endpoint))) return std::nullopt;
  return RequestPayload{transaction, endpoint, operation, argument};
}
std::array<std::uint8_t, 8> RequestPayload::encode() const {
  return {transaction_, endpoint_.value(), static_cast<std::uint8_t>(operation_), argument_, 0, 0, 0, 0};
}
}  // namespace hacan::protocol
