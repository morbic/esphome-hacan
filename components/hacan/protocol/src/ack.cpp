#include <hacan/protocol/payloads/ack.h>

namespace hacan::protocol {

std::optional<AckPayload> AckPayload::create(std::uint8_t transaction,
                                              EndpointId endpoint,
                                              MessageKind original_kind,
                                              Status status,
                                              std::uint32_t detail) {
  if (static_cast<std::uint8_t>(status) > 7) return std::nullopt;
  return AckPayload{transaction, endpoint, original_kind, status, detail};
}

std::array<std::uint8_t, 8> AckPayload::encode() const {
  return {transaction_, endpoint_.value(), static_cast<std::uint8_t>(original_kind_),
          static_cast<std::uint8_t>(status_), static_cast<std::uint8_t>(detail_),
          static_cast<std::uint8_t>(detail_ >> 8),
          static_cast<std::uint8_t>(detail_ >> 16),
          static_cast<std::uint8_t>(detail_ >> 24)};
}

}  // namespace hacan::protocol
