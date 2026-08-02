# Phase 3 Video Detector

Phase 3 replaces the Phase 2 mock own-sensor input with a Python detector subprocess. The subprocess emits R3 `TrackedObject` JSONL on stdout, and the ADA C++ core consumes the same JSONL seam through `detector_jsonl_ingest`.

## Current Scope

- OpenCV video decode path is implemented in `tools/video_detector.py`.
- `placeholder` backend remains for deterministic CI and smoke tests.
- `yolo-onnx` backend runs real YOLO ONNX inference with ONNX Runtime CPU.
- The detector filters COCO vehicle classes: `car`, `motorcycle`, `bus`, `truck`.
- Vehicle B selection rejects implausible full-frame boxes, then scores vehicle area weighted by ego-lane center proximity.
- Distance is estimated from bounding-box width with `VEHICLE_WIDTH_M` and `CAMERA_FOCAL_PX`.
- Output contract is R3 JSONL with `source = "own_sensor"` and id `own:B`.

## Install

```sh
python3 -m venv ADA_ECU/.venv
source ADA_ECU/.venv/bin/activate
python -m pip install -r ADA_ECU/requirements.txt
```

## Smoke Without OpenCV

```sh
python ADA_ECU/tools/video_detector.py --synthetic 2
```

## Placeholder Video Smoke

```sh
python ADA_ECU/tools/smoke_demo_video_detector.py
```

Expected result:

```text
phase3 demo video detector: pass (5 R3 objects)
```

## ML Detector Setup

Download the pretrained YOLO11n ONNX model:

```sh
python ADA_ECU/tools/download_yolo_model.py
```

The model is saved to:

```sh
ADA_ECU/models/yolo11n.onnx
```

The pretrained `yolo11n.onnx` artifact is committed so CI and CarSky image builds are offline-reproducible.

## ML Detector Run

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

`--log-detections` writes bounding-box evidence to stderr, while stdout stays valid R3 JSONL for ADA ingestion.

## ML Smoke Test

```sh
python ADA_ECU/tools/smoke_ml_video_detector.py
```

Observed local result on `ADA_ECU/media/ego-b-occluding-c.mp4`:

```text
phase3 ML video detector: pass (5 R3 objects)
```

## Full-clip R12 Evidence

The acceptance runner processes all 200 frames at a stride of four, writes 50
sampled-frame R3 objects to `/tmp/ada_phase3_yolo11n.jsonl`, and reports
coverage, effective inference rate, warm-up, distance range/trend, and 30 m
gate crossings:

```sh
python ADA_ECU/tools/benchmark_video_detector.py
python ADA_ECU/tools/check_zero_c.py /tmp/ada_phase3_yolo11n.jsonl
```

The calibrated defaults are `VEHICLE_WIDTH_M=1.8` and
`CAMERA_FOCAL_PX=2000`. They affect only monocular distance estimation; the
R13 admission gates remain unchanged.

Example ML evidence:

```json
{"event":"ml_detection","frame":80,"timestampMs":4000,"class":"bus","confidence":0.885,"bbox":[703.1,325.7,849.0,454.5],"distance":11.104}
```

Example emitted R3:

```json
{"id":"own:B","class":"vehicle","source":"own_sensor","position":{"x":11.104,"y":1.679,"confidence":0.885},"distance":11.104,"speed":0.0,"confidence":0.885,"state":"tentative","timestamps":{"measured":4000,"received":4000,"lastUpdated":4000}}
```

## Feed ML R3 Into ADA

```sh
ADA_ECU/build-runtime/ada_ecu \
  --config ADA_ECU/config/ada-ecu.conf \
  --mock \
  --own-sensor-sample /tmp/ada_r3_ml_from_project_video.jsonl
```

Expected R4 evidence:

- `trackedObjects` contains ML-derived `own:B`.
- `trackedObjects` contains V2X-relayed `v2x:1201:7`.
- `geometry.vehicleB` is populated from ML-derived B.
- `geometry.vehicleC` is composed from B + V2X C.

## Status

- Phase 3 video decode, ML inference, R3 emission, and Phase 4 ingestion are verified locally.
- Distance remains a monocular estimate; its constants are calibrated for the committed M1 clip.
