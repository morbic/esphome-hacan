#include <doctest/doctest.h>

#include <hacan/protocol/payloads/entities.h>

namespace hacan::protocol {
namespace {

TEST_CASE("entity claim encodes entity id little endian") {
  const auto payload =
      EntityClaimPayload::create(0x21, EndpointId{0x01}, EntityId{0x01010001});

  REQUIRE(payload.has_value());
  CHECK(payload->encode() ==
        std::array<std::uint8_t, 8>{0x21, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x00});
}

TEST_CASE("entity claim rejects endpoint zero and entity zero") {
  CHECK_FALSE(EntityClaimPayload::create(0x21, EndpointId{0x00}, EntityId{0x01010001}));
  CHECK_FALSE(EntityClaimPayload::create(0x21, EndpointId{0x01}, EntityId{0x00000000}));
}

TEST_CASE("entity resolve rejects response windows outside the protocol range") {
  CHECK_FALSE(EntityResolvePayload::create(0x21, EntityId{0x01010001}, 999));
  CHECK_FALSE(EntityResolvePayload::create(0x21, EntityId{0x01010001}, 10001));
}

}  // namespace
}  // namespace hacan::protocol
