#include <doctest/doctest.h>

#include "protocol_payload_application.h"

namespace hacan::protocol {
namespace {

TEST_CASE("state payload encodes typed value and quality") {
  const TypedValue value{DataType::kUInt16, {0x34, 0x12, 0x00, 0x00}};
  const auto payload = StatePayload::create(0x21, EndpointId{0x01}, value,
                                            StateQuality::kValid);

  REQUIRE(payload.has_value());
  CHECK(payload->encode() ==
        std::array<std::uint8_t, 8>{0x21, 0x01, 0x04, 0x01, 0x34, 0x12, 0x00, 0x00});
}

TEST_CASE("command payload rejects reserved flag bits") {
  const TypedValue value{DataType::kBool, {0x01, 0x00, 0x00, 0x00}};

  CHECK_FALSE(CommandPayload::create(0x21, EndpointId{0x01}, value, 0x04));
}

TEST_CASE("ack payload encodes detail little endian") {
  const auto payload = AckPayload::create(0x21, EndpointId{0x01},
                                          MessageKind::kCommand,
                                          Status::kAccepted, 0x12345678);

  REQUIRE(payload.has_value());
  CHECK(payload->encode() ==
        std::array<std::uint8_t, 8>{0x21, 0x01, 0x0A, 0x00, 0x78, 0x56, 0x34, 0x12});
}

TEST_CASE("event payload rejects reserved flags") {
  const TypedValue value{DataType::kBool, {0x01, 0x00, 0x00, 0x00}};

  CHECK_FALSE(EventPayload::create(0x21, EndpointId{0x01}, value, 0x04));
}

TEST_CASE("request payload accepts endpoint information operation") {
  const auto payload = RequestPayload::create(0x21, EndpointId{0x01},
                                              RequestOperation::kGetEndpointInfo, 0);

  REQUIRE(payload.has_value());
  CHECK(payload->encode() ==
        std::array<std::uint8_t, 8>{0x21, 0x01, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00});
}

}  // namespace
}  // namespace hacan::protocol
