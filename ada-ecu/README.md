# ADA ECU

Phase 2/3/4 scaffold for the ADA ECU track:

- Phase 2: R3 `TrackedObject` store, R13 admission gate, config, JSONL evidence logs, mock inputs.
- Phase 3 seam: Python detector subprocess emits R3 JSONL with `source = "own_sensor"`.
- Phase 4 seam: R2 V2X object input becomes `source = "v2x_relayed"` tracks, CRA emits R4 warnings for IVI.

The core is C++17 and uses no middleware. JSON parsing in this scaffold is intentionally narrow and contract-shaped; replace it with `nlohmann/json` once dependency packaging is finalized.

## Contracts

- `schemas/r2_v2x_object.schema.json`: V2X ECU to ADA object update.
- `schemas/r3_tracked_object.schema.json`: shared ADA track object used by own-sensor and relayed sources.
- `schemas/r4_warning.schema.json`: ADA to IVI warning event.
- `testdata/*.sample.json`: golden contract examples for mapper/store/warning tests.

Phase 2 acceptance tracking lives in `docs/phase2_acceptance.md`.
Phase 3 detector notes live in `docs/phase3_video_detector.md`.
Phase 4 fusion/warning notes live in `docs/phase4_fusion_warning.md`.

## Build

Install local build dependencies:

```sh
brew install cmake nlohmann-json
```

Install Python detector dependencies:

```sh
python3 -m pip install -r ada-ecu/requirements.txt
```

```sh
cmake -S ada-ecu -B ada-ecu/build
cmake --build ada-ecu/build
ctest --test-dir ada-ecu/build --output-on-failure
```

## Linux/ARM Container Build

ADA runs as a Linux OCI Container Node on CarSky. Keep the code portable C++17 and verify the image for the simulator architecture:

```sh
docker buildx build --platform linux/arm64 -t ada-ecu:phase2 ada-ecu
```

For local x86_64 smoke checks:

```sh
docker build -t ada-ecu:phase2 ada-ecu
docker run --rm ada-ecu:phase2 --config /app/config/ada-ecu.conf --mock
```

## Run Mock

```sh
ada-ecu/build/ada_ecu --config ada-ecu/config/ada-ecu.conf --mock
```

`--mock` now exercises both ADA inputs: own-sensor B enters through the R3 JSONL detector seam, then relayed C enters through the UDP R2 receiver over loopback.

To test with an external mock V2X sender:

```sh
ada-ecu/build/ada_ecu --config ada-ecu/config/ada-ecu.conf --listen-once
python3 ada-ecu/tools/mock_v2x_sender.py --host 127.0.0.1 --port 46002
```

Use `--own-sensor-sample <jsonl>` to replace the detector seam sample.

To test ADA → IVI R4 UDP, terminal 1:

```sh
python3 ada-ecu/tools/mock_ivi_receiver.py --host 127.0.0.1 --port 46004
```

Terminal 2:

```sh
ada-ecu/build/ada_ecu --config ada-ecu/config/ada-ecu.conf --mock
```

The IVI receiver should print one R4 warning with `trackedObjects` containing `own:B` and `v2x:1201:7`.

Generate a Phase 3 detector JSONL sample:

```sh
python3 ada-ecu/tools/video_detector.py --synthetic 2 > /tmp/r3_own_sensor.jsonl
```

Smoke-test the OpenCV video path:

```sh
python3 ada-ecu/tools/smoke_video_detector.py
```
