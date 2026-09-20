cmake_minimum_required(VERSION 3.20)

project(hacan_protocol LANGUAGES CXX)

add_library(hacan_protocol
  ../protocol_can_identifier.cpp
  ../protocol_ack.cpp
  ../protocol_command.cpp
  ../protocol_event.cpp
  ../protocol_request.cpp
  ../protocol_state.cpp
  ../protocol_frame_codec.cpp
  ../protocol_entities.cpp
  ../protocol_lifecycle.cpp
  ../protocol_node_runtime.cpp
  ../protocol_typed_value.cpp
)
target_compile_features(hacan_protocol PUBLIC cxx_std_17)
target_include_directories(hacan_protocol PUBLIC ..)

enable_testing()
add_subdirectory(tests)
