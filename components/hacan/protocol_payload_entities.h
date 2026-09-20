#pragma once

#include <array>
#include <cstdint>
#include <optional>

#include "protocol_types.h"

namespace hacan::protocol {

class EntityClaimPayload {
 public:
  [[nodiscard]] static std::optional<EntityClaimPayload> create(
      std::uint8_t sequence, EndpointId endpoint, EntityId entity);

  [[nodiscard]] std::array<std::uint8_t, 8> encode() const;
  [[nodiscard]] constexpr std::uint8_t sequence() const { return sequence_; }
  [[nodiscard]] constexpr EndpointId endpoint() const { return endpoint_; }
  [[nodiscard]] constexpr EntityId entity() const { return entity_; }

 private:
  constexpr EntityClaimPayload(std::uint8_t sequence, EndpointId endpoint,
                               EntityId entity)
      : sequence_(sequence), endpoint_(endpoint), entity_(entity) {}

  std::uint8_t sequence_;
  EndpointId endpoint_;
  EntityId entity_;
};

class EntityResolvePayload {
 public:
  [[nodiscard]] static std::optional<EntityResolvePayload> create(
      std::uint8_t transaction, EntityId entity, std::uint16_t window_ms);

  [[nodiscard]] std::array<std::uint8_t, 8> encode() const;

 private:
  constexpr EntityResolvePayload(std::uint8_t transaction, EntityId entity,
                                 std::uint16_t window_ms)
      : transaction_(transaction), entity_(entity), window_ms_(window_ms) {}

  std::uint8_t transaction_;
  EntityId entity_;
  std::uint16_t window_ms_;
};

}  // namespace hacan::protocol
