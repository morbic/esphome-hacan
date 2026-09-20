#pragma once

#include <cstdint>
#include <optional>

#include "protocol_types.h"

namespace hacan::protocol {

class CanIdentifier {
 public:
  [[nodiscard]] static std::optional<CanIdentifier> create(
      Priority priority, MessageKind kind, NodeAddress destination,
      NodeAddress source, std::uint8_t hop);
  [[nodiscard]] static std::optional<CanIdentifier> from_raw(std::uint32_t raw);

  [[nodiscard]] constexpr Priority priority() const { return priority_; }
  [[nodiscard]] constexpr MessageKind kind() const { return kind_; }
  [[nodiscard]] constexpr NodeAddress destination() const { return destination_; }
  [[nodiscard]] constexpr NodeAddress source() const { return source_; }
  [[nodiscard]] constexpr std::uint8_t hop() const { return hop_; }
  [[nodiscard]] std::uint32_t to_raw() const;

 private:
  constexpr CanIdentifier(Priority priority, MessageKind kind,
                          NodeAddress destination, NodeAddress source,
                          std::uint8_t hop)
      : priority_(priority),
        kind_(kind),
        destination_(destination),
        source_(source),
        hop_(hop) {}

  Priority priority_;
  MessageKind kind_;
  NodeAddress destination_;
  NodeAddress source_;
  std::uint8_t hop_;
};

}  // namespace hacan::protocol
