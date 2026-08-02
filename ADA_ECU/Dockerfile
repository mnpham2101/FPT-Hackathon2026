FROM --platform=$BUILDPLATFORM debian:12-slim AS build

ARG TARGETARCH

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        build-essential \
        ca-certificates \
        cmake \
        nlohmann-json3-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . .

RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build \
    && ctest --test-dir build --output-on-failure

FROM debian:12-slim AS runtime

RUN apt-get update \
    && apt-get install -y --no-install-recommends ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY --from=build /src/build/ada_ecu /app/ada_ecu
COPY --from=build /src/config /app/config
COPY --from=build /src/schemas /app/schemas
COPY --from=build /src/testdata /app/testdata

ENTRYPOINT ["/app/ada_ecu"]
CMD ["--config", "/app/config/ada-ecu.conf"]
