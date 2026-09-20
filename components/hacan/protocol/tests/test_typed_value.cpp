#include <doctest/doctest.h>

#include "protocol_typed_value.h"

namespace hacan::protocol {
namespace {

TEST_CASE("typed value rejects boolean values other than zero or one") {
  const TypedValue value{DataType::kBool, {0x02, 0x00, 0x00, 0x00}};

  CHECK(value.validate() == ValueError::kInvalidBoolean);
}

TEST_CASE("typed value rejects nonzero unused bytes") {
  const TypedValue value{DataType::kUInt16, {0x34, 0x12, 0x01, 0x00}};

  CHECK(value.validate() == ValueError::kNonZeroUnusedBytes);
}

TEST_CASE("typed value accepts little endian unsigned sixteen bit values") {
  const TypedValue value{DataType::kUInt16, {0x34, 0x12, 0x00, 0x00}};

  CHECK(value.validate() == ValueError::kNone);
}

}  // namespace
}  // namespace hacan::protocol
