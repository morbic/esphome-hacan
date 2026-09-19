#include <doctest/doctest.h>

#include <hacan/protocol/version.h>

TEST_CASE("protocol library exposes version one constants") {
  CHECK(hacan::protocol::kProtocolMajor == 1);
  CHECK(hacan::protocol::kProtocolMinor == 0);
}
