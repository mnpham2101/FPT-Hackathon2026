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

# R3 track store — id → TrackedObject map, single writer (3.2.4.1);
# admission + expiry wired in via ada_admission/ada_event_log (13.2.4.3).
add_library(ada_store STATIC src/store/track_store.cpp)
target_include_directories(ada_store PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(ada_store PUBLIC ada_contracts ada_admission ada_event_log)
target_compile_features(ada_store PUBLIC cxx_std_17)

ada_add_test(ada_track_store_test tests/store/test_track_store.cpp)
target_link_libraries(ada_track_store_test PRIVATE ada_store)

# R13 admission state machine — pure function, thresholds injected (13.2.4.2).
add_library(ada_admission STATIC src/store/admission.cpp)
target_include_directories(ada_admission PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(ada_admission PUBLIC ada_contracts)
target_compile_features(ada_admission PUBLIC cxx_std_17)

ada_add_test(ada_admission_own_sensor_test tests/store/test_admission_own_sensor.cpp)
target_link_libraries(ada_admission_own_sensor_test PRIVATE ada_admission)

ada_add_test(ada_admission_relayed_test tests/store/test_admission_relayed.cpp)
target_link_libraries(ada_admission_relayed_test PRIVATE ada_store)

ada_add_test(ada_expiry_monotonic_test tests/store/test_expiry_monotonic.cpp)
target_link_libraries(ada_expiry_monotonic_test PRIVATE ada_store)

# R2 UDP listener thread — socket → input queue, no parsing (2.2.6.1).
add_library(ada_v2x_listener STATIC src/observer/v2x_listener.cpp)
target_include_directories(ada_v2x_listener PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(ada_v2x_listener PUBLIC ada_net ada_input_queue Threads::Threads)
target_compile_features(ada_v2x_listener PUBLIC cxx_std_17)

ada_add_test(ada_v2x_listener_test tests/observer/test_v2x_listener.cpp)
target_link_libraries(ada_v2x_listener_test PRIVATE ada_v2x_listener)

# Detector reader — spawn/read/respawn of DETECTOR_CMD, the ego side of the
# D2 process contract (12.2.6.2).
add_library(ada_detector_reader STATIC src/observer/detector_reader.cpp)
target_include_directories(ada_detector_reader PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(ada_detector_reader PUBLIC ada_input_queue ada_event_log Threads::Threads)
target_compile_features(ada_detector_reader PUBLIC cxx_std_17)

ada_add_test(ada_detector_reader_test tests/observer/test_detector_reader.cpp)
target_link_libraries(ada_detector_reader_test PRIVATE ada_detector_reader)

# R14 CRA seam — frozen header-only interface, AssessmentDb forward-declared (14.2.5.1).
add_library(ada_cra_interface INTERFACE)
target_include_directories(ada_cra_interface INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(ada_cra_interface INTERFACE ada_store)
target_compile_features(ada_cra_interface INTERFACE cxx_std_17)

ada_add_test(ada_cra_interface_test tests/cra/test_cra_interface.cpp)
target_link_libraries(ada_cra_interface_test PRIVATE ada_cra_interface)

# R14 CRA plugin registry — explicit registration, no static-init (14.2.5.4);
# carries the M1 NLOS plugin (14.4.1.2).
add_library(ada_cra_registry STATIC
  src/cra/registry.cpp
  src/cra/builtin_plugins.cpp
  src/cra/plugins/chained_collision.cpp
)
target_include_directories(ada_cra_registry PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(ada_cra_registry PUBLIC ada_cra_interface ada_config ada_fusion ada_assessment_db)
target_compile_features(ada_cra_registry PUBLIC cxx_std_17)

ada_add_test(ada_cra_registry_test tests/cra/test_registry.cpp)
target_link_libraries(ada_cra_registry_test PRIVATE ada_cra_registry)

ada_add_test(ada_chained_collision_test tests/cra/test_chained_collision.cpp)
target_link_libraries(ada_chained_collision_test PRIVATE ada_cra_registry ada_assessment_db ada_fusion)

# R14 assessment database — typed accessor over the D4 schema, every write
# mirrored to the [EVT] stream (14.2.5.3).
add_library(ada_assessment_db STATIC src/cra/assessment_db.cpp)
target_include_directories(ada_assessment_db PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(ada_assessment_db PUBLIC ada_contracts ada_event_log)
target_compile_features(ada_assessment_db PUBLIC cxx_std_17)

ada_add_test(ada_assessment_db_test tests/cra/test_assessment_db.cpp)
target_link_libraries(ada_assessment_db_test PRIVATE ada_assessment_db)

# Ego-frame scene composition — pure geometry over the store (15.4.1.1).
add_library(ada_fusion STATIC src/fusion/scene_composer.cpp)
target_include_directories(ada_fusion PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(ada_fusion PUBLIC ada_store)
target_compile_features(ada_fusion PUBLIC cxx_std_17)

ada_add_test(ada_scene_composer_test tests/fusion/test_scene_composer.cpp)
target_link_libraries(ada_scene_composer_test PRIVATE ada_fusion)

# R15 warning builder — RiskFinding + composed geometry onto the frozen R4
# binding; the node's only R4 producer (15.4.2.1).
add_library(ada_warning_builder STATIC src/output/warning_builder.cpp)
target_include_directories(ada_warning_builder PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(ada_warning_builder PUBLIC ada_contracts ada_fusion)
target_compile_features(ada_warning_builder PUBLIC cxx_std_17)

# The builder test loads the synced schemas from disk (schema-guided
# structural validation) — ADA_CONTRACTS_DIR mirrors the ADA_FIXTURE_DIR
# pattern of ada_add_test.
ada_add_test(ada_warning_builder_test tests/output/test_warning_builder.cpp)
target_link_libraries(ada_warning_builder_test PRIVATE ada_warning_builder)
target_compile_definitions(ada_warning_builder_test PRIVATE ADA_CONTRACTS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/contracts")

# R15 IVI sender — one UDP datagram per R4 event, r4_tx evidence (15.4.2.2).
add_library(ada_ivi_sender STATIC src/output/ivi_sender.cpp)
target_include_directories(ada_ivi_sender PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(ada_ivi_sender PUBLIC ada_contracts ada_net ada_event_log)
target_compile_features(ada_ivi_sender PUBLIC cxx_std_17)

ada_add_test(ada_ivi_sender_test tests/output/test_ivi_sender.cpp)
target_link_libraries(ada_ivi_sender_test PRIVATE ada_ivi_sender ada_warning_builder ada_fusion)

# The ada_ecu executable — composition root, controller only (13.2.6.4).
add_executable(ada_ecu src/main.cpp)
target_link_libraries(ada_ecu PRIVATE
  ada_config ada_store ada_assessment_db ada_cra_registry ada_parser
  ada_v2x_listener ada_detector_reader ada_event_log ada_input_queue
  ada_fusion ada_warning_builder ada_ivi_sender
  Threads::Threads)
