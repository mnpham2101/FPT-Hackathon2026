# ADA ECU Phase 2/3/4 test and demo guide

Canonical folder: `ADA_ECU/`.

This guide proves the current ADA scope:

1. Phase 2 receives V2X R2 data and maps it into tracked objects.
2. Phase 3 decodes a provided video and emits R3 own-sensor objects.
3. Phase 4 fuses own-sensor vehicle B with V2X-relayed vehicle C and emits R4 warning JSON.

## 1. Build runtime

Use runtime-only build when network access is unavailable:

```sh
cmake -S ADA_ECU -B ADA_ECU/build-runtime -DADA_BUILD_CONTRACT_TESTS=OFF
cmake --build ADA_ECU/build-runtime
ctest --test-dir ADA_ECU/build-runtime --output-on-failure
```

The full build also enables the frozen contract tests from `ADA_ECU/src/contracts`:

```sh
cmake -S ADA_ECU -B ADA_ECU/build
cmake --build ADA_ECU/build
ctest --test-dir ADA_ECU/build --output-on-failure
```

## 2. Prepare Python detector environment

Do not install into the Homebrew-managed system Python. Use a venv:

```sh
python3 -m venv ADA_ECU/.venv
source ADA_ECU/.venv/bin/activate
python -m pip install -r ADA_ECU/requirements.txt
```

## 3. Prove video can emit object detections

Run the deterministic placeholder smoke check against the committed demo clip:

```sh
python ADA_ECU/tools/smoke_demo_video_detector.py
```

Expected result:

```text
phase3 demo video detector: pass (5 R3 objects)
```

Download the YOLO ONNX model for ML detection:

```sh
python ADA_ECU/tools/download_yolo_model.py
```

Run the ML smoke check:

```sh
python ADA_ECU/tools/smoke_ml_video_detector.py
```

Expected result:

```text
phase3 ML video detector: pass (5 R3 objects)
```

To see the raw ML R3 JSONL:

```sh
python ADA_ECU/tools/video_detector.py \
  --video ADA_ECU/media/ego-b-occluding-c.mp4 \
  --backend yolo-onnx \
  --model ADA_ECU/models/yolov8n.onnx \
  --every-n-frames 20 \
  --limit 5 \
  --confidence 0.20 \
  --log-detections
```

`--log-detections` writes ML bbox evidence to stderr. stdout remains R3 JSONL.

## 4. Prove video R3 can drive ADA Phase 4 output

Save the ML video detector output:

```sh
python ADA_ECU/tools/video_detector.py \
  --video ADA_ECU/media/ego-b-occluding-c.mp4 \
  --backend yolo-onnx \
  --model ADA_ECU/models/yolov8n.onnx \
  --every-n-frames 20 \
  --limit 5 \
  --confidence 0.20 > /tmp/r3_own_sensor.jsonl
```

Run ADA with mock V2X C plus the video-derived B sample:

```sh
ADA_ECU/build-runtime/ada_ecu \
  --config ADA_ECU/config/ada-ecu.conf \
  --mock \
  --own-sensor-sample /tmp/r3_own_sensor.jsonl
```

Expected R4 evidence:

- `type` is `warning`
- `warningType` is `nlos_obstruction`
- `riskState` becomes `high` or `low` depending on distance/gate state
- `trackedObjects` contains `own:B` from video and `v2x:*` from V2X
- `geometry.vehicleB` and `geometry.vehicleC` are populated

## 5. IVI realtime integration check

Run a mock IVI receiver:

```sh
python ADA_ECU/tools/mock_ivi_receiver.py --host 127.0.0.1 --port 47300 --max 1
```

In another terminal, send ADA R4 over UDP:

```sh
IVI_HOST=127.0.0.1 IVI_PORT=47300 \
ADA_ECU/build-runtime/ada_ecu \
  --config ADA_ECU/config/ada-ecu.conf \
  --mock \
  --own-sensor-sample /tmp/r3_own_sensor.jsonl
```

The realtime path to IVI is UDP JSON R4.

## 6. Deterministic timeline demo for review

Use this when the reviewer asks for the exact story:

- `t=1.00s`: video/R3 has vehicle B.
- `t=1.01s`: V2X/R2 has vehicle C.
- `t=1.02s`: ADA sends R4 to IVI with both vehicles.

Run:

```sh
python ADA_ECU/tools/demo_timeline_ivi.py
```

Expected output shape:

```text
t=1.00s video: vehicle B detected from R3 own_sensor sample
t=1.01s v2x: vehicle C received from R2 v2x sample
t=1.02s ada: sending R4 to IVI UDP 127.0.0.1:<port>
t=1.02s ivi: received R4 warning with vehicleB and vehicleC
{"schemaVersion":1,"type":"warning",...}
```

The final R4 JSON must contain:

- `trackedObjects[].id == "own:B"` with `timestamps.measured == 1000`
- `trackedObjects[].id == "v2x:1201:7"` with timestamp `1010`
- `geometry.vehicleB`
- `geometry.vehicleC`
