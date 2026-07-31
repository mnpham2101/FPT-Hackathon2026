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
FetchContent_Declare(vanetza
  GIT_REPOSITORY https://github.com/riebl/vanetza.git
  GIT_TAG fb6c551030dcc12b924299bf401e35e5fe814713  # tag v26.06
  EXCLUDE_FROM_ALL
)

# ASN.1-only: no security/geodesy/RPC backends (HLD section 8 - no GN/BTP full-stack pull-in).
# Vanetza defaults these options to "was the package found on this machine", which would make the
# configure step depend on the runner image; forcing them keeps it deterministic.
set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(VANETZA_WITH_GEOGRAPHICLIB OFF CACHE BOOL "" FORCE)
set(VANETZA_WITH_RPC OFF CACHE BOOL "" FORCE)
set(VANETZA_WITH_CRYPTOPP OFF CACHE BOOL "" FORCE)
set(VANETZA_WITH_OPENSSL OFF CACHE BOOL "" FORCE)

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

# Later subtasks add their targets here:
# gv_tool (1.0.2.4) and the golden-vector test (1.0.2.5).
