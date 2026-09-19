#pragma once

#include <cstdint>

namespace hacan::protocol {

enum class Priority : std::uint8_t {
  kEmergency = 0,
  kControl = 1,
  kEvent = 2,
  kStateQuery = 3,
  kManagement = 4,
  kDiagnostic = 5,
  kBulkReserved = 6,
  kBackgroundReserved = 7,
};

enum class MessageKind : std::uint8_t {
  kProtocolError = 0x00,
  kAnnounce = 0x01,
  kHeartbeat = 0x02,
  kDiscoveryRequest = 0x03,
  kDiscoveryResponse = 0x04,
  kAddressClaim = 0x05,
  kAddressAssign = 0x06,
  kAddressConflict = 0x07,
  kState = 0x08,
  kEvent = 0x09,
  kCommand = 0x0A,
  kRequest = 0x0B,
  kResponse = 0x0C,
  kAck = 0x0D,
  kConfigGet = 0x0E,
  kConfigSet = 0x0F,
  kConfigValue = 0x10,
  kDiagnosticRequest = 0x11,
  kDiagnosticResponse = 0x12,
  kTimeSync = 0x13,
  kEntityClaim = 0x14,
  kEntityResolve = 0x15,
};

enum class DataType : std::uint8_t {
  kNull = 0x00,
  kBool = 0x01,
  kUInt8 = 0x02,
  kInt8 = 0x03,
  kUInt16 = 0x04,
  kInt16 = 0x05,
  kUInt32 = 0x06,
  kInt32 = 0x07,
  kFloat32 = 0x08,
  kFixed16_16 = 0x09,
  kEnum8 = 0x0A,
  kBitset32 = 0x0B,
  kInvalid = 0xFF,
};

enum class Status : std::uint8_t {
  kAccepted = 0x00,
  kBusy = 0x01,
  kUnsupported = 0x02,
  kInvalidValue = 0x03,
  kDenied = 0x04,
  kFault = 0x05,
  kConflict = 0x06,
  kInternalError = 0x07,
};

class NodeAddress {
 public:
  explicit constexpr NodeAddress(std::uint16_t value) : value_(value) {}

  [[nodiscard]] constexpr std::uint16_t value() const { return value_; }

 private:
  std::uint16_t value_;
};

class EndpointId {
 public:
  explicit constexpr EndpointId(std::uint8_t value) : value_(value) {}

  [[nodiscard]] constexpr std::uint8_t value() const { return value_; }

 private:
  std::uint8_t value_;
};

class EntityId {
 public:
  explicit constexpr EntityId(std::uint32_t value) : value_(value) {}

  [[nodiscard]] constexpr std::uint32_t value() const { return value_; }

 private:
  std::uint32_t value_;
};

}  // namespace hacan::protocol
