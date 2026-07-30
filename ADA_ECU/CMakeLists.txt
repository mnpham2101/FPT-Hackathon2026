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

# Later subtasks add here: contracts library + test targets for 3.0.4.2, 2.0.3.2, 4.0.4.3, 4.0.4.4.
