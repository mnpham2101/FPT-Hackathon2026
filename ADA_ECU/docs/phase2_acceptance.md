# Phase 2 Acceptance Checklist — ADA Scaffolding

Phase 2 objective: stand up the ADA skeleton, R3 track store, and R13 admission state machine on mock input, before real video detection.

## Scope Delivered

| Area | Status | Artifact |
|---|---:|---|
| C++17 ADA skeleton | Done | `CMakeLists.txt`, `src/main.cpp`, `include/ada/*` |
| R2 V2X ECU → ADA contract | Done | `schemas/r2_v2x_object.schema.json`, `testdata/r2_v2x_object.sample.json` |
| R3 TrackedObject contract | Done | `schemas/r3_tracked_object.schema.json`, `testdata/r3_own_sensor.sample.json`, `testdata/r3_v2x_relayed.sample.json`, `testdata/r3_own_sensor.jsonl` |
| R4 ADA → IVI warning contract | Done | `schemas/r4_warning.schema.json`, `testdata/r4_warning.sample.json` |
| R3 track store | Done | `include/ada/track_store.hpp`, `src/track_store.cpp` |
| R13 admission gate | Done | `TrackStore::apply_v2x_relayed`, `config/ada-ecu.conf` |
| Externalized gate constants | Done | `gate_enter_m`, `gate_exit_m`, `miss_limit_ms`, `tentative_hits` in `config/ada-ecu.conf` |
| R3 own-sensor JSONL detector seam | Done | `detector_jsonl_ingest.*`, `r3_mapper.*` |
| R2 UDP receiver seam | Done | `udp_r2_receiver.*`, `v2x_r2_ingest.*` |
| CRA / NLOS risk scaffold | Done | `risk_assessor.*` |
| R4 warning smoke output | Done | `warning_builder.*`, `ada_ecu --mock` |
| R4 ADA → IVI UDP sender | Done | `udp_r4_sender.*`, `tools/mock_ivi_receiver.py` |
| JSONL evidence logs | Done | `EventLogger`, `track_transition`, `own_sensor_rx`, `r2_rx`, `risk_event`, `r4_tx` |
| Linux/ARM container build path | Ready, not locally verified | `Dockerfile`, README command; Docker daemon unavailable during this check |

## Verification Commands

Local CMake:

```sh
cmake -S ADA_ECU -B ADA_ECU/build
cmake --build ADA_ECU/build
ctest --test-dir ADA_ECU/build --output-on-failure
```

Phase 2 mock smoke:

```sh
ADA_ECU/build/ada_ecu --config ADA_ECU/config/ada-ecu.conf --mock
```

Expected smoke output: one R4 `warning` JSON with `warningType = "nlos_obstruction"` and triggering object `source = "v2x_relayed"`.

ADA → IVI R4 UDP smoke:

```sh
python3 ADA_ECU/tools/mock_ivi_receiver.py --host 127.0.0.1 --port 46004
ADA_ECU/build/ada_ecu --config ADA_ECU/config/ada-ecu.conf --mock
```

Expected IVI receiver output: one R4 warning with `geometry.vehicleB`, `geometry.vehicleC`, and debug `trackedObjects` containing `own:B` and `v2x:1201:7`.

External V2X mock sender smoke:

```sh
ADA_ECU/build/ada_ecu --config ADA_ECU/config/ada-ecu.conf --listen-once
python3 ADA_ECU/tools/mock_v2x_sender.py --host 127.0.0.1 --port 46002
```

Linux/ARM image build:

```sh
docker buildx build --platform linux/arm64 -t ADA_ECU:phase2 ADA_ECU
```

## Phase 2 Acceptance Mapping

| Milestone acceptance item | Status | Evidence |
|---|---:|---|
| Store exposes all R3 fields | Done | `types.hpp`, `r3_tracked_object.schema.json`, `track_store_tests.cpp` |
| Detector-shaped and relayed-shaped entries enter through identical R3 store interface | Done | `detector_jsonl_ingest.cpp`, `v2x_r2_ingest.cpp`, `TrackStore::upsert` |
| Mock-driven state transitions observable in logs and match R13 lifecycle | Done | `EventLogger`, `track_transition` events, `ada_ecu --mock` |
| ADA sends R4 warning to IVI over UDP | Done | `UdpR4Sender`, `mock_ivi_receiver.py` |
| Toggling mock off yields no tracks | Partial | Default run exits without ingesting; continuous runtime loop is not implemented yet |
| Mock C admitted only within gate and dropped beyond exit or timeout | Done | Enter, hysteresis, exit, and timeout expiry covered in `track_store_tests.cpp` |
| Gate constants read from configuration, no literals in logic | Done | `config/ada-ecu.conf`, `AdaConfig` |
| CRA database schema committed | Deferred | Current implementation uses in-memory `TrackStore`; no persistent database selected for Phase 2 scaffold |
| Video-input proposal sent to FPT-Mentor | Not code | Requires team/user action outside code |
| Build + CI round-trip tests green on frozen contracts | Partial | CMake/CTest local pass; CI not configured yet |
| Linux/ARM container build pass | Pass | Buildx `linux/arm64` image loaded successfully; 14/14 CTest pass in build stage |

## Not Phase 3 Yet

- No YOLO11n ONNX Runtime inference.
- `tools/video_detector.py --backend placeholder` and `testdata/r3_own_sensor.jsonl` are detector seam fixtures only.

## Not Full Phase 4 Yet

- CRA is a scaffold with one NLOS distance-risk assessor.
- R4 emission sends UDP to IVI and also prints to stdout/log for debugging.
- Multi-packet risk transition behavior is covered in unit tests and documented in `phase4_fusion_warning.md`.
- Periodic awareness state is not implemented.
