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

### Final hardening before CarSky

- CRA evaluates the composed ego-to-C distance `d_AC = d_AB + d_BC`; configured bands are
  high at ≤30 m and medium at ≤50 m, with TTC recorded when closing.
- A periodic fusion tick commits the configured 300 ms dwell without requiring another R2 packet.
- The saved-video detector runs with real-time pacing and looping in CarSky so B remains fresh.
- R4 is serialized through the shared contract binding and carries ratified additive
  `trackedObjects` for B and C.
- Full R3 snapshots, assessment values, transition records, delivery status, and the complete R4
  body are preserved in the EVT log.
- Phase 4 CI exercises the detector seam, live R2 UDP, live R4 UDP receiver, schema validation,
  `medium → high → low`, and an out-of-range zero-R4 negative control.
- Rotating ADA→IVI captures are exported through CarSky View Log with SHA-256-protected PCAP
  markers.

Final local evidence (2026-08-03): 28 EVT records, three risk transitions, three successfully
delivered R4 datagrams, and zero R4 in the out-of-range negative run. Docker `linux/arm64` built
successfully and all 14 C++/contract tests passed inside the image.

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

- Build-stage CTest passed `18/18` with GCC 14 on `linux/arm64`.
- Host-side Python tool tests passed `17/17`; detector seam tests passed `5/5`.
- Clip preflight decoded `200/200` frames (1280x720, 20 fps, 10 seconds).
- The C++ mock runtime emitted R4 with `trackedObjects` containing `own:B`
  (`own_sensor`) and `v2x:1201:7` (`v2x_relayed`).
- The schema-aware evidence checker passed the captured local chain with
  `events=11`, `riskTransitions=1`, and `r4Tx=1`.
- YOLO ONNX decoded the packaged demo video inside the ARM64 image and emitted
  five R3 `own:B` objects at timestamps 0, 1000, 2000, 3000, and 4000 ms.
- Native `aarch64` wheels resolved for OpenCV, NumPy, and ONNX Runtime.

The latest local acceptance image is
`m1-ada-ecu:phase24-acceptance-local` (`linux/arm64`). The build exposed and
fixed one Linux portability issue: `risk_assessor.cpp` now includes
`<stdexcept>` directly instead of relying on a transitive macOS/Clang include.
The packaged mock fixture now contains the three own-sensor observations
required by `CONFIRM_HITS=3`, so vehicle B is `tracked` before R2 vehicle C is
fused. Registry publication and Room deployment were then completed as recorded
below; IVI receipt and PCAP inspection remain separate downstream evidence.

The CarSky-ready image was subsequently published as
`registry.hackathon-2.carsky.io/m1-ada-ecu:20260803-phase24-acceptance-2`.
The remote registry reports a single OCI image manifest with digest
`sha256:244143189698649041b0191c47a0590bef5e5aa0fab31124831bc1758796a005`;
it was built with provenance and SBOM attestations disabled for CarSky
compatibility. The earlier `20260803-phase24-acceptance-1` tag is retained as
immutable build history but is not the deployment candidate because its OCI
index also contains an attestation manifest.

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

### CarSky deployment acceptance

ADA Phase 2–4 was deployed on the custom CarSky tenant on 2026-08-03 using:

```text
registry.hackathon-2.carsky.io/m1-ada-ecu:20260803-phase24-acceptance-2
sha256:244143189698649041b0191c47a0590bef5e5aa0fab31124831bc1758796a005
```

The deployment reached `5/5 nodes ready`. The ADA node used static Ethernet
address `10.99.0.12/24`, received R2 on UDP port `47200`, and targeted IVI at
`10.99.0.13:47300`. For the short deterministic demo, the deploy-only overrides
were `RISK_NEAR_M=60` and `RISK_DWELL_MS=0`; the source defaults remain 50 m
and 300 ms.

Observed live evidence:

| Check | Evidence | Result |
|---|---|---|
| Real video detection | YOLO emitted tracked `own:B`; observed B distance `16.595 m` at emission | Pass |
| Live V2X input | `v2x:1201:7` crossed the 30 m admission gate at `29.774 m` | Pass |
| Fusion/CRA | `distanceAC=46.369 m`, `riskState=medium`, rationale `composed_distance_threshold` | Pass |
| R4 content | `trackedObjects` contained tracked `own:B` and `v2x:1201:7`; geometry contained ego, B and C | Pass |
| ADA UDP transmission | `r4_tx` targeted `10.99.0.13:47300` with `sent=true`, length 987 bytes | Pass |

The admission transition, assessment, risk transition, and R4 transmission all
used timestamp `1785771523404`, proving that ADA emitted the warning in the
same processing tick after C became tracked. `sent=true` proves the ADA-side
UDP `sendto()` succeeded; an IVI-side receive/parse log is still required to
claim transport delivery and HMI acceptance for the Phase 5 scope.

## Open Integration Notes for Other Owners

These are requests to coordinate with other phases, not ADA-owned changes:

| Owner scope | Request |
|---|---|
| Phase 1 / V2X ECU | Live R2 delivery to ADA port `47200` passed on CarSky; retain consistent position/distance derivation. |
| Phase 5 / IVI ECU | Consume R4 `geometry.vehicleB` / `geometry.vehicleC` and `riskState = low | medium | high`; listen on deploy port `47300`. |
| Deployment / CarSky | ADA deploy and ADA-side R4 transmission passed; preserve the immutable image tag and canonical environment block. |

## Status

ADA Phase 2–4 passed local tests, ARM64 container verification, live CarSky R2
ingest, real-video B detection, B+C fusion, CRA assessment, and ADA-side R4
transmission. It is ready for commit and MR. Full product end-to-end closure
still requires the Phase 5 IVI receive/parse or HMI evidence.
