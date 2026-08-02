# Phase 3 Video Detector Skeleton

Phase 3 replaces the Phase 2 mock own-sensor input with a Python detector subprocess. The subprocess emits R3 `TrackedObject` JSONL on stdout, and the ADA C++ core consumes the same JSONL seam through `detector_jsonl_ingest`.

## Current Scope

- OpenCV video decode path is scaffolded in `tools/video_detector.py`.
- YOLO11n ONNX inference is not wired yet.
- The current `placeholder` backend emits deterministic `own:B` detections per sampled frame so the subprocess contract is testable before model integration.
- The output contract is R3 JSONL with `source = "own_sensor"`.

## Install

```sh
python3 -m pip install -r ADA_ECU/requirements.txt
```

## Smoke Without OpenCV

```sh
python3 ADA_ECU/tools/video_detector.py --synthetic 2
```

## Run On A Video

```sh
python3 ADA_ECU/tools/video_detector.py --video path/to/ego_video.mp4 --backend placeholder --every-n-frames 5 --limit 20 > /tmp/r3_own_sensor.jsonl
ADA_ECU/build/ada_ecu --config ADA_ECU/config/ada-ecu.conf --mock --own-sensor-sample /tmp/r3_own_sensor.jsonl
```

## Smoke Video Path

Generate a tiny synthetic video and validate the detector JSONL contract:

```sh
python3 ADA_ECU/tools/smoke_video_detector.py
```

## Next Step

Add a YOLO11n ONNX backend behind the `DetectionBackend` seam and keep the stdout R3 JSONL shape unchanged.
