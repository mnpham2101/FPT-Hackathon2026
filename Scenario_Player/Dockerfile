# Scenario Player node image (SP HLD sections 3/8) - multi-stage: cmake builds the cpm_encode
# helper -> python-slim runtime carrying main.py + player/ + scenarios/ + the helper binary.
# Context is Scenario_Player/ ALONE (self-contained; no cross-node source imports) - the helper
# compiles here from the byte-synced codec seam in codec_helper/ (SP HLD D1/D2), not from V2X_ECU/.
#
# BUILD RECIPE (from the repo root):
#   docker buildx build --platform linux/arm64 --provenance=false --sbom=false \
#     -t m1-scenario-player:latest Scenario_Player/
#
#   --platform linux/arm64 is a STANDING REQUIREMENT, not a preference: a CarSky Container
#   node rejects a multi-platform manifest index and hangs in Provisioning
#   (plans/doc/phase0-smoke-test-run.md, "images must be single-platform arm64"), so exactly
#   one platform goes in the buildx invocation. --provenance/--sbom off for the same reason -
#   attestations turn the tag into an index.
#
# ONE BASE IMAGE FOR BOTH STAGES - fallback F1, ratified by the architecture owner 2026-08-01.
#   Stage 1 and stage 2 both resolve ${BASE_IMAGE} (python:3.11-slim), so cpm_encode links against
#   exactly the glibc and libstdc++ it later runs on: cross-base symbol skew
#   (`version GLIBCXX_3.4.30 not found`, `__isoc23_strtol`) is impossible BY CONSTRUCTION rather
#   than something the stage-2 ldd guard has to catch at the end of an expensive emulated build.
#   The single ARG is what keeps the two references from drifting - never hardcode a tag in a FROM.
#   cmake comes from pip, not apt, because this base is bookworm-based and bookworm's apt cmake is
#   3.25 - below the 3.28 floor (see the stage-1 comment). No HLD change: SP HLD section 4 fixes
#   only the stage-2 runtime as python-slim and names no builder base.
#
# BUILD-TIME RISKS this image carries (the Wave D CI lane 5.1.8.2 is what proves them):
#   * arm64 under QEMU: the helper drags in Vanetza's asn1c-generated C (asn1_support + asn1_its +
#     asn1_its_r2, ~1400 translation units), emulated instruction-by-instruction on an amd64
#     runner. Expect tens of minutes - the lane needs a raised timeout-minutes and a buildx cache,
#     exactly as the native v2x-core-build lane already needs actions/cache on the FetchContent
#     _deps tree. This image is NOT cheaper than the V2X ECU one: it compiles the same ASN.1 set.
#   * Network is required at build time: dependencies come in via CMake FetchContent (Vanetza and
#     nlohmann/json are git-cloned during `cmake -S`), so the build host must reach github.com.
#     There is no vendored/offline path.
#
# LICENSE POSTURE (report section 4, SP HLD section 10 item 2): Vanetza is LGPLv3, so it is linked
# DYNAMICALLY - -DBUILD_SHARED_LIBS=ON below, and the resulting libvanetza_asn1*.so are copied into
# the runtime stage. Never link it statically here.

# Declared before the first FROM so it is a GLOBAL build arg - the only scope from which a value
# can be substituted into a FROM instruction. Both FROM lines below use it verbatim.
ARG BASE_IMAGE=python:3.11-slim

# ---------------------------------------------------------------------------
# Stage 1 - build the cpm_encode helper
# ---------------------------------------------------------------------------
FROM ${BASE_IMAGE} AS build

