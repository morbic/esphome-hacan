#include <hacan/protocol/payloads/event.h>

namespace hacan::protocol {
namespace {
bool valid_endpoint(EndpointId endpoint) { return endpoint.value() != 0 && endpoint.value() != 0xFF; }
void write_value(std::array<std::uint8_t, 8>& bytes, const TypedValue& value) {
  bytes[2] = static_cast<std::uint8_t>(value.type());
  for (std::uint8_t index = 0; index < 4; ++index) bytes[4 + index] = value.bytes()[index];
}
}  // namespace
std::optional<EventPayload> EventPayload::create(std::uint8_t sequence, EndpointId endpoint, TypedValue value, std::uint8_t flags) {
  if (!valid_endpoint(endpoint) || value.validate() != ValueError::kNone || (flags & 0xFCU) != 0) return std::nullopt;
  return EventPayload{sequence, endpoint, value, flags};
}
std::array<std::uint8_t, 8> EventPayload::encode() const {
  std::array<std::uint8_t, 8> bytes{};
  bytes[0] = sequence_; bytes[1] = endpoint_.value(); bytes[3] = flags_;
  write_value(bytes, value_);
  return bytes;
}
}  // namespace hacan::protocol
