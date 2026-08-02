# ADA ECU Phase 2–4 Report

## Scope

ADA ECU owns the ego-side fusion path:

| Phase | ADA responsibility | Input | Output |
|---|---|---|---|
| Phase 2 | Contract/runtime scaffold, R3 track store, R13 admission gate | R2 V2X object JSON, mock samples | TrackedObject store, evidence logs, R4 warning smoke |
| Phase 3 | Video detector skeleton behind the R3 JSONL seam | Video file or synthetic frames | `own_sensor` R3 object for vehicle B |
| Phase 4 | NLOS fusion and risk warning | R2 relayed C + R3 own-sensor B | R4 ADA→IVI warning JSON over UDP |

Out of ADA scope:

- Phase 1 V2X decoding and CPM generation.
- Phase 5 IVI application behavior.
- CarSky deployment execution and acceptance evidence collection.

## Runtime Architecture

```text
V2X ECU / mock R2
  -> UDP R2 receiver
  -> R2 mapper
  -> TrackStore source=v2x_relayed

Video detector / mock R3 JSONL
  -> detector JSONL ingest
  -> R3 mapper
  -> TrackStore source=own_sensor

TrackStore
  -> NLOS risk assessor
  -> R4 warning builder
  -> UDP R4 sender
```

## Phase 2 — Contract and Store Scaffold

Implemented:

- C++17 ADA runtime scaffold in `ADA_ECU/`.
- Config loader for gate and network tunables.
- R2/R3/R4 local schemas and sample payloads.
- R3 `TrackStore` with source-aware object state.
- R13 gate lifecycle:
  - `not_tracked`
  - `tentative`
  - `tracked`
  - timeout/exit demotion
- JSONL evidence logging:
  - `own_sensor_rx`
  - `r2_rx`
  - `track_transition`
  - `risk_event`
  - `r4_tx`

Key files:

| Area | Files |
|---|---|
| Config | `include/ada/config.hpp`, `src/config.cpp`, `config/ada-ecu.conf` |
| Track store | `include/ada/track_store.hpp`, `src/track_store.cpp` |
| Contracts/mappers | `schemas/`, `testdata/`, `src/r2_mapper.cpp`, `src/r3_mapper.cpp` |
| Tests | `tests/track_store_tests.cpp` |

## Phase 3 — Video Detector Skeleton

Implemented:

- Python OpenCV-based detector seam at `tools/video_detector.py`.
- Synthetic mode for deterministic development without video assets.
- Placeholder backend for deterministic smoke tests.
- YOLO ONNX backend for real ML vehicle detection from video frames.
- Bounding-box evidence logging with class, confidence, bbox, and distance estimate.
- R3 JSONL output compatible with ADA ingest.
- Smoke test for the detector path.

Current behavior:

- Emits `own:B` from either placeholder or ML-selected vehicle B.
- Uses `source = "own_sensor"`.
- Emits distance/position fields needed by Phase 4 fusion.
- Supports `--video`, `--synthetic`, `--backend placeholder`, `--backend yolo-onnx`, `--model`, `--confidence`, `--every-n-frames`, and `--limit`.

Example:

```sh
python ADA_ECU/tools/video_detector.py --synthetic 2
```

## Phase 4 — Fusion and Warning Runtime

Implemented:

- UDP R2 receiver for live/mock V2X input.
- R2 ingestion into `TrackStore` as `source = "v2x_relayed"`.
- Source-specific timeout handling: V2X expiry does not remove `own_sensor` B.
- NLOS risk transition logic:
  - enter gate → high-risk warning event
  - repeated in-gate update → no duplicate warning
  - exit/timeout → low-risk transition
- R4 warning builder:
  - `geometry.ego`
  - `geometry.vehicleB`
  - `geometry.vehicleC`
  - debug/additive `trackedObjects` for local evidence
- UDP sender toward IVI.
- Env overrides for CarSky deploy ports without changing local config:
  - `V2X_LISTEN_PORT`
  - `IVI_HOST`
  - `IVI_PORT`

R4 example shape:

```json
{
  "schemaVersion": 1,
  "type": "warning",
  "warningType": "nlos_obstruction",
  "riskState": "high",
  "geometry": {
    "ego": { "x": 0, "y": 0 },
    "vehicleB": { "x": 12, "y": 0.2 },
    "vehicleC": { "x": 37.4, "y": 1.4 }
  }
}
```

## Verification

Commands run for the ADA scope:

```sh
cmake --build ADA_ECU/build
ctest --test-dir ADA_ECU/build --output-on-failure
python3 -m py_compile ADA_ECU/tools/mock_ivi_receiver.py ADA_ECU/tools/mock_v2x_sender.py ADA_ECU/tools/video_detector.py ADA_ECU/tools/smoke_video_detector.py
ADA_ECU/.venv/bin/python ADA_ECU/tools/smoke_video_detector.py
ADA_ECU/build/ada_ecu --config ADA_ECU/config/ada-ecu.conf --mock
```

