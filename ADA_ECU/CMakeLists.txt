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

# UDP socket — the only socket-API holder in src/ (6.2.2.2).
add_library(ada_net STATIC src/net/udp_socket.cpp)
target_include_directories(ada_net PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_features(ada_net PUBLIC cxx_std_17)

ada_add_test(ada_udp_socket_test tests/net/test_udp_socket.cpp)
target_link_libraries(ada_udp_socket_test PRIVATE ada_net)

# R18 event log — the [EVT] JSONL writer (18.2.2.3).
add_library(ada_event_log STATIC src/log/event_log.cpp)
target_include_directories(ada_event_log PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(ada_event_log PUBLIC nlohmann_json::nlohmann_json)
target_compile_features(ada_event_log PUBLIC cxx_std_17)

ada_add_test(ada_event_log_test tests/log/test_event_log.cpp)
target_link_libraries(ada_event_log_test PRIVATE ada_event_log)

# Data parsers — wire → frozen R3 model (2.2.3.1, 3.2.3.2).
add_library(ada_parser STATIC
  src/parser/r2_parser.cpp
  src/parser/r3_parser.cpp
)
target_include_directories(ada_parser PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(ada_parser PUBLIC ada_contracts)
target_compile_features(ada_parser PUBLIC cxx_std_17)

ada_add_test(ada_r2_parser_test tests/parser/test_r2_parser.cpp)
target_link_libraries(ada_r2_parser_test PRIVATE ada_parser)

ada_add_test(ada_r3_parser_test tests/parser/test_r3_parser.cpp)
target_link_libraries(ada_r3_parser_test PRIVATE ada_parser)

# Parse-reject corpus — both parsers vs tests/fixtures/malformed/ (3.2.3.3).
ada_add_test(ada_parse_reject_corpus_test tests/parser/test_parse_reject_corpus.cpp)
target_link_libraries(ada_parse_reject_corpus_test PRIVATE ada_parser)

# Bounded input queue — two producers, one consumer, header-only (3.2.2.4).
find_package(Threads REQUIRED)
add_library(ada_input_queue INTERFACE)
target_include_directories(ada_input_queue INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_features(ada_input_queue INTERFACE cxx_std_17)

ada_add_test(ada_input_queue_test tests/observer/test_input_queue.cpp)
target_link_libraries(ada_input_queue_test PRIVATE ada_input_queue Threads::Threads)

# R3 track store — id → TrackedObject map, single writer (3.2.4.1).
add_library(ada_store STATIC src/store/track_store.cpp)
target_include_directories(ada_store PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(ada_store PUBLIC ada_contracts)
target_compile_features(ada_store PUBLIC cxx_std_17)

ada_add_test(ada_track_store_test tests/store/test_track_store.cpp)
target_link_libraries(ada_track_store_test PRIVATE ada_store)
