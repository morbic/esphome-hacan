cmake_minimum_required(VERSION 3.20)

project(hacan_protocol LANGUAGES CXX)

add_library(hacan_protocol
  src/can_identifier.cpp
  src/ack.cpp
  src/command.cpp
  src/event.cpp
  src/request.cpp
  src/state.cpp
  src/frame_codec.cpp
  src/entities.cpp
  src/lifecycle.cpp
  src/node_runtime.cpp
  src/typed_value.cpp
)
target_compile_features(hacan_protocol PUBLIC cxx_std_17)
target_include_directories(hacan_protocol PUBLIC include)

enable_testing()
add_subdirectory(tests)
