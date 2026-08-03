cmake_minimum_required(VERSION 3.22)
project(ada_ecu CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ADA_BUILD_CONTRACT_TESTS "Build Phase 0 ADA contract binding tests" ON)

include(FetchContent)

find_package(nlohmann_json QUIET)
if(NOT nlohmann_json_FOUND)
  FetchContent_Declare(nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
  )
  FetchContent_MakeAvailable(nlohmann_json)
endif()

enable_testing()
find_package(Threads REQUIRED)

add_library(ada_core
  src/config.cpp
  src/detector_jsonl_ingest.cpp
  src/detector_process.cpp
  src/event_logger.cpp
  src/r2_mapper.cpp
  src/r3_mapper.cpp
  src/risk_assessor.cpp
  src/risk_registry.cpp
  src/scene_composer.cpp
  src/track_store.cpp
  src/udp_r2_receiver.cpp
  src/udp_r4_sender.cpp
  src/v2x_r2_ingest.cpp
  src/warning_builder.cpp
)
target_include_directories(ada_core PUBLIC include)
target_link_libraries(ada_core PUBLIC nlohmann_json::nlohmann_json Threads::Threads)

add_executable(ada_ecu src/main.cpp)
target_link_libraries(ada_ecu PRIVATE ada_core)

add_executable(ada_core_tests tests/track_store_tests.cpp)
target_link_libraries(ada_core_tests PRIVATE ada_core)
target_compile_definitions(ada_core_tests PRIVATE ADA_TESTDATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/testdata")
add_test(NAME ada_core_tests COMMAND ada_core_tests)

# Contracts library — pure model code, no transport (3.0.4.2).
add_library(ada_contracts STATIC
  src/contracts/tracked_object.cpp
  src/contracts/r2_message.cpp
  src/contracts/r4_message.cpp
)
target_include_directories(ada_contracts PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(ada_contracts PUBLIC nlohmann_json::nlohmann_json)
target_compile_features(ada_contracts PUBLIC cxx_std_17)
target_include_directories(ada_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(ada_core PUBLIC ada_contracts)

if(ADA_BUILD_CONTRACT_TESTS)
  FetchContent_Declare(googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
  )
  FetchContent_MakeAvailable(googletest)

  include(GoogleTest)

  function(ada_add_contract_test name)
    add_executable(${name} ${ARGN})
    target_link_libraries(${name} PRIVATE ada_contracts GTest::gtest_main)
    target_compile_definitions(${name} PRIVATE ADA_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures")
    gtest_discover_tests(${name})
  endfunction()

  ada_add_contract_test(ada_sanity_test tests/test_sanity.cpp)
  ada_add_contract_test(ada_r3_roundtrip_test tests/contracts/test_r3_roundtrip.cpp)
  ada_add_contract_test(ada_r2_roundtrip_test tests/contracts/test_r2_roundtrip.cpp)
  ada_add_contract_test(ada_r4_roundtrip_test tests/contracts/test_r4_roundtrip.cpp)
  ada_add_contract_test(ada_r4_additive_version_test tests/contracts/test_r4_additive_version.cpp)
endif()
