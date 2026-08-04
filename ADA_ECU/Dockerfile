# ADA ECU node image (ADA HLD section 11, decision D9) - multi-stage: cmake/g++ build ->
# same-base runtime + tcpdump. Context is ADA_ECU/ ALONE (self-contained; no cross-node
# source imports).
#
# BUILD RECIPE (from the repo root):
#   docker buildx build --platform linux/arm64 --provenance=false --sbom=false \
#     -t m1-ada-ecu:latest ADA_ECU/
#
#   --platform linux/arm64 is a STANDING REQUIREMENT, not a preference: a CarSky Container
#   node rejects a multi-platform manifest index and hangs in Provisioning
#   (plans/doc/phase0-smoke-test-run.md, "images must be single-platform arm64"), so exactly
#   one platform goes in the buildx invocation. --provenance/--sbom off for the same reason -
#   attestations turn the tag into an index.
#
# ONE BASE IMAGE FOR BOTH STAGES (D9): python:3.11-slim is the report's Python 3.11 for the
# detector (which enters this image in Phase 3, 5.3.6.1) AND the C++ build base, so the core
# binary links against exactly the glibc and libstdc++ it later runs on - cross-base symbol
# skew is impossible BY CONSTRUCTION. The single ARG keeps the two FROM lines from drifting.
# This node carries NO Vanetza, so neither the ASN.1 build cost nor the CMake 3.28 floor of
# the V2X/Scenario_Player images applies: CMakeLists.txt asks for 3.22 and this base's apt
# cmake (bookworm, 3.25) clears it - no pip cmake needed.
#
# Network is required at build time: dependencies come in via CMake FetchContent
# (nlohmann/json and googletest are git-cloned during `cmake -S`), so the build host must
# reach github.com. googletest is downloaded even though no test target is ever built here -
# FetchContent_MakeAvailable in ADA_ECU/CMakeLists.txt runs unconditionally at configure.
# Accepted cost: stripping it would mean editing CMakeLists.txt, which is not this file's to
# change, and the clone is cached in the builder's layer cache.

# Declared before the first FROM so it is a GLOBAL build arg - the only scope from which a
# value can be substituted into a FROM instruction. Both FROM lines use it verbatim.
ARG BASE_IMAGE=python:3.11-slim

# ---------------------------------------------------------------------------
# Stage 1 - build
# ---------------------------------------------------------------------------
FROM ${BASE_IMAGE} AS build

# make: the default CMake generator is Unix Makefiles and neither cmake nor g++ depends on it.
# git + ca-certificates: FetchContent clones over https.
RUN apt-get update \
 && apt-get install -y --no-install-recommends \
      ca-certificates \
      cmake \
      g++ \
      git \
      make \
 && rm -rf /var/lib/apt/lists/*

WORKDIR /src

# The whole context, .dockerignore-filtered (doc/, tools/, schema/, build/, the media
# provenance notes and detector/requirements-dev.txt never reach the builder). tests/ IS in
# the context ON PURPOSE: CMakeLists.txt names tests/**.cpp in ada_add_test()'s
# add_executable(), and CMake resolves those sources at CONFIGURE time - a missing file is a
# hard generate error even for targets this image never builds. Everything copied here is
# build-stage only and never reaches stage 2.
COPY . .

RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release

# Target-scoped build - THIS is what keeps the test binaries out of the image: `--target
# ada_ecu` builds the app and its link closure only, so no gtest executable is ever compiled.
# Bounded -j on purpose: a bare `-j` means unlimited jobs to make - far worse under QEMU.
RUN cmake --build build --target ada_ecu -j"$(nproc)"

# ---------------------------------------------------------------------------
# Stage 2 - runtime
# ---------------------------------------------------------------------------
# Same ${BASE_IMAGE} as stage 1 (header): the interpreter the Phase 3 detector will run on,
# and the exact glibc/libstdc++ the stage-1 link resolved against.
FROM ${BASE_IMAGE}

# tcpdump: capture.sh's capture handle (lands in Phase 4, 6.4.4.1) - needs the NET_RAW
#   capability at runtime, granted by the blueprint (node-ada-ecu.md), never by this image.
# coreutils: base64 (the pcap export encoding) plus mktemp/stdbuf for the same script.
# libstdc++6 is an explicit install because it buys PRESENCE, not compatibility: the official
#   python:*-slim images guarantee only the shared libs CPython itself resolved, so a C++
#   runtime lib shipping in the base is an assumption. On this base the line can only resolve
#   to the same libstdc++ stage 1 compiled against, so it cannot reintroduce skew (the same
#   reasoning as Scenario_Player's runtime stage).
RUN apt-get update \
 && apt-get install -y --no-install-recommends \
      coreutils \
      libstdc++6 \
      tcpdump \
 && rm -rf /var/lib/apt/lists/*

# /app is the image workdir the blueprint's relative command ["./entrypoint.sh"] resolves
# against (node-ada-ecu.md).
WORKDIR /app

# Media and model blobs change rarely - their layers push once and cache thereafter, so they
# sit ABOVE the code layers that change every edit. The models/ and detector/ layers land with
# 5.3.6.1 BELOW this line and ABOVE the code layers.
COPY media/ /app/media/

# models/ above detector/ (5.3.6.1): detector code changes every commit while media/models
# rarely do - layer order decides which blobs are re-pushed.
COPY models/ /app/models/
COPY detector/ /app/detector/
RUN pip install --no-cache-dir -r /app/detector/requirements.txt

COPY --from=build /src/build/ada_ecu ./ada_ecu
COPY entrypoint.sh ./

# chmod rather than trusting the git file mode: the mode is not guaranteed to survive every
# checkout/context path, and entrypoint.sh IS the container command - non-executable means the
# node dies at start.
RUN chmod +x ./entrypoint.sh

# Fail the BUILD, not the deploy, if the runtime stage is missing something the app links
# against: a "not found" here would otherwise surface as a node stuck at start-up with only a
# loader error in View Log. With one base image across both stages this guard is a regression
# detector, not a safety net.
RUN if ldd ./ada_ecu | grep -q 'not found'; then \
      echo "FATAL: unresolved shared libraries in the runtime stage:"; ldd ./ada_ecu; exit 1; \
    fi \
 && ldd ./ada_ecu

# Deliberately NO ENV for any HLD section 6 tunable (V2X_LISTEN_PORT, IVI_ECU_HOST/PORT,
# GATE_ENTER_M, GATE_EXIT_M, and the rest): the blueprint injects them (node-ada-ecu.md), and
# anything it omits must fall through to the single owner of those defaults -
# src/config/config.cpp. An ENV here would silently shadow them and make the image, not the
# config, the source of truth (CLAUDE.md governing principle 5).
#
# Deliberately NO USER: the platform runs the container as-is and the Phase 4 capture path
# needs NET_RAW - the same posture as the V2X ECU and Scenario Player images.
#
# CMD mirrors the blueprint `command: ["./entrypoint.sh"]` exactly so the image is runnable
# standalone (docker run) for local checks; node-ada-ecu.md's `command` remains authoritative.
CMD ["./entrypoint.sh"]
