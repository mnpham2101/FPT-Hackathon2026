# ADA_ECU — ADA ECU node (R3, R12–R15)

ADA is the ego-side perception and fusion node:

- Phase 2: R3 `TrackedObject` store, R13 admission gate, config, evidence logs, and mock inputs.
- Phase 3: Python detector subprocess emits R3 JSONL with `source = "own_sensor"`.
- Phase 4: R2 V2X object input becomes `source = "v2x_relayed"` tracks, then the CRA emits R4 warnings for IVI.

The folder name is intentionally `ADA_ECU/`, matching the project node layout. Older lowercase `ada-ecu/` content has been migrated here.

## Contracts and runtime

| Area | Files |
|---|---|
| Frozen shared contracts | `contracts/`, `src/contracts/`, `tests/contracts/` |
| Runtime core | `include/ada/`, `src/*.cpp` |
| Runtime config | `config/ada-ecu.conf` |
| Detector | `tools/video_detector.py`, `requirements.txt` |
| Runtime docs | `docs/` |
| Test data | `testdata/`, `tests/track_store_tests.cpp` |

## Build

Install local dependencies:

```sh
brew install cmake nlohmann-json
```

Runtime-only build, useful when network access for FetchContent is unavailable:

```sh
cmake -S ADA_ECU -B ADA_ECU/build-runtime -DADA_BUILD_CONTRACT_TESTS=OFF
cmake --build ADA_ECU/build-runtime
ctest --test-dir ADA_ECU/build-runtime --output-on-failure
```

Full contract + runtime build:

```sh
cmake -S ADA_ECU -B ADA_ECU/build
cmake --build ADA_ECU/build
ctest --test-dir ADA_ECU/build --output-on-failure
```

Install detector dependencies in a virtual environment:

```sh
python3 -m venv ADA_ECU/.venv
source ADA_ECU/.venv/bin/activate
python -m pip install -r ADA_ECU/requirements.txt
```

## Run mock/demo

```sh
ADA_ECU/build-runtime/ada_ecu --config ADA_ECU/config/ada-ecu.conf --mock
```

External mock V2X sender:

```sh
ADA_ECU/build-runtime/ada_ecu --config ADA_ECU/config/ada-ecu.conf --listen-once
python3 ADA_ECU/tools/mock_v2x_sender.py --host 127.0.0.1 --port 46002
```

ADA → IVI R4 UDP smoke:

```sh
python3 ADA_ECU/tools/mock_ivi_receiver.py --host 127.0.0.1 --port 46004
ADA_ECU/build-runtime/ada_ecu --config ADA_ECU/config/ada-ecu.conf --mock
```

## Video detector proof

The repo now contains a team-provided demo clip:

```txt
ADA_ECU/media/ego-b-occluding-c.mp4
```

Run placeholder video detection:

```sh
source ADA_ECU/.venv/bin/activate
python ADA_ECU/tools/video_detector.py --video ADA_ECU/media/ego-b-occluding-c.mp4 --backend placeholder --every-n-frames 30 --limit 5
```

Run the placeholder detector smoke test:

```sh
python ADA_ECU/tools/smoke_video_detector.py
```

Run real ML vehicle detection:

```sh
python ADA_ECU/tools/download_yolo_model.py
python ADA_ECU/tools/smoke_ml_video_detector.py
python ADA_ECU/tools/video_detector.py \
  --video ADA_ECU/media/ego-b-occluding-c.mp4 \
  --backend yolo-onnx \
  --model ADA_ECU/models/yolov8n.onnx \
  --every-n-frames 20 \
  --limit 5 \
  --confidence 0.20 \
  --log-detections
```

## CarSky deployment overrides

Local defaults stay developer-friendly. Deployment should override ports through env:

```sh
V2X_LISTEN_PORT=47200
IVI_HOST=10.99.0.13
IVI_PORT=47300
```
