# Phase 3 Video Detector Skeleton

Phase 3 replaces the Phase 2 mock own-sensor input with a Python detector subprocess. The subprocess emits R3 `TrackedObject` JSONL on stdout, and the ADA C++ core consumes the same JSONL seam through `detector_jsonl_ingest`.

## Current Scope

- OpenCV video decode path is scaffolded in `tools/video_detector.py`.
- YOLO11n ONNX inference is not wired yet.
- The current detector emits deterministic placeholder `own:B` detections per sampled frame so the subprocess contract is testable before model integration.
- The output contract is R3 JSONL with `source = "own_sensor"`.

## Install

```sh
python3 -m pip install -r ada-ecu/requirements.txt
```

## Smoke Without OpenCV

```sh
python3 ada-ecu/tools/video_detector.py --synthetic 2
```

## Run On A Video

```sh
python3 ada-ecu/tools/video_detector.py --video path/to/ego_video.mp4 --every-n-frames 5 --limit 20 > /tmp/r3_own_sensor.jsonl
ada-ecu/build/ada_ecu --config ada-ecu/config/ada-ecu.conf --mock --own-sensor-sample /tmp/r3_own_sensor.jsonl
```

## Next Step

Replace `make_r3_own_sensor_b` with YOLO11n ONNX Runtime inference and distance estimation while keeping the stdout R3 JSONL shape unchanged.
