# CMake 3.28 is the floor because FetchContent_Declare(... EXCLUDE_FROM_ALL) only exists from 3.28
# on, and without it Vanetza's root CMakeLists adds every component (geonet, security, gnss, ...) to
# the ALL target. The ubuntu-latest runner ships CMake 3.31, so the floor is satisfied on CI.
# (ADA_ECU/CMakeLists.txt stays at 3.22 - it has no Vanetza dependency.)
cmake_minimum_required(VERSION 3.28)

# C is enabled on purpose: Vanetza's ASN.1 layer is asn1c-generated C, compiled in-tree.
project(v2x_ecu CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Dependencies via FetchContent, exact pinned tags/commits only (no floating branches).
include(FetchContent)

FetchContent_Declare(nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.11.3
)
FetchContent_Declare(googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG v1.14.0
)
# Vanetza declare + ASN.1-only option forcing live in the shared pin fragment (master:
# contracts/vanetza-pin.cmake, byte-synced copy here) shared with the Scenario Player
# codec helper. The fragment declares only; MakeAvailable stays below.
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/vanetza-pin.cmake)

FetchContent_MakeAvailable(nlohmann_json googletest vanetza)

enable_testing()
include(GoogleTest)

# Helper for registering test executables: links nlohmann_json + gtest_main,
# exposes the shared fixture dir, and wires CTest discovery.
function(v2x_add_test name)
  add_executable(${name} ${ARGN})
  target_link_libraries(${name} PRIVATE nlohmann_json::nlohmann_json GTest::gtest_main)
  target_compile_definitions(${name} PRIVATE V2X_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures")
  gtest_discover_tests(${name})
endfunction()

v2x_add_test(v2x_sanity_test tests/test_sanity.cpp)
# Vanetza::asn1 carries the asn1c wrapper + release-1 asn1_its; the release-2 type descriptors
# (asn_DEF_Vanetza_ITS2_*) live in asn1_its_r2 and must be linked explicitly. This mirrors Vanetza's
# own ItsAsn1Rel2 test. Security ASN.1 targets are deliberately not linked.
target_link_libraries(v2x_sanity_test PRIVATE Vanetza::asn1 Vanetza::asn1_its_r2)

# R1 codec seam (1.0.2.2) - header-only: CpmContent, the nlohmann binding and the
# ICpmCodec pure-virtual interface all live in src/codec/cpm_codec.hpp, so there is
# no .cpp and no compiled library - an INTERFACE target is the right shape. It puts
# src/ on the include path for every consumer (1.0.2.3 VanetzaCpmCodec, 1.0.2.4
# gv_tool, 1.0.2.5 golden vectors, 2.0.3.1 R2 binding). Deliberately no Vanetza
# link: the seam itself is codec-library-agnostic (F2).
add_library(v2x_codec_seam INTERFACE)
target_include_directories(v2x_codec_seam INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(v2x_codec_seam INTERFACE nlohmann_json::nlohmann_json)
target_compile_features(v2x_codec_seam INTERFACE cxx_std_17)

v2x_add_test(v2x_cpm_content_roundtrip_test tests/codec/test_cpm_content_roundtrip.cpp)
target_link_libraries(v2x_cpm_content_roundtrip_test PRIVATE v2x_codec_seam)

# Contracts library — pure model code, no transport (2.0.3.1).
add_library(v2x_contracts STATIC
  src/contracts/r2_message.cpp
)
target_include_directories(v2x_contracts PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(v2x_contracts PUBLIC nlohmann_json::nlohmann_json)
target_compile_features(v2x_contracts PUBLIC cxx_std_17)

v2x_add_test(v2x_r2_roundtrip_test tests/contracts/test_r2_roundtrip.cpp)
target_link_libraries(v2x_r2_roundtrip_test PRIVATE v2x_contracts)

# R1 codec implementation (1.0.2.3) - the sole ICpmCodec: CpmContent <-> ASN.1 UPER over
# vanetza::asn1::r2::Cpm. Vanetza::asn1 carries the asn1c PER wrapper + allocator; the
# release-2 type descriptors (asn_DEF_Vanetza_ITS2_*) live in Vanetza::asn1_its_r2.
# Both Vanetza links are PRIVATE: vanetza_cpm_codec.hpp names no ASN.1 type, so seam
# consumers never need the ASN.1 headers on their include path (F2).
add_library(v2x_cpm_codec STATIC
  src/codec/vanetza_cpm_codec.cpp
)
target_link_libraries(v2x_cpm_codec
  PUBLIC v2x_codec_seam
  PRIVATE Vanetza::asn1 Vanetza::asn1_its_r2
)
target_compile_features(v2x_cpm_codec PUBLIC cxx_std_17)

v2x_add_test(v2x_vanetza_cpm_codec_test tests/codec/test_vanetza_cpm_codec.cpp)
target_link_libraries(v2x_vanetza_cpm_codec_test PRIVATE v2x_cpm_codec)

# Golden-vector generator (1.0.2.4) - build-only developer/CI tool, never shipped in a node
# image (HLD D3). A plain add_executable, deliberately NOT v2x_add_test: it is a CLI that
# writes the frozen corpus, not a gtest binary, so it must not be registered with CTest.
# Linking v2x_cpm_codec is enough - it carries v2x_codec_seam (src/ include path + nlohmann)
# PUBLIC-ly, and the Vanetza ASN.1 links PRIVATE-ly (F2: no ASN.1 headers reach this TU).
add_executable(gv_tool tools/golden_vectors/main.cpp)
target_link_libraries(gv_tool PRIVATE v2x_cpm_codec)
# CI invokes the literal path V2X_ECU/build/gv_tool, so pin the output dir rather than rely
# on no FetchContent subproject having leaked a CMAKE_RUNTIME_OUTPUT_DIRECTORY into cache.
set_target_properties(gv_tool PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# Golden-vector conformance test (1.0.2.5) - reads the frozen corpus synced to
# tests/fixtures/golden/ (V2X_FIXTURE_DIR, set by v2x_add_test) and drives it through the
# real codec, so it links v2x_cpm_codec rather than the bare seam: the seam alone has no
# implementation to encode/decode with.
v2x_add_test(v2x_cpm_golden_vectors_test tests/codec/test_cpm_golden_vectors.cpp)
target_link_libraries(v2x_cpm_golden_vectors_test PRIVATE v2x_cpm_codec)

# Env config loader (8.1.2.1) - the app's only env reader (Phase 1 HLD sections 4/6).
# Transport-blind data-layer code: stdlib only - no nlohmann, no Vanetza, and no socket
# headers (the tools/check_transport_imports.py gate applies). Static-lib shape mirrors
# v2x_contracts; the HLD section 6 defaults table lives in src/config/config.cpp alone.
add_library(v2x_config STATIC
  src/config/config.cpp
)
target_include_directories(v2x_config PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_features(v2x_config PUBLIC cxx_std_17)

v2x_add_test(v2x_config_test tests/config/test_config.cpp)
target_link_libraries(v2x_config_test PRIVATE v2x_config)

# Sole socket-API holder (7.1.2.2, Phase 1 HLD D1) - src/net/ is the only code
# allowed to include transport headers, and only udp_socket.cpp does; the header
# stays POSIX-free so consumers (R7 stub Rx, 2.1.2.4 ADA forwarder) never see
# them (tools/check_transport_imports.py enforces this). Static-lib shape
# mirrors v2x_config: stdlib + POSIX only - no nlohmann, no Vanetza.
add_library(v2x_net STATIC
  src/net/udp_socket.cpp
)
target_include_directories(v2x_net PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_features(v2x_net PUBLIC cxx_std_17)

v2x_add_test(v2x_udp_socket_test tests/net/test_udp_socket.cpp)
target_link_libraries(v2x_udp_socket_test PRIVATE v2x_net)

# R18 JSONL event-log writer (18.1.2.3, Phase 1 HLD D4) - the [EVT] evidence
# stream. Links PUBLIC v2x_codec_seam (src/ include path + nlohmann + the
# CpmContent binding embedded by decode_ok) and v2x_contracts (the R2Message
# binding embedded by r2_forwarded). Transport-blind: no socket headers
# (tools/check_transport_imports.py applies).
add_library(v2x_event_log STATIC
  src/log/event_log.cpp
)
target_link_libraries(v2x_event_log
  PUBLIC v2x_codec_seam v2x_contracts
)
target_compile_features(v2x_event_log PUBLIC cxx_std_17)

v2x_add_test(v2x_event_log_test tests/log/test_event_log.cpp)
target_link_libraries(v2x_event_log_test PRIVATE v2x_event_log)

# ADA forwarder (2.1.2.4, Phase 1 HLD D1 / D3 stage 4) - the intra-ego R2
# edge, deliberately NOT under the R7 seam: serializes the Phase 0 R2Message
# to compact JSON and sends one UDP datagram per message. Links PUBLIC
# v2x_net (the sole socket holder it consumes - no socket headers here, the
# transport-import gate applies) and v2x_contracts (the R2 binding in its
# public signature + nlohmann transitively). Fire-and-forget per HLD D3:
# no retry/queueing target exists.
add_library(v2x_forward STATIC
  src/forward/ada_forwarder.cpp
)
target_link_libraries(v2x_forward
  PUBLIC v2x_net v2x_contracts
)
target_compile_features(v2x_forward PUBLIC cxx_std_17)

v2x_add_test(v2x_ada_forwarder_test tests/forward/test_ada_forwarder.cpp)
target_link_libraries(v2x_ada_forwarder_test PRIVATE v2x_forward)

# R7 radio-adapter seam (7.1.3.1, Phase 1 HLD D2) - the FROZEN interface:
# RadioConfig, RxCallback, RadioResult and the pure-virtual IRadioAdapter all
# live header-only in src/adapter/i_radio_adapter.hpp, so an INTERFACE target
# is the right shape (pattern: v2x_codec_seam; no nlohmann link - the seam
# includes only <cstdint>/<functional>/<vector>). Deliberately transport-free
# per HLD D1: the seam mirrors the radio (telux) surface only, and the
# tools/check_transport_imports.py gate applies. Consumers: 7.1.3.4
# StubRadioAdapter, 8.1.3.2 modem stub (RadioConfig + result codes).
add_library(v2x_adapter_seam INTERFACE)
target_include_directories(v2x_adapter_seam INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_features(v2x_adapter_seam INTERFACE cxx_std_17)

v2x_add_test(v2x_i_radio_adapter_test tests/adapter/test_i_radio_adapter.cpp)
target_link_libraries(v2x_i_radio_adapter_test PRIVATE v2x_adapter_seam)

# R8 modem stub FSM (8.1.3.2, Phase 1 HLD D2) - pure bring-up logic
# idle -> initialized -> configured -> rx-subscribed, each call acked with the
# seam's RadioResult and every outcome (ack or illegal-order rejection)
# reported to the injectable TransitionObserver (main wires it to
# EventLog::stubTransition). Constructor takes plain params (FaultPlan +
# RetryParams), never reads env; plans other than None are stored unused
# until 8.1.3.3 adds fault injection + recoveries. Transport-blind: no
# sockets/threads (tools/check_transport_imports.py applies). Links PUBLIC
# v2x_config (FaultPlan in the constructor signature) and v2x_adapter_seam
# (RadioConfig/RadioResult in the API).
add_library(v2x_stub STATIC
  src/stub/modem_stub.cpp
)
target_link_libraries(v2x_stub
  PUBLIC v2x_config v2x_adapter_seam
)
target_compile_features(v2x_stub PUBLIC cxx_std_17)

v2x_add_test(v2x_modem_stub_fsm_test tests/stub/test_modem_stub_fsm.cpp)
target_link_libraries(v2x_modem_stub_fsm_test PRIVATE v2x_stub)
