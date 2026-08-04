# V2X ECU node image (HLD section 11) - multi-stage: cmake/g++ build -> slim runtime + tcpdump.
# Context is V2X_ECU/ ALONE (self-contained; no cross-node source imports).
#
# BUILD RECIPE (from the repo root):
#   docker buildx build --platform linux/arm64 --provenance=false --sbom=false \
#     -t m1-v2x-ecu:latest V2X_ECU/
#
#   --platform linux/arm64 is a STANDING REQUIREMENT, not a preference: a CarSky Container
#   node rejects a multi-platform manifest index and hangs in Provisioning
#   (plans/doc/phase0-smoke-test-run.md, "images must be single-platform arm64"), so exactly
#   one platform goes in the buildx invocation. --provenance/--sbom off for the same reason -
#   attestations turn the tag into an index.
#
# BUILD-TIME RISKS this image carries (the Wave D CI lane 5.1.8.2 is what proves them):
#   * arm64 under QEMU: Vanetza's asn1c-generated C (asn1_support + asn1_its + asn1_its_r2,
#     ~1400 translation units) is emulated instruction-by-instruction on an amd64 runner.
#     Expect tens of minutes - the lane needs the raised timeout and a buildx cache, exactly
#     as the native v2x-core-build lane already needs actions/cache on V2X_ECU/build/_deps.
#   * Network is required at build time: dependencies come in via CMake FetchContent (Vanetza,
#     nlohmann/json, googletest are git-cloned during `cmake -S`), so the build host must reach
#     github.com. There is no vendored/offline path.
#
# LICENSE POSTURE (report section 4, Scenario_Player decision D4): Vanetza is LGPLv3,
# so it is linked DYNAMICALLY - -DBUILD_SHARED_LIBS=ON below, and the resulting
# libvanetza_asn1*.so are copied into the runtime stage. Never link it statically here.

# ---------------------------------------------------------------------------
# Stage 1 - build
# ---------------------------------------------------------------------------
# debian:trixie-slim, NOT bookworm: V2X_ECU/CMakeLists.txt sets
# cmake_minimum_required(VERSION 3.28) (FetchContent_Declare EXCLUDE_FROM_ALL), and bookworm's
# apt cmake is 3.25 - it cannot configure this project at all. trixie ships 3.31.
# Tag is pinned (no :latest) and IDENTICAL in both stages so the glibc/libstdc++ the binary was
# linked against is the glibc/libstdc++ it runs on.
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

# Only what the BUILD needs. tests/ and tools/ are here for a configure-time reason, not a
# runtime one: CMakeLists.txt names tests/**.cpp and tools/golden_vectors/main.cpp in
# add_executable(), and CMake resolves those sources at CONFIGURE time - a missing file is a
# hard generate error even for targets this image never builds. They are build-stage only and
# never reach stage 2. doc/ is not copied at all (and is .dockerignore'd out of the context).
COPY CMakeLists.txt ./
COPY cmake/ cmake/
COPY contracts/ contracts/
COPY src/ src/
COPY tests/ tests/
COPY tools/ tools/

# BUILD_SHARED_LIBS=ON is the LGPLv3 dynamic-link posture (see header), not a size tweak.
RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON

# Target-scoped build - THIS is what keeps the test binaries (and gv_tool) out of the image:
# `--target v2x_ecu` builds the app and its link closure only, so no gtest executable is ever
# compiled. Bounded -j on purpose: a bare `-j` means unlimited jobs to make and OOMs on
# asn1_its' ~700 concurrent compilers (same reasoning as the CI lanes).
RUN cmake --build build --target v2x_ecu -j"$(nproc)"

# Stage the runtime artifacts into a flat, predictable /out so stage 2 never has to guess a
# FetchContent path (build/_deps/vanetza-build/vanetza/asn1/... is an implementation detail of
# the pin and moves with it).
#   - libvanetza*.so*  : found, not hardcoded.
#   - libboost*.so*    : whatever the link ACTUALLY resolved, taken from ldd rather than assumed.
#     The linked set is the ASN.1-only one (Vanetza::asn1 + Vanetza::asn1_its_r2 = asn1c-generated
#     C plus a thin C++ wrapper), so a compiled Boost runtime is not expected - Boost is a
#     header-only dependency on this path. Copying what ldd names costs nothing and removes the
#     guess; stage 2 verifies the result independently.
#   - the guard FAILS THE BUILD LOUDLY if no Vanetza .so was produced (e.g. a future Vanetza pin
#     declaring its libraries explicitly STATIC would silently ignore BUILD_SHARED_LIBS and
#     produce a statically-linked binary - a license-posture regression, not a cosmetic one).
RUN set -eu; \
    mkdir -p /out/lib; \
    cp build/v2x_ecu /out/v2x_ecu; \
    find build -name 'libvanetza*.so*' -exec cp -a {} /out/lib/ \; ; \
    ldd build/v2x_ecu | awk '/=> \//{print $3}' | grep -E '/lib(boost|vanetza)' \
      | xargs -r cp -a -t /out/lib/; \
    ls -A /out/lib | grep -q '^libvanetza_asn1' || { \
      echo "FATAL: no libvanetza_asn1*.so staged - Vanetza did not build shared."; \
      echo "       BUILD_SHARED_LIBS=ON is the LGPLv3 posture and must hold."; \
      exit 1; }; \
    echo "staged runtime libs:"; ls -l /out/lib; \
    ldd /out/v2x_ecu

