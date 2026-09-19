include(FetchContent)

FetchContent_Declare(
  doctest
  GIT_REPOSITORY https://github.com/doctest/doctest.git
  GIT_TAG 2d0a9359a60c51affe2a9bebb1be1dca47868151
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(doctest)

add_executable(hacan_protocol_tests
  test_main.cpp
  test_build_contract.cpp
  test_can_identifier.cpp
  test_typed_value.cpp
  test_entity_payloads.cpp
  test_lifecycle_payloads.cpp
  test_application_payloads.cpp
  test_frame_codec.cpp
  test_message_kind_coverage.cpp
  test_node_runtime.cpp
)
target_link_libraries(hacan_protocol_tests PRIVATE hacan_protocol doctest::doctest)

add_test(NAME hacan_protocol_tests COMMAND hacan_protocol_tests)
