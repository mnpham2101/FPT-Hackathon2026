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
# BUILD-TIME RISKS this image carries (the Wave D CI lane 5.1.8.2 is what proves them):
#   * arm64 under QEMU: the helper drags in Vanetza's asn1c-generated C (asn1_support + asn1_its +
#     asn1_its_r2, ~1400 translation units), emulated instruction-by-instruction on an amd64
#     runner. Expect tens of minutes - the lane needs a raised timeout-minutes and a buildx cache,
#     exactly as the native v2x-core-build lane already needs actions/cache on the FetchContent
#     _deps tree. This image is NOT cheaper than the V2X ECU one: it compiles the same ASN.1 set.
#   * Network is required at build time: dependencies come in via CMake FetchContent (Vanetza and
#     nlohmann/json are git-cloned during `cmake -S`), so the build host must reach github.com.
#     There is no vendored/offline path.
#   * GLIBC / LIBSTDC++ SKEW BETWEEN THE STAGES - the one risk this image has and V2X_ECU's does
#     not. That image pins the IDENTICAL base tag in both stages so the binary runs on the libs it
#     linked against; here stage 2's base is fixed by the HLD (section 4: "python-slim runtime")
#     while stage 1 must be trixie to get cmake >= 3.28 (see the stage-1 comment). So the helper is
#     compiled against trixie's glibc/GCC 14 libstdc++ and then runs on bookworm-based
#     python:3.11-slim (older glibc, GCC 12 libstdc++) - a forward-compat symbol mismatch
#     (`version GLIBCXX_3.4.3x not found`, `__isoc23_strtol`) is possible and would be caught by
#     the stage-2 `ldd` guard at the END of an expensive build.
#     Mitigation chosen: (b) keep trixie and let the 5.1.8.2 lane prove it. Rejected: (a) building
#     stage 1 on python:3.11-slim with apt's cmake - bookworm ships cmake 3.25, below the 3.28
#     floor, so apt alone cannot configure this project at all.
#     DOCUMENTED FALLBACKS if the lane reports a mismatch, best first:
#       F1. `FROM python:3.11-slim AS build` + `pip install "cmake>=3.28,<4"` (Kitware's PyPI
#           wheels; `<4` keeps Vanetza's older cmake_minimum_required valid). Same base tag in both
#           stages, so the skew is gone BY CONSTRUCTION - this is variant (a) with cmake from a
#           non-apt source, and the HLD names no stage-1 base, so it needs no design change.
#       F2. Move stage 2 onto a trixie-based python tag (python:3.11-slim-trixie if published,
#           else the newest published trixie python-slim) - pins one glibc, but changes the runtime
#           tag the HLD section 4 names, so it needs an HLD touch.
#       F3. Also stage stage-1's libstdc++.so.6 into /out/lib - fixes the C++ runtime half only,
#           leaves the glibc half. Partial; prefer F1.
#
# LICENSE POSTURE (report section 4, SP HLD section 10 item 2): Vanetza is LGPLv3, so it is linked
# DYNAMICALLY - -DBUILD_SHARED_LIBS=ON below, and the resulting libvanetza_asn1*.so are copied into
# the runtime stage. Never link it statically here.

# ---------------------------------------------------------------------------
# Stage 1 - build the cpm_encode helper
# ---------------------------------------------------------------------------
# debian:trixie-slim, NOT bookworm: codec_helper/CMakeLists.txt sets
# cmake_minimum_required(VERSION 3.28) (the synced Vanetza pin uses
# FetchContent_Declare(... EXCLUDE_FROM_ALL), which only exists from 3.28 on), and bookworm's apt
# cmake is 3.25 - it cannot configure this project at all. trixie ships 3.31. Same reasoning, same
# tag as V2X_ECU/Dockerfile's build stage: the two contexts compile the same codec seam.
FROM debian:trixie-slim AS build

# make: the default CMake generator is Unix Makefiles and neither cmake nor g++ depends on make.
# git + ca-certificates: FetchContent clones over https.
# libboost-dev + libboost-date-time-dev: Vanetza's hard dependency, same pair the
# v2x-core-build CI lane installs.
RUN apt-get update \
 && apt-get install -y --no-install-recommends \
      ca-certificates \
      cmake \
      g++ \
      git \
      libboost-date-time-dev \
      libboost-dev \
      make \
 && rm -rf /var/lib/apt/lists/*

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
# python:3.11-slim per SP HLD section 4 (Python 3.11 + PyYAML generator). Pinned to the minor, not
# :latest, so the interpreter the tests ran against is the interpreter the node runs.
FROM python:3.11-slim

# libstdc++6 is installed EXPLICITLY rather than assumed: cpm_encode is a C++ binary, and the
# official python:*-slim images keep only the shared libs CPython itself resolved (their build
# apt-marks those and purges the rest of the toolchain), so libstdc++6 being present is an
# ASSUMPTION, not a documented guarantee of the base. One apt line makes it deterministic instead.
# Caveat: this installs bookworm's GCC 12 libstdc++, which does NOT resolve a GLIBCXX symbol newer
# than 3.4.30 - it fixes "missing entirely", not the header's version-skew risk. The ldd guard
# below is what turns that into a build failure rather than a start-up failure on the node.
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
# against (a Vanetza .so left behind, or the header's glibc/libstdc++ skew): a "not found" here
# would otherwise surface as a node that starts, spawns the helper, and dies on the first encode
# with only a loader error in View Log.
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
