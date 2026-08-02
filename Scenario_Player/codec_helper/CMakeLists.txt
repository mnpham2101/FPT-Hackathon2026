# Scenario_Player/codec_helper/CMakeLists.txt - the `cpm_encode` helper build (SP HLD D1/D2).
#
# SECOND BUILD CONTEXT OVER THE SAME CODEC SOURCE (SP HLD D2). This folder is not a second
# codec: src/codec/{cpm_codec.hpp, vanetza_cpm_codec.hpp, vanetza_cpm_codec.cpp} and
# cmake/vanetza-pin.cmake are BYTE-SYNCED COPIES of their masters
# (V2X_ECU/src/codec/* and contracts/vanetza-pin.cmake), present only because each node folder
# is a self-contained Docker build context that may not read across folders. They must NEVER be
# edited here - edit the master, then re-sync; contracts/check_sync.py gates byte identity and
# contracts/sync-manifest.json lists the pairs. Only CMakeLists.txt and src/main.cpp are
# original to this folder.
#
# Mirrors V2X_ECU/CMakeLists.txt for every shared part (CMake floor, languages, nlohmann pin,
# pin-fragment placement, ASN.1 link set) so both contexts compile the same seam identically.
# Deliberately NO GoogleTest: there are no C++ tests here - the helper's wire truth is proven by
# the Python golden test Scenario_Player/tests/test_encoder_golden.py (11.1.7.2) and the CI
# `sp-codec-helper` lane (11.1.8.1).

# CMake 3.28 is the floor because FetchContent_Declare(... EXCLUDE_FROM_ALL) - used by the synced
# Vanetza pin fragment - only exists from 3.28 on; without it Vanetza's root CMakeLists adds every
# component (geonet, security, gnss, ...) to the ALL target.
cmake_minimum_required(VERSION 3.28)

# C is enabled on purpose: Vanetza's ASN.1 layer is asn1c-generated C, compiled in-tree.
project(cpm_encode_helper CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Dependencies via FetchContent, exact pinned tags/commits only (no floating branches).
include(FetchContent)

# Same nlohmann pin as V2X_ECU/CMakeLists.txt - the synced seam's JSON binding must behave
# identically in both build contexts.
FetchContent_Declare(nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.11.3
)
# Vanetza declare + ASN.1-only option forcing live in the shared pin fragment (master:
# contracts/vanetza-pin.cmake, byte-synced copy here). The fragment declares only;
# MakeAvailable stays below.
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/vanetza-pin.cmake)

FetchContent_MakeAvailable(nlohmann_json vanetza)

# The helper: the D1 CLI plus the synced codec implementation, compiled straight into the
# executable (no intermediate library - this context has exactly one consumer).
add_executable(cpm_encode
  src/main.cpp
  src/codec/vanetza_cpm_codec.cpp
)
# src/ on the include path so `#include "codec/..."` resolves exactly as it does in V2X_ECU -
# that is what keeps the copies byte-identical to their masters.
target_include_directories(cpm_encode PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
# Vanetza::asn1 carries the asn1c PER wrapper + allocator; the release-2 type descriptors
# (asn_DEF_Vanetza_ITS2_*) live in Vanetza::asn1_its_r2 and must be linked explicitly. Both are
# PRIVATE exactly as in V2X_ECU's v2x_cpm_codec: vanetza_cpm_codec.hpp names no ASN.1 type, so
# main.cpp never sees an ASN.1 header (F2). Security ASN.1 targets are deliberately not linked.
target_link_libraries(cpm_encode PRIVATE
  nlohmann_json::nlohmann_json
  Vanetza::asn1
  Vanetza::asn1_its_r2
)
target_compile_features(cpm_encode PRIVATE cxx_std_17)

# The golden test (11.1.7.2), the CI lane (11.1.8.1) and the Dockerfile (5.1.7.3) all invoke the
# literal path Scenario_Player/codec_helper/build/cpm_encode, so pin the output dir rather than
# rely on no FetchContent subproject having leaked a CMAKE_RUNTIME_OUTPUT_DIRECTORY into the
# cache - same precedent as V2X_ECU's gv_tool and v2x_ecu targets.
set_target_properties(cpm_encode PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