Current result:

| Check | Result |
|---|---|
| C++ build | Pass |
| ADA unit tests | Pass |
| Python tool syntax | Pass |
| Phase 3 placeholder detector smoke through ADA venv | Pass |
| Phase 3 ML detector smoke on project video | Pass |
| ADA mock R2/R3/R4 loopback | Pass with sandbox escalation for UDP bind |

### Latest Local Verification With Project Video

#### YOLO11n full-clip acceptance run

The runtime model is pretrained `yolo11n.onnx`; no training or fine-tuning is
used. On 2026-08-03, the complete 10-second, 200-frame project clip was run at
a stride of four with calibrated `CAMERA_FOCAL_PX=2000`:

| KPI | Result | Verdict |
|---|---:|---|
| Sampled frames / R3 detections | 50 / 50 | Pass |
| Detection coverage | 100% | Pass (at least 90%) |
| Effective inference rate | 20.395 Hz | Pass (at least 5 Hz) |
| Model/video warm-up | 0.2234 s | Pass |
| First to last B distance | 31.674 to 7.249 m | Approaching |
| Observed distance range | 7.154 to 39.247 m | Covers demo gate region |
| 30 m gate crossings | 1 | Pass |
| Non-increasing raw steps | 87.76% | Minor bbox jitter; overall trend decreases |
| Zero-C structural check | 50 objects examined | Pass |

The evidence JSONL was then ingested by the ADA C++ runtime with the R2 sample.
R4 contained tracked `own:B` at 7.249 m and tracked `v2x:1201:7` at 25.4 m,
with composed `vehicleC.x=32.649`.

The same full-clip benchmark inside the `linux/arm64` image produced 50/50
detections at 17.543 Hz with 0.2949 s warm-up and one 30 m gate crossing.

#### Linux/ARM64 container verification

The combined C++ and Python image was built for the CarSky target architecture:

```sh
docker buildx build --platform linux/arm64 --provenance=false --sbom=false \
  --load -t m1-ada-ecu:latest ADA_ECU
```

Evidence recorded on 2026-08-03:

- Build-stage CTest passed `14/14`.
- The C++ mock runtime emitted R4 with `trackedObjects` containing `own:B`
  (`own_sensor`) and `v2x:1201:7` (`v2x_relayed`).
- YOLO ONNX decoded the packaged demo video inside the ARM64 image and emitted
  five R3 `own:B` objects at timestamps 0, 1000, 2000, 3000, and 4000 ms.
- Native `aarch64` wheels resolved for OpenCV, NumPy, and ONNX Runtime.

The ONNX Runtime `Unknown CPU vendor` warning under Docker Desktop
virtualization was informational; inference completed successfully.

Date: 2026-08-02.

Video used:

```sh
ADA_ECU/media/ego-b-occluding-c.mp4
```

Video metadata:

| Field | Value |
|---|---|
| Container/codec | MP4 / H.264 |
| Resolution | 1280x720 |
| Frame rate | 20 fps |
| Duration | 10 seconds |
| Size | ~5.0 MB |

Phase 3 placeholder video detector smoke:

```sh
python ADA_ECU/tools/smoke_demo_video_detector.py
```

Observed result:

```text
phase3 demo video detector: pass (5 R3 objects)
```

ML model setup:

```sh
python ADA_ECU/tools/download_yolo_model.py
```

Observed result:

```text
downloaded model: ADA_ECU/models/yolo11n.onnx (pretrained artifact)
```

Phase 3 ML detector smoke:

```sh
python ADA_ECU/tools/smoke_ml_video_detector.py
```

Observed result:

```text
phase3 ML video detector: pass (5 R3 objects)
```

Observed ML bbox evidence:

```json
{"event":"ml_detection","frame":0,"timestampMs":0,"class":"car","confidence":0.752,"bbox":[649.0,416.9,762.7,498.3],"distance":31.674}
{"event":"ml_detection","frame":20,"timestampMs":1000,"class":"truck","confidence":0.706,"bbox":[685.5,362.4,777.2,463.2],"distance":39.247}
{"event":"ml_detection","frame":40,"timestampMs":2000,"class":"bus","confidence":0.685,"bbox":[708.9,341.8,826.5,450.5],"distance":30.612}
{"event":"ml_detection","frame":60,"timestampMs":3000,"class":"bus","confidence":0.91,"bbox":[736.7,328.3,867.9,451.1],"distance":27.448}
{"event":"ml_detection","frame":80,"timestampMs":4000,"class":"bus","confidence":0.867,"bbox":[704.2,325.0,849.2,456.8],"distance":24.82}
```

