#include <doctest/doctest.h>

#include "protocol_version.h"

TEST_CASE("protocol library exposes version one constants") {
  CHECK(hacan::protocol::kProtocolMajor == 1);
  CHECK(hacan::protocol::kProtocolMinor == 0);
}
