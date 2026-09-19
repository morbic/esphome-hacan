#include <doctest/doctest.h>

#include <hacan/protocol/frame_codec.h>

namespace hacan::protocol {
namespace {

TEST_CASE("frame codec rejects a standard CAN frame") {
  const RawCanFrame raw{0x129FF950U, false, 8, {}};
  const auto decoded = FrameCodec::decode(raw);

  CHECK(std::holds_alternative<DecodeError>(decoded));
  CHECK(std::get<DecodeError>(decoded) == DecodeError::kNotExtended);
}

TEST_CASE("frame codec preserves a valid entity claim") {
  const RawCanFrame raw{0x129FF950U, true, 8,
                        {0x21, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x00}};
  const auto decoded = FrameCodec::decode(raw);

  REQUIRE(std::holds_alternative<ProtocolFrame>(decoded));
  const auto encoded = FrameCodec::encode(std::get<ProtocolFrame>(decoded));
  REQUIRE(std::holds_alternative<RawCanFrame>(encoded));
  CHECK(std::get<RawCanFrame>(encoded).can_id == raw.can_id);
  CHECK(std::get<RawCanFrame>(encoded).data == raw.data);
}

TEST_CASE("frame codec distinguishes an unknown kind from a malformed frame") {
  const RawCanFrame raw{(4U << 26U) | (0x1FU << 21U) | (0x1FFU << 12U) |
                            (0x001U << 3U),
                        true, 8, {}};

  const auto decoded = FrameCodec::decode(raw);
  CHECK(std::holds_alternative<DecodeError>(decoded));
  CHECK(std::get<DecodeError>(decoded) == DecodeError::kUnknownKind);
}

TEST_CASE("frame codec rejects a broadcast command") {
  const auto identifier = CanIdentifier::create(
      Priority::kControl, MessageKind::kCommand, NodeAddress{0x1FF},
      NodeAddress{0x001}, 0);
  REQUIRE(identifier.has_value());

  const RawCanFrame raw{identifier->to_raw(), true, 8,
                        {0, 1, 1, 0, 1, 0, 0, 0}};
  const auto decoded = FrameCodec::decode(raw);
  REQUIRE(std::holds_alternative<DecodeError>(decoded));
  CHECK(std::get<DecodeError>(decoded) == DecodeError::kInvalidAddressing);
}

TEST_CASE("frame codec rejects a heartbeat sent by an unassigned node") {
  const auto identifier = CanIdentifier::create(
      Priority::kManagement, MessageKind::kHeartbeat, NodeAddress{0x1FF},
      NodeAddress{0x000}, 0);
  REQUIRE(identifier.has_value());

  const RawCanFrame raw{identifier->to_raw(), true, 8,
                        {1, 0, 1, 0, 0, 0, 0, 0}};
  const auto decoded = FrameCodec::decode(raw);
  REQUIRE(std::holds_alternative<DecodeError>(decoded));
  CHECK(std::get<DecodeError>(decoded) == DecodeError::kInvalidAddressing);
}

TEST_CASE("frame codec accepts an unassigned discovery request") {
  const auto identifier = CanIdentifier::create(
      Priority::kManagement, MessageKind::kDiscoveryRequest, NodeAddress{0x1FF},
      NodeAddress{0x000}, 0);
  REQUIRE(identifier.has_value());
  const RawCanFrame raw{identifier->to_raw(), true, 8,
                        {1, 2, 0xE8, 3, 0, 0, 0, 0}};
  CHECK(std::holds_alternative<ProtocolFrame>(FrameCodec::decode(raw)));
}

}  // namespace
}  // namespace hacan::protocol
