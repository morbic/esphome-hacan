#include <hacan/protocol/payloads/entities.h>

namespace hacan::protocol {
namespace {

void write_u16_le(std::array<std::uint8_t, 8>& bytes, std::uint8_t offset,
                  std::uint16_t value) {
  bytes[offset] = static_cast<std::uint8_t>(value);
  bytes[offset + 1] = static_cast<std::uint8_t>(value >> 8U);
}

void write_u32_le(std::array<std::uint8_t, 8>& bytes, std::uint8_t offset,
                  std::uint32_t value) {
  bytes[offset] = static_cast<std::uint8_t>(value);
  bytes[offset + 1] = static_cast<std::uint8_t>(value >> 8U);
  bytes[offset + 2] = static_cast<std::uint8_t>(value >> 16U);
  bytes[offset + 3] = static_cast<std::uint8_t>(value >> 24U);
}

}  // namespace

std::optional<EntityClaimPayload> EntityClaimPayload::create(
    std::uint8_t sequence, EndpointId endpoint, EntityId entity) {
  if (endpoint.value() == 0 || endpoint.value() == 0xFF || entity.value() == 0) {
    return std::nullopt;
  }
  return EntityClaimPayload{sequence, endpoint, entity};
}

std::array<std::uint8_t, 8> EntityClaimPayload::encode() const {
  std::array<std::uint8_t, 8> bytes{};
  bytes[0] = sequence_;
  bytes[1] = endpoint_.value();
  write_u32_le(bytes, 2, entity_.value());
  return bytes;
}

std::optional<EntityResolvePayload> EntityResolvePayload::create(
    std::uint8_t transaction, EntityId entity, std::uint16_t window_ms) {
  if (entity.value() == 0 || window_ms < 1000 || window_ms > 10000) {
    return std::nullopt;
  }
  return EntityResolvePayload{transaction, entity, window_ms};
}

std::array<std::uint8_t, 8> EntityResolvePayload::encode() const {
  std::array<std::uint8_t, 8> bytes{};
  bytes[0] = transaction_;
  write_u32_le(bytes, 2, entity_.value());
  write_u16_le(bytes, 6, window_ms_);
  return bytes;
}

}  // namespace hacan::protocol
