FROM python:3.11-slim AS build

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        build-essential \
        ca-certificates \
        cmake \
        git \
        nlohmann-json3-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . .

RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build \
    && ctest --test-dir build --output-on-failure

FROM python:3.11-slim AS runtime

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        ca-certificates \
        libgomp1 \
        tcpdump \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY requirements.txt /app/detector/requirements.txt
RUN python -m pip install --no-cache-dir -r /app/detector/requirements.txt

COPY --from=build /src/build/ada_ecu /app/ada_ecu
COPY --from=build /src/config /app/config
COPY --from=build /src/schemas /app/schemas
COPY --from=build /src/testdata /app/testdata
COPY detector /app/detector/detector
COPY tools /app/detector/tools
COPY media /app/media
COPY models/yolo11n.onnx /app/models/yolo11n.onnx
COPY capture.sh entrypoint.sh /app/

RUN chmod +x /app/capture.sh /app/entrypoint.sh

ENV V2X_LISTEN_HOST=0.0.0.0 \
    V2X_LISTEN_PORT=47200 \
    IVI_HOST=10.99.0.13 \
    IVI_PORT=47300 \
    TRACK_TIMEOUT_MS=1000 \
    CONFIRM_HITS=3 \
    EVENT_LOG_PATH=ada-events.jsonl \
    DETECTOR_ENABLED=true \
    DETECTOR_RESTART_MAX=3 \
    DETECTOR_CMD="python3 /app/detector/tools/video_detector.py --video /app/media/ego-b-occluding-c.mp4 --backend yolo-onnx --model /app/models/yolo11n.onnx --every-n-frames 4 --confidence 0.20 --realtime --loop" \
    CRA_ENABLED=nlos_obstruction \
    GATE_ENTER_M=30 \
    GATE_EXIT_M=35 \
    RISK_NEAR_M=50 \
    RISK_CRITICAL_M=30 \
    RISK_DWELL_MS=300 \
    MODEL_PATH=/app/models/yolo11n.onnx \
    VIDEO_PATH=/app/media/ego-b-occluding-c.mp4

ENTRYPOINT ["/app/entrypoint.sh"]
CMD ["--config", "/app/config/ada-ecu.conf"]