ML video-to-R3 command:

```sh
python ADA_ECU/tools/video_detector.py \
  --video ADA_ECU/media/ego-b-occluding-c.mp4 \
  --backend yolo-onnx \
  --model ADA_ECU/models/yolo11n.onnx \
  --every-n-frames 20 \
  --limit 5 \
  --confidence 0.20 \
  --log-detections > /tmp/ada_r3_ml_from_project_video.jsonl
```

Observed R3 output count:

```text
5 /tmp/ada_r3_ml_from_project_video.jsonl
```

Observed R3 evidence:

```json
{"id":"own:B","class":"vehicle","source":"own_sensor","position":{"x":9.01,"y":-5.496,"confidence":0.271},"distance":9.01,"speed":0.0,"confidence":0.271,"state":"tentative","timestamps":{"measured":0,"received":0,"lastUpdated":0}}
{"id":"own:B","class":"vehicle","source":"own_sensor","position":{"x":15.559,"y":1.505,"confidence":0.718},"distance":15.559,"speed":0.0,"confidence":0.718,"state":"tentative","timestamps":{"measured":1000,"received":1000,"lastUpdated":1000}}
{"id":"own:B","class":"vehicle","source":"own_sensor","position":{"x":13.624,"y":1.922,"confidence":0.648},"distance":13.624,"speed":0.0,"confidence":0.648,"state":"tentative","timestamps":{"measured":2000,"received":2000,"lastUpdated":2000}}
{"id":"own:B","class":"vehicle","source":"own_sensor","position":{"x":12.381,"y":2.21,"confidence":0.664},"distance":12.381,"speed":0.0,"confidence":0.664,"state":"tentative","timestamps":{"measured":3000,"received":3000,"lastUpdated":3000}}
{"id":"own:B","class":"vehicle","source":"own_sensor","position":{"x":11.104,"y":1.679,"confidence":0.885},"distance":11.104,"speed":0.0,"confidence":0.885,"state":"tentative","timestamps":{"measured":4000,"received":4000,"lastUpdated":4000}}
```

Phase 4 fusion command using video-derived B plus mock V2X C:

```sh
ADA_ECU/build-runtime/ada_ecu \
  --config ADA_ECU/config/ada-ecu.conf \
  --mock \
  --own-sensor-sample /tmp/ada_r3_ml_from_project_video.jsonl
```

Observed R4 evidence:

```json
{
  "schemaVersion": 1,
  "type": "warning",
  "warningType": "nlos_obstruction",
  "riskState": "high",
  "trackedObjects": [
    {
      "id": "own:B",
      "class": "vehicle",
      "source": "own_sensor",
      "position": { "x": 11.104, "y": 1.679, "confidence": 0.885 },
      "distance": 11.104,
      "speed": 0,
      "confidence": 0.885,
      "state": "tracked",
      "timestamps": { "measured": 4000, "received": 4000, "lastUpdated": 4000 }
    },
    {
      "id": "v2x:1201:7",
      "class": "vehicle",
      "source": "v2x_relayed",
      "position": { "x": 25, "y": 1.2, "confidence": 0.9 },
      "distance": 25.4,
      "speed": 15.2,
      "confidence": 0.95,
      "state": "tracked"
    }
  ],
  "geometry": {
    "ego": { "x": 0, "y": 0 },
    "vehicleB": { "x": 11.104, "y": 1.679 },
    "vehicleC": { "x": 36.504, "y": 2.879 }
  }
}
```

Interpretation:

- Phase 3 proves the committed project video can be decoded and detected with YOLO ONNX into R3 `own_sensor` objects.
- Phase 4 proves ADA can fuse ML-derived vehicle B with mock V2X vehicle C.
- The emitted R4 contains both `trackedObjects` entries (`own:B`, `v2x:1201:7`) and composed `geometry.vehicleB` / `geometry.vehicleC`.
- Distance is a bounding-box estimate, not calibrated ground truth.

## Open Integration Notes for Other Owners

These are requests to coordinate with other phases, not ADA-owned changes:

| Owner scope | Request |
|---|---|
| Phase 1 / V2X ECU | Send R2 JSON to ADA deploy port `47200`; object distance should be derived consistently from position. |
| Phase 5 / IVI ECU | Consume R4 `geometry.vehicleB` / `geometry.vehicleC` and `riskState = low | medium | high`; listen on deploy port `47300`. |
| Deployment / CarSky | Use env overrides instead of editing local ADA config: `V2X_LISTEN_PORT=47200`, `IVI_HOST=10.99.0.13`, `IVI_PORT=47300`. |

## Status

ADA Phase 2–4 is ready for MR as an ego-side mock/demo runtime. Full closure still depends on integration with Phase 1 live R2 and Phase 5 IVI consumption.
