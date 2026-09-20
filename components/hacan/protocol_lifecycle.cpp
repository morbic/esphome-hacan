#include "protocol_payload_lifecycle.h"

namespace hacan::protocol {

std::optional<HeartbeatPayload> HeartbeatPayload::create(
    std::uint8_t sequence, NodeCondition condition, std::uint32_t boot_token,
    std::uint8_t tx_queue_load) {
  const auto flags = static_cast<std::uint8_t>(condition);
  if (boot_token == 0 || (flags & 0xE0U) != 0 ||
      (tx_queue_load > 100 && tx_queue_load != 0xFFU)) {
    return std::nullopt;
  }
  return HeartbeatPayload{sequence, condition, boot_token, tx_queue_load};
}

std::array<std::uint8_t, 8> HeartbeatPayload::encode() const {
  return {sequence_, static_cast<std::uint8_t>(condition_),
          static_cast<std::uint8_t>(boot_token_),
          static_cast<std::uint8_t>(boot_token_ >> 8U),
          static_cast<std::uint8_t>(boot_token_ >> 16U),
          static_cast<std::uint8_t>(boot_token_ >> 24U), tx_queue_load_, 0};
}

}  // namespace hacan::protocol
