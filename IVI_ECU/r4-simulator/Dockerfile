# syntax=docker/dockerfile:1
# m1-r4-sim:latest - the R4 simulator, the mini-blueprint ADA node's stand-in image.
#
# Build context is IVI_ECU/ (an HLD-flagged deviation from "Dockerfile at the folder root"):
# this build compiles the R4 model out of the application's own tree rather than keeping a
# private copy, so the context must span both folders. It reads nothing outside IVI_ECU/ and
# writes nothing at all - self-containment holds.
#
#     docker buildx build --platform linux/arm64 --provenance=false --sbom=false \
#       -f IVI_ECU/r4-simulator/Dockerfile -t m1-r4-sim:latest IVI_ECU/
#
# Build stage runs on the builder's own platform: JVM bytecode is architecture-neutral, so
# Gradle never runs under QEMU emulation; only the runtime stage is arm64.
FROM --platform=$BUILDPLATFORM gradle:8.13-jdk17 AS build
WORKDIR /src

# Copied deliberately narrowly, NOT `COPY . .`: the :app Android module cannot configure
# without an Android SDK, and this build does not need it. Only the simulator plus the two
# read-only paths its build.gradle.kts compiles from are brought in.
COPY r4-simulator/ r4-simulator/
COPY app/src/main/java/com/hackathon/v2x/ivi/model/ app/src/main/java/com/hackathon/v2x/ivi/model/
COPY app/src/test/resources/ app/src/test/resources/

# --no-daemon: one-shot container build, so a background daemon is pure overhead.
# The image's own gradle is used rather than the IVI wrapper, because this is a standalone
# Gradle build (r4-simulator/settings.gradle.kts) and the wrapper belongs to the IVI project.
RUN gradle -p r4-simulator installDist --no-daemon

FROM eclipse-temurin:17-jre
WORKDIR /app
COPY --from=build /src/r4-simulator/build/install/r4-simulator/ /app/
COPY --from=build /src/r4-simulator/scenarios/ /app/scenarios/
COPY r4-simulator/entrypoint.sh /app/entrypoint.sh
RUN chmod +x /app/entrypoint.sh
# Blueprint node config command: ["./entrypoint.sh"] - relative, resolved against /app.
ENTRYPOINT ["./entrypoint.sh"]
