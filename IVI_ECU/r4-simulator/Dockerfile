# syntax=docker/dockerfile:1
# m1-r4-sim:latest - the R4 simulator, the mini-blueprint ADA node's stand-in image.
# Build context is IVI_ECU/ (an HLD-flagged deviation from "Dockerfile at the folder
# root"): the build needs the Gradle wrapper, settings, :contract and :r4-simulator,
# and reads nothing outside IVI_ECU/ - self-containment holds.

# Build stage on the builder's own platform: JVM bytecode is architecture-neutral,
# so Gradle never runs under QEMU emulation; only the runtime stage is arm64.
FROM --platform=$BUILDPLATFORM eclipse-temurin:17-jdk AS build
WORKDIR /src
# Deliberately NOT `COPY . .`: the :app Android module cannot configure without an
# Android SDK. settings.gradle.kts still includes :app, but a project whose build
# file is absent configures as an empty project - copying everything EXCEPT app/
# keeps this build SDK-free.
COPY gradlew settings.gradle.kts build.gradle.kts gradle.properties ./
COPY gradle/ gradle/
COPY contract/ contract/
COPY r4-simulator/ r4-simulator/
RUN chmod +x gradlew && ./gradlew :r4-simulator:installDist --no-daemon

FROM eclipse-temurin:17-jre
WORKDIR /app
COPY --from=build /src/r4-simulator/build/install/r4-simulator/ /app/
COPY --from=build /src/r4-simulator/scenarios/ /app/scenarios/
COPY r4-simulator/entrypoint.sh /app/entrypoint.sh
RUN chmod +x /app/entrypoint.sh
# Blueprint node config command: ["./entrypoint.sh"] - relative, resolved against /app.
ENTRYPOINT ["./entrypoint.sh"]
