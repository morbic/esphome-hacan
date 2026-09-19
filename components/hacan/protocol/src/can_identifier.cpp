#include <hacan/protocol/can_identifier.h>

namespace hacan::protocol {
namespace {

constexpr std::uint32_t kIdentifierMask = 0x1FFFFFFFU;
constexpr std::uint16_t kAddressMask = 0x01FFU;
constexpr std::uint8_t kHopMask = 0x07U;

}  // namespace

std::optional<CanIdentifier> CanIdentifier::create(
    Priority priority, MessageKind kind, NodeAddress destination,
    NodeAddress source, std::uint8_t hop) {
  if (destination.value() > kAddressMask || source.value() > kAddressMask ||
      hop > kHopMask) {
    return std::nullopt;
  }

  return CanIdentifier{priority, kind, destination, source, hop};
}

std::optional<CanIdentifier> CanIdentifier::from_raw(std::uint32_t raw) {
  if (raw > kIdentifierMask) {
    return std::nullopt;
  }

  const auto priority = static_cast<Priority>((raw >> 26U) & 0x07U);
  const auto kind = static_cast<MessageKind>((raw >> 21U) & 0x1FU);
  const auto destination = NodeAddress{static_cast<std::uint16_t>((raw >> 12U) & kAddressMask)};
  const auto source = NodeAddress{static_cast<std::uint16_t>((raw >> 3U) & kAddressMask)};
  const auto hop = static_cast<std::uint8_t>(raw & kHopMask);

  return CanIdentifier{priority, kind, destination, source, hop};
}

std::uint32_t CanIdentifier::to_raw() const {
  return (static_cast<std::uint32_t>(priority_) << 26U) |
         (static_cast<std::uint32_t>(kind_) << 21U) |
         (static_cast<std::uint32_t>(destination_.value()) << 12U) |
         (static_cast<std::uint32_t>(source_.value()) << 3U) | hop_;
}

}  // namespace hacan::protocol
