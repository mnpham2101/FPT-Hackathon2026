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

- C++17 ADA runtime scaffold in `ada-ecu/`.
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
- Placeholder backend for real video decode before YOLO integration.
- R3 JSONL output compatible with ADA ingest.
- Smoke test for the detector path.

Current behavior:

- Emits `own:B`.
- Uses `source = "own_sensor"`.
- Emits distance/position fields needed by Phase 4 fusion.
- Supports `--video`, `--synthetic`, `--backend placeholder`, `--every-n-frames`, and `--limit`.

Example:

```sh
python ada-ecu/tools/video_detector.py --synthetic 2
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
cmake --build ada-ecu/build
ctest --test-dir ada-ecu/build --output-on-failure
python3 -m py_compile ada-ecu/tools/mock_ivi_receiver.py ada-ecu/tools/mock_v2x_sender.py ada-ecu/tools/video_detector.py ada-ecu/tools/smoke_video_detector.py
ada-ecu/.venv/bin/python ada-ecu/tools/smoke_video_detector.py
ada-ecu/build/ada_ecu --config ada-ecu/config/ada-ecu.conf --mock
```

Current result:

| Check | Result |
|---|---|
| C++ build | Pass |
| ADA unit tests | Pass |
| Python tool syntax | Pass |
| Phase 3 detector smoke through ADA venv | Pass |
| ADA mock R2/R3/R4 loopback | Pass with sandbox escalation for UDP bind |

## Open Integration Notes for Other Owners

These are requests to coordinate with other phases, not ADA-owned changes:

| Owner scope | Request |
|---|---|
| Phase 1 / V2X ECU | Send R2 JSON to ADA deploy port `47200`; object distance should be derived consistently from position. |
| Phase 5 / IVI ECU | Consume R4 `geometry.vehicleB` / `geometry.vehicleC` and `riskState = low | medium | high`; listen on deploy port `47300`. |
| Deployment / CarSky | Use env overrides instead of editing local ADA config: `V2X_LISTEN_PORT=47200`, `IVI_HOST=10.99.0.13`, `IVI_PORT=47300`. |

## Status

ADA Phase 2–4 is ready for MR as an ego-side mock/demo runtime. Full closure still depends on integration with Phase 1 live R2 and Phase 5 IVI consumption.
