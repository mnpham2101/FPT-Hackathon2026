cmake_minimum_required(VERSION 3.22)
project(ada_ecu CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Dependencies via FetchContent, exact pinned tags only (no floating branches).
# Deliberately NO Vanetza: the ADA ECU never touches UPER.
include(FetchContent)

FetchContent_Declare(nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.11.3
)
FetchContent_Declare(googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG v1.14.0
)
FetchContent_MakeAvailable(nlohmann_json googletest)

enable_testing()
include(GoogleTest)

# Helper for registering test executables: links nlohmann_json + gtest_main,
# exposes the shared fixture dir, and wires CTest discovery.
function(ada_add_test name)
  add_executable(${name} ${ARGN})
  target_link_libraries(${name} PRIVATE nlohmann_json::nlohmann_json GTest::gtest_main)
  target_compile_definitions(${name} PRIVATE ADA_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures")
  gtest_discover_tests(${name})
endfunction()

ada_add_test(ada_sanity_test tests/test_sanity.cpp)

# Contracts library — pure model code, no transport (3.0.4.2).
add_library(ada_contracts STATIC
  src/contracts/tracked_object.cpp
  src/contracts/r2_message.cpp
  src/contracts/r4_message.cpp
)
target_include_directories(ada_contracts PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(ada_contracts PUBLIC nlohmann_json::nlohmann_json)
target_compile_features(ada_contracts PUBLIC cxx_std_17)

ada_add_test(ada_r3_roundtrip_test tests/contracts/test_r3_roundtrip.cpp)
target_link_libraries(ada_r3_roundtrip_test PRIVATE ada_contracts)

ada_add_test(ada_r2_roundtrip_test tests/contracts/test_r2_roundtrip.cpp)
target_link_libraries(ada_r2_roundtrip_test PRIVATE ada_contracts)

ada_add_test(ada_r4_roundtrip_test tests/contracts/test_r4_roundtrip.cpp)
target_link_libraries(ada_r4_roundtrip_test PRIVATE ada_contracts)

ada_add_test(ada_r4_additive_version_test tests/contracts/test_r4_additive_version.cpp)
target_link_libraries(ada_r4_additive_version_test PRIVATE ada_contracts)

# Config loader — the node's only env reader (13.2.2.1).
add_library(ada_config STATIC src/config/config.cpp)
target_include_directories(ada_config PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_features(ada_config PUBLIC cxx_std_17)

ada_add_test(ada_config_test tests/config/test_config.cpp)
target_link_libraries(ada_config_test PRIVATE ada_config)
