#pragma once

#include <array>
#include <cstdint>
#include <optional>

namespace hacan::protocol {

enum class NodeCondition : std::uint8_t {
  kNormal = 0,
  kDegraded = 1U << 0U,
  kConfigurationRequired = 1U << 1U,
  kApplicationFault = 1U << 2U,
  kCanErrorPassive = 1U << 3U,
  kBusOffRecovery = 1U << 4U,
};

class HeartbeatPayload {
 public:
  [[nodiscard]] static std::optional<HeartbeatPayload> create(
      std::uint8_t sequence, NodeCondition condition, std::uint32_t boot_token,
      std::uint8_t tx_queue_load);
  [[nodiscard]] std::array<std::uint8_t, 8> encode() const;

 private:
  constexpr HeartbeatPayload(std::uint8_t sequence, NodeCondition condition,
                             std::uint32_t boot_token,
                             std::uint8_t tx_queue_load)
      : sequence_(sequence),
        condition_(condition),
        boot_token_(boot_token),
        tx_queue_load_(tx_queue_load) {}

  std::uint8_t sequence_;
  NodeCondition condition_;
  std::uint32_t boot_token_;
  std::uint8_t tx_queue_load_;
};

}  // namespace hacan::protocol
