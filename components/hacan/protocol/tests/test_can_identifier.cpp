#include <doctest/doctest.h>

#include <hacan/protocol/can_identifier.h>

namespace hacan::protocol {
namespace {

TEST_CASE("identifier encodes the entity claim protocol vector") {
  const auto identifier = CanIdentifier::create(
      Priority::kManagement, MessageKind::kEntityClaim,
      NodeAddress{0x1FF}, NodeAddress{0x12A}, 0);

  REQUIRE(identifier.has_value());
  CHECK(identifier->to_raw() == 0x129FF950U);
}

TEST_CASE("identifier decodes all wire fields") {
  const auto identifier = CanIdentifier::from_raw(0x12BFF008U);

  REQUIRE(identifier.has_value());
  CHECK(identifier->priority() == Priority::kManagement);
  CHECK(identifier->kind() == MessageKind::kEntityResolve);
  CHECK(identifier->destination().value() == 0x1FF);
  CHECK(identifier->source().value() == 0x001);
  CHECK(identifier->hop() == 0);
}

TEST_CASE("identifier rejects values wider than its wire fields") {
  CHECK_FALSE(CanIdentifier::create(
      Priority::kManagement, MessageKind::kEntityClaim,
      NodeAddress{0x200}, NodeAddress{0x12A}, 0));
  CHECK_FALSE(CanIdentifier::create(
      Priority::kManagement, MessageKind::kEntityClaim,
      NodeAddress{0x1FF}, NodeAddress{0x12A}, 8));
}

}  // namespace
}  // namespace hacan::protocol
