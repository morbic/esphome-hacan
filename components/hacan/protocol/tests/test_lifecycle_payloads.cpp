#include <doctest/doctest.h>

#include <hacan/protocol/payloads/lifecycle.h>

namespace hacan::protocol {
namespace {

TEST_CASE("heartbeat encodes a normal node condition and little endian boot token") {
  const auto payload = HeartbeatPayload::create(0x21, NodeCondition::kNormal,
                                                0x12345678, 42);

  REQUIRE(payload.has_value());
  CHECK(payload->encode() ==
        std::array<std::uint8_t, 8>{0x21, 0x00, 0x78, 0x56, 0x34, 0x12, 42, 0x00});
}

TEST_CASE("heartbeat rejects a zero boot token") {
  CHECK_FALSE(HeartbeatPayload::create(0x21, NodeCondition::kNormal, 0, 42));
}

}  // namespace
}  // namespace hacan::protocol