# make: the default CMake generator is Unix Makefiles and neither cmake nor g++ depends on make.
# git + ca-certificates: FetchContent clones over https.
# libboost-dev + libboost-date-time-dev: Vanetza's hard dependency, same pair the
# v2x-core-build CI lane installs.
# Deliberately NO cmake here - apt's is too old on this base (next RUN).
RUN apt-get update \
 && apt-get install -y --no-install-recommends \
      ca-certificates \
      g++ \
      git \
      libboost-date-time-dev \
      libboost-dev \
      make \
 && rm -rf /var/lib/apt/lists/*

# cmake from Kitware's PyPI wheel, not apt: codec_helper/CMakeLists.txt sets
# cmake_minimum_required(VERSION 3.28) because the synced Vanetza pin uses
# FetchContent_Declare(... EXCLUDE_FROM_ALL), which only exists from 3.28 on - and this base is
# bookworm-based, whose apt cmake is 3.25 and cannot configure the project at all. The wheel
# satisfies the floor without moving the base image off the runtime's.
# The `<4` upper bound is load-bearing: CMake 4 removed compatibility with the pre-3.5
# cmake_minimum_required that Vanetza's own CMakeLists still declares, so an unbounded install
# would break the FetchContent subproject.
RUN pip install --no-cache-dir "cmake>=3.28,<4"

WORKDIR /src

# Only what the BUILD needs. codec_helper/ carries its own CMakeLists.txt, its synced
# cmake/vanetza-pin.cmake and src/ - nothing else in this context is a configure- or compile-time
# input (unlike V2X_ECU, whose top-level CMakeLists names tests/ and tools/ sources in
# add_executable(); codec_helper/CMakeLists.txt deliberately has no GoogleTest and one target).
# tests/, doc/ and the Python app are NOT copied here, and never reach stage 2 from this stage.
COPY codec_helper/ codec_helper/

# -S codec_helper (not -S .): the helper's CMakeLists.txt lives one level down, since this build
# context is the whole node folder, not the helper folder.
# BUILD_SHARED_LIBS=ON is the LGPLv3 dynamic-link posture (see header), not a size tweak.
RUN cmake -S codec_helper -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON

# Target-scoped build: cpm_encode plus its link closure only.
# Binary path is /src/build/cpm_encode: codec_helper/CMakeLists.txt ends with
#   set_target_properties(cpm_encode PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
# and CMAKE_BINARY_DIR here is /src/build (from -B build under WORKDIR /src). The property is the
# reason the path is guaranteed rather than merely conventional - it survives a FetchContent
# subproject leaking CMAKE_RUNTIME_OUTPUT_DIRECTORY into the cache.
# Bounded -j on purpose: a bare `-j` means unlimited jobs to make and OOMs on asn1_its' ~700
# concurrent compilers (same reasoning as the CI lanes) - and far worse under QEMU.
RUN cmake --build build --target cpm_encode -j"$(nproc)"

# Stage the runtime artifacts into a flat, predictable /out so stage 2 never has to guess a
# FetchContent path (build/_deps/vanetza-build/vanetza/asn1/... is an implementation detail of
# the pin and moves with it).
#   - libvanetza*.so*  : found, not hardcoded.
#   - libboost*.so*    : whatever the link ACTUALLY resolved, taken from ldd rather than assumed.
#     The linked set is the ASN.1-only one (Vanetza::asn1 + Vanetza::asn1_its_r2), so a compiled
#     Boost runtime is not expected - Boost is header-only on this path. Copying what ldd names
#     costs nothing and removes the guess; stage 2 verifies the result independently.
#   - the guard FAILS THE BUILD LOUDLY if no Vanetza .so was produced (e.g. a future Vanetza pin
#     declaring its libraries explicitly STATIC would silently ignore BUILD_SHARED_LIBS and
#     produce a statically-linked helper - a license-posture regression, not a cosmetic one).
RUN set -eu; \
    mkdir -p /out/lib; \
    cp build/cpm_encode /out/cpm_encode; \
    find build -name 'libvanetza*.so*' -exec cp -a {} /out/lib/ \; ; \
    ldd build/cpm_encode | awk '/=> \//{print $3}' | grep -E '/lib(boost|vanetza)' \
      | xargs -r cp -a -t /out/lib/; \
    ls -A /out/lib | grep -q '^libvanetza_asn1' || { \
      echo "FATAL: no libvanetza_asn1*.so staged - Vanetza did not build shared."; \
      echo "       BUILD_SHARED_LIBS=ON is the LGPLv3 posture and must hold."; \
      exit 1; }; \
    echo "staged runtime libs:"; ls -l /out/lib; \
    ldd /out/cpm_encode

# ---------------------------------------------------------------------------
# Stage 2 - runtime
# ---------------------------------------------------------------------------
# Same ${BASE_IMAGE} as stage 1 (header), and python:3.11-slim is what SP HLD section 4 names for
# the runtime (Python 3.11 + PyYAML generator). Pinned to the minor, not :latest, so the
# interpreter the tests ran against is the interpreter the node runs.
FROM ${BASE_IMAGE}

# libstdc++6 is KEPT as an explicit install even though the shared base makes the VERSION question
# moot - it now buys presence, not compatibility. cpm_encode is a C++ binary, and the official
# python:*-slim images keep only the shared libs CPython itself resolved (their build apt-marks
# those and purges the rest of the toolchain), so libstdc++6 shipping in the base is an ASSUMPTION.
# One apt line makes it deterministic, and on this base it can only resolve to the same GCC 12
# libstdc++ stage 1 compiled against - so it cannot reintroduce skew.
# libc6 / libgcc-s1 / ldd come with the base image.
RUN apt-get update \
 && apt-get install -y --no-install-recommends \
      libstdc++6 \
 && rm -rf /var/lib/apt/lists/*

# Vanetza's shared objects go on the system loader path rather than travelling with an
# LD_LIBRARY_PATH env var: /usr/local/lib is already in Debian's ld.so.conf, so one ldconfig makes
# them resolvable for every process in the container (including the cpm_encode subprocess the
# player spawns) with no environment to forget. The helper still carries stage 1's build-tree
# RPATH; those directories do not exist here, the loader skips them and falls through to the
# ldconfig cache.
COPY --from=build /out/lib/ /usr/local/lib/
RUN ldconfig \
 && if ldd /usr/local/lib/libvanetza_asn1*.so* 2>/dev/null | grep -q 'not found'; then \
      echo "FATAL: staged Vanetza .so has unresolved dependencies in the runtime stage:"; \
      ldd /usr/local/lib/libvanetza_asn1*.so*; exit 1; \
    fi

# /app is the image workdir the blueprint's relative command resolves against, and the literal
# prefix of every SP HLD section 5 default path (SCENARIO_CONFIG, ENCODER_PATH).
WORKDIR /app

# ENCODER_PATH's default is /app/cpm_encode (SP HLD section 5) - this COPY is what makes that
# default true, so the blueprint never has to set the var.
COPY --from=build /out/cpm_encode ./cpm_encode

# requirements.txt alone, copied before the app sources so the pip layer caches independently of
# every code edit. NEVER requirements-dev.txt: it starts with `-r requirements.txt` and adds
# pytest + jsonschema, which are test tooling and have no place in a deployed node
# (.dockerignore keeps it out of the context too).
COPY requirements.txt ./
RUN pip install --no-cache-dir -r requirements.txt

# The app: the entrypoint the blueprint runs, the package it imports, and the scenario data.
# player/ MUST sit beside main.py at /app - main.py does `from player.config import ...`, resolved
# relative to the script directory. scenarios/ is DATA selected at runtime by SCENARIO_CONFIG
# (both committed YAMLs ship, so switching scenarios is a node-config edit + redeploy, never a
# rebuild - SP HLD section 8). tests/, doc/, contracts/ and codec_helper/ are absent from this
# stage by construction: it copies only /out/* from stage 1 plus these four app paths.
COPY main.py ./
COPY player/ ./player/
COPY scenarios/ ./scenarios/

# chmod rather than trusting the git/COPY file mode: cpm_encode is exec'd as a subprocess by
# player/encoder_client.py, and a non-executable helper is a runtime failure at the first CPM.
RUN chmod +x ./cpm_encode

# Fail the BUILD, not the deploy, if the runtime stage is missing something the helper links
# against (a Vanetza .so left behind): a "not found" here would otherwise surface as a node that
# starts, spawns the helper, and dies on the first encode with only a loader error in View Log.
# With one base image across both stages this guard is a regression detector, not the safety net
# it had to be when the stages differed.
RUN if ldd ./cpm_encode | grep -q 'not found'; then \
      echo "FATAL: unresolved shared libraries in the runtime stage:"; ldd ./cpm_encode; exit 1; \
    fi \
 && ldd ./cpm_encode

# Deliberately NO ENV for any SP HLD section 5 tunable (SCENARIO_CONFIG, V2X_ECU_HOST,
# V2X_ECU_PORT, ENCODER_PATH): the blueprint injects them (node-scenario-player.md), and anything
# it omits must fall through to the single owner of that default - player/config.py. An ENV here
# would silently shadow those defaults and make the image, not the config, the source of truth
# (CLAUDE.md governing principle 5).
#
# Deliberately NO USER: the platform runs the container as-is, and keeping root matches the
# netcheck and V2X ECU images already deployed to this Room.
#
# CMD mirrors the blueprint `command: ["python", "main.py"]` exactly so the image is runnable
# standalone (docker run) for local checks; node-scenario-player.md's `command` remains the
# authoritative one.
CMD ["python", "main.py"]