# ---------------------------------------------------------------------------
# Stage 2 - runtime
# ---------------------------------------------------------------------------
FROM debian:trixie-slim

# tcpdump: capture.sh's two consumers (live [CAP] text + rotating pcap), HLD D5. Needs the
#   NET_RAW capability at runtime - granted by the blueprint, not by this image.
# coreutils: base64 (the pcap export encoding), mktemp and stdbuf, all used by capture.sh.
# mawk: capture.sh's /proc/net/dev counter fallback is awk-based.
# libstdc++6 / libgcc-s1 / libc6 / ldd come with the base image; the identical base tag in both
# stages is what makes that safe.
RUN apt-get update \
 && apt-get install -y --no-install-recommends \
      coreutils \
      mawk \
      tcpdump \
 && rm -rf /var/lib/apt/lists/*

# Debian's tcpdump drops privileges to the unprivileged `tcpdump` user when started as root, so
# the rotating `-w` files land as that user. Pre-create the HLD section 6 default PCAP_DIR owned
# by it (world-writable sticky fallback if the package did not create the user) - otherwise
# consumer (b) cannot write and capture.sh degrades to live-text-only. Creating the default
# DIRECTORY is not the same as baking the tunable: PCAP_DIR is still read from env by capture.sh.
RUN mkdir -p /data/capture \
 && { chown tcpdump:tcpdump /data/capture 2>/dev/null || chmod 1777 /data/capture; }

# Vanetza's shared objects go on the system loader path rather than travelling with an
# LD_LIBRARY_PATH env var: /usr/local/lib is already in Debian's ld.so.conf, so one ldconfig
# makes them resolvable for every process in the container (including tcpdump's -z re-exec of
# capture.sh) with no environment to forget. The binary still carries stage 1's build-tree
# RPATH; those directories do not exist here, the loader skips them and falls through to the
# ldconfig cache.
COPY --from=build /out/lib/ /usr/local/lib/
RUN ldconfig \
 && if ldd /usr/local/lib/libvanetza_asn1*.so* 2>/dev/null | grep -q 'not found'; then \
      echo "FATAL: staged Vanetza .so has unresolved dependencies in the runtime stage:"; \
      ldd /usr/local/lib/libvanetza_asn1*.so*; exit 1; \
    fi

# /app is the image workdir the blueprint's relative command resolves against.
WORKDIR /app
COPY --from=build /out/v2x_ecu ./v2x_ecu
COPY capture.sh entrypoint.sh ./

# chmod rather than trusting the git file mode: the mode is not guaranteed to survive every
# checkout/context path, and both scripts MUST be executable - entrypoint.sh is the container
# command, and tcpdump's `-z` re-execs capture.sh as a program (capture.sh disables its pcap
# consumer if it finds itself non-executable).
RUN chmod +x ./entrypoint.sh ./capture.sh

# Fail the BUILD, not the deploy, if the runtime stage is missing something the app links
# against (a Boost runtime, a Vanetza .so left behind): a "not found" here would otherwise
# surface as a node stuck at start-up with only a loader error in View Log.
RUN if ldd ./v2x_ecu | grep -q 'not found'; then \
      echo "FATAL: unresolved shared libraries in the runtime stage:"; ldd ./v2x_ecu; exit 1; \
    fi \
 && ldd ./v2x_ecu

# Deliberately NO ENV for any HLD section 6 tunable (LISTEN_PORT, ADA_ECU_HOST/PORT, FAULT_PLAN,
# INIT_RETRY_MAX, RETRY_BACKOFF_MS, DEDUPE_WINDOW_MS, EVENT_LOG_PATH, CAPTURE_FILTER, PCAP_DIR,
# CAPTURE_ROTATE_S): the blueprint injects them (node-v2x-ecu.md), and anything it omits must
# fall through to the single owner of that default - src/config/config.cpp for the app,
# capture.sh for the capture ones. An ENV here would silently shadow those defaults and make the
# image, not the config, the source of truth (CLAUDE.md governing principle 5).
#
# Deliberately NO USER: the platform runs the container as-is and the capture path needs the
# NET_RAW capability, so introducing an unprivileged user is out of scope here and risks
# breaking tcpdump - the one thing R6/R19 evidence depends on.
#
# CMD mirrors the blueprint `command: ["./entrypoint.sh"]` so the image is runnable standalone
# (docker run) for local checks; node-v2x-ecu.md's `command` remains the authoritative one.
CMD ["./entrypoint.sh"]
