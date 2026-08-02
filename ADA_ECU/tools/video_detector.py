#!/usr/bin/env python3
"""Phase 3 video detector.

This process is the ADA detector subprocess seam. It reads video frames with
OpenCV and emits R3 TrackedObject JSONL on stdout. `placeholder` remains for
deterministic CI/smoke tests; `yolo-onnx` runs a YOLOv8/YOLO11-style ONNX model
with ONNX Runtime and emits vehicle detections from real video frames.
"""

from __future__ import annotations

import argparse
import json
import os
import sys
import time
from dataclasses import dataclass
from typing import Iterable, Iterator, Protocol


@dataclass(frozen=True)
class FrameInput:
    frame_index: int
    timestamp_ms: int
    width: int
    height: int
    image: object | None = None


@dataclass(frozen=True)
class Detection:
    class_id: int
    class_name: str
    score: float
    x1: float
    y1: float
    x2: float
    y2: float

    @property
    def width(self) -> float:
        return max(0.0, self.x2 - self.x1)

    @property
    def height(self) -> float:
        return max(0.0, self.y2 - self.y1)

    @property
    def area(self) -> float:
        return self.width * self.height

    @property
    def center_x(self) -> float:
        return (self.x1 + self.x2) / 2.0


class DetectionBackend(Protocol):
    def detect(self, frame: FrameInput) -> Iterable[dict]:
        """Return R3-compatible own-sensor detections for one frame."""


class PlaceholderVehicleBackend:
    """Deterministic B detection used by CI and smoke tests."""

    def detect(self, frame: FrameInput) -> Iterable[dict]:
        longitudinal_m = 12.0 + frame.frame_index * 0.05
        yield r3_vehicle_b(
            distance_m=longitudinal_m,
            lateral_m=0.0,
            confidence=0.5,
            timestamp_ms=frame.timestamp_ms,
        )


class YoloOnnxVehicleBackend:
    """YOLOv8/YOLO11 ONNX vehicle detector that emits the best vehicle as B."""

    vehicle_class_ids = {2, 3, 5, 7}  # COCO: car, motorcycle, bus, truck
    coco_names = {
        2: "car",
        3: "motorcycle",
        5: "bus",
        7: "truck",
    }

    def __init__(
        self,
        model_path: str,
        confidence_threshold: float,
        iou_threshold: float,
        input_size: int,
        vehicle_width_m: float,
        focal_px: float,
        log_detections: bool,
    ) -> None:
        if not model_path:
            raise RuntimeError("--model is required when --backend yolo-onnx")
        if not os.path.exists(model_path):
            raise RuntimeError(f"YOLO ONNX model not found: {model_path}")

        try:
            import numpy as np  # type: ignore
            import onnxruntime as ort  # type: ignore
        except ModuleNotFoundError as exc:
            raise RuntimeError("YOLO backend dependencies are missing. Install with: python3 -m pip install -r ADA_ECU/requirements.txt") from exc

        self.np = np
        self.session = ort.InferenceSession(model_path, providers=["CPUExecutionProvider"])
        self.input_name = self.session.get_inputs()[0].name
        shape = self.session.get_inputs()[0].shape
        if len(shape) == 4 and isinstance(shape[2], int) and isinstance(shape[3], int):
            self.input_size = int(shape[2])
        else:
            self.input_size = input_size
        self.confidence_threshold = confidence_threshold
        self.iou_threshold = iou_threshold
        self.vehicle_width_m = vehicle_width_m
        self.focal_px = focal_px
        self.log_detections = log_detections

    def detect(self, frame: FrameInput) -> Iterable[dict]:
        if frame.image is None:
            return []

        tensor, scale, pad_x, pad_y = self._preprocess(frame.image)
        outputs = self.session.run(None, {self.input_name: tensor})
        detections = self._postprocess(outputs[0], frame.width, frame.height, scale, pad_x, pad_y)
        if not detections:
            return []

        # Vehicle B is the visible occluder in ego view. For the demo clip and
        # near-lane driving scenes, the largest vehicle bbox is a deterministic
        # proxy for the occluding vehicle.
        vehicle_b = max(detections, key=lambda item: (item.area, item.score))
        distance_m = self._distance_m(vehicle_b.width)
        lateral_m = self._lateral_m(vehicle_b.center_x, frame.width, distance_m)
        confidence = round(vehicle_b.score, 3)

        if self.log_detections:
            print(
                json.dumps(
                    {
                        "event": "ml_detection",
                        "frame": frame.frame_index,
                        "timestampMs": frame.timestamp_ms,
                        "class": vehicle_b.class_name,
                        "confidence": confidence,
                        "bbox": [
                            round(vehicle_b.x1, 1),
                            round(vehicle_b.y1, 1),
                            round(vehicle_b.x2, 1),
                            round(vehicle_b.y2, 1),
                        ],
                        "distance": round(distance_m, 3),
                    },
                    separators=(",", ":"),
                ),
                file=sys.stderr,
                flush=True,
            )

        return [
            r3_vehicle_b(
                distance_m=distance_m,
                lateral_m=lateral_m,
                confidence=confidence,
                timestamp_ms=frame.timestamp_ms,
            )
        ]

    def _preprocess(self, image: object) -> tuple[object, float, float, float]:
        import cv2  # type: ignore

        np = self.np
        original_h, original_w = image.shape[:2]
        scale = min(self.input_size / original_w, self.input_size / original_h)
        resized_w = int(round(original_w * scale))
        resized_h = int(round(original_h * scale))
        resized = cv2.resize(image, (resized_w, resized_h), interpolation=cv2.INTER_LINEAR)

        pad_x = (self.input_size - resized_w) / 2.0
        pad_y = (self.input_size - resized_h) / 2.0
        left = int(round(pad_x))
        top = int(round(pad_y))
        canvas = np.full((self.input_size, self.input_size, 3), 114, dtype=np.uint8)
        canvas[top : top + resized_h, left : left + resized_w] = resized

        rgb = cv2.cvtColor(canvas, cv2.COLOR_BGR2RGB)
        tensor = rgb.astype(np.float32) / 255.0
        tensor = np.transpose(tensor, (2, 0, 1))[None, ...]
        return tensor, scale, float(left), float(top)

    def _postprocess(self, output: object, width: int, height: int, scale: float, pad_x: float, pad_y: float) -> list[Detection]:
        np = self.np
        preds = np.squeeze(output)
        if preds.ndim != 2:
            return []
        # YOLOv8/YOLO11 exports commonly produce (84, N); transpose to (N, 84).
        if preds.shape[0] < preds.shape[1] and preds.shape[0] <= 256:
            preds = preds.T

        candidates: list[Detection] = []
        for row in preds:
            if row.shape[0] < 6:
                continue
            class_scores = row[4:]
            class_id = int(np.argmax(class_scores))
            score = float(class_scores[class_id])
            if class_id not in self.vehicle_class_ids or score < self.confidence_threshold:
                continue

            cx, cy, box_w, box_h = [float(value) for value in row[:4]]
            x1 = (cx - box_w / 2.0 - pad_x) / scale
            y1 = (cy - box_h / 2.0 - pad_y) / scale
            x2 = (cx + box_w / 2.0 - pad_x) / scale
            y2 = (cy + box_h / 2.0 - pad_y) / scale
            x1 = min(max(x1, 0.0), float(width))
            y1 = min(max(y1, 0.0), float(height))
            x2 = min(max(x2, 0.0), float(width))
            y2 = min(max(y2, 0.0), float(height))
            if x2 <= x1 or y2 <= y1:
                continue
            candidates.append(Detection(class_id, self.coco_names[class_id], score, x1, y1, x2, y2))

        return self._nms(candidates)

    def _nms(self, detections: list[Detection]) -> list[Detection]:
        selected: list[Detection] = []
        remaining = sorted(detections, key=lambda item: item.score, reverse=True)
        while remaining:
            current = remaining.pop(0)
            selected.append(current)
            remaining = [item for item in remaining if self._iou(current, item) < self.iou_threshold]
        return selected

    @staticmethod
    def _iou(a: Detection, b: Detection) -> float:
        inter_x1 = max(a.x1, b.x1)
        inter_y1 = max(a.y1, b.y1)
        inter_x2 = min(a.x2, b.x2)
        inter_y2 = min(a.y2, b.y2)
        inter_w = max(0.0, inter_x2 - inter_x1)
        inter_h = max(0.0, inter_y2 - inter_y1)
        inter = inter_w * inter_h
        union = a.area + b.area - inter
        return inter / union if union > 0 else 0.0

    def _distance_m(self, bbox_width_px: float) -> float:
        if bbox_width_px <= 1.0:
            return 0.0
        return max(0.1, (self.vehicle_width_m * self.focal_px) / bbox_width_px)

    def _lateral_m(self, center_x: float, frame_width: int, distance_m: float) -> float:
        return ((center_x - frame_width / 2.0) / self.focal_px) * distance_m


def r3_vehicle_b(distance_m: float, lateral_m: float, confidence: float, timestamp_ms: int) -> dict:
    return {
        "id": "own:B",
        "class": "vehicle",
        "source": "own_sensor",
        "position": {
            "x": round(distance_m, 3),
            "y": round(lateral_m, 3),
            "confidence": confidence,
        },
        "distance": round(distance_m, 3),
        "speed": 0.0,
        "confidence": confidence,
        "state": "tentative",
        "timestamps": {
            "measured": timestamp_ms,
            "received": timestamp_ms,
            "lastUpdated": timestamp_ms,
        },
    }


def make_backend(args: argparse.Namespace) -> DetectionBackend:
    if args.backend == "placeholder":
        return PlaceholderVehicleBackend()
    if args.backend == "yolo-onnx":
        return YoloOnnxVehicleBackend(
            model_path=args.model,
            confidence_threshold=args.confidence,
            iou_threshold=args.iou,
            input_size=args.input_size,
            vehicle_width_m=args.vehicle_width_m,
            focal_px=args.focal_px,
            log_detections=args.log_detections,
        )
    raise RuntimeError(f"unsupported detector backend: {args.backend}")


def synthetic_samples(count: int, start_ms: int) -> Iterator[FrameInput]:
    for frame_index in range(count):
        yield FrameInput(
            frame_index=frame_index,
            timestamp_ms=start_ms + frame_index * 100,
            width=1280,
            height=720,
        )


def video_samples(video_path: str, every_n_frames: int, limit: int | None) -> Iterator[FrameInput]:
    try:
        import cv2  # type: ignore
    except ModuleNotFoundError as exc:
        raise RuntimeError("OpenCV is not installed. Install with: python3 -m pip install -r ADA_ECU/requirements.txt") from exc

    cap = cv2.VideoCapture(video_path)
    if not cap.isOpened():
        raise RuntimeError(f"cannot open video: {video_path}")

    fps = cap.get(cv2.CAP_PROP_FPS)
    if not fps or fps <= 0:
        fps = 10.0

    emitted = 0
    frame_index = 0
    try:
        while True:
            ok, frame = cap.read()
            if not ok:
                break
            if frame_index % every_n_frames == 0:
                height, width = frame.shape[:2]
                yield FrameInput(
                    frame_index=frame_index,
                    timestamp_ms=int(frame_index * 1000 / fps),
                    width=int(width),
                    height=int(height),
                    image=frame,
                )
                emitted += 1
                if limit is not None and emitted >= limit:
                    break
            frame_index += 1
    finally:
        cap.release()


def emit_jsonl(samples: Iterable[FrameInput], backend: DetectionBackend) -> int:
    count = 0
    for sample in samples:
        for detection in backend.detect(sample):
            print(json.dumps(detection, separators=(",", ":")), flush=True)
            count += 1
    return count


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Emit R3 own-sensor detections from a video file as JSONL.")
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--video", help="input video file decoded by OpenCV")
    mode.add_argument("--synthetic", type=int, metavar="COUNT", help="emit COUNT synthetic frame detections without OpenCV")
    parser.add_argument("--backend", choices=["placeholder", "yolo-onnx"], default="placeholder", help="detection backend to run")
    parser.add_argument("--model", default=os.getenv("MODEL_PATH", "ADA_ECU/models/yolov8n.onnx"), help="YOLO ONNX model path")
    parser.add_argument("--confidence", type=float, default=float(os.getenv("CONF_THRESHOLD", "0.25")), help="YOLO confidence threshold")
    parser.add_argument("--iou", type=float, default=float(os.getenv("IOU_THRESHOLD", "0.45")), help="YOLO NMS IoU threshold")
    parser.add_argument("--input-size", type=int, default=int(os.getenv("MODEL_INPUT_SIZE", "640")), help="YOLO square input size")
    parser.add_argument("--vehicle-width-m", type=float, default=float(os.getenv("VEHICLE_WIDTH_M", "1.8")), help="nominal vehicle width for distance estimate")
    parser.add_argument("--focal-px", type=float, default=float(os.getenv("CAMERA_FOCAL_PX", "900")), help="camera focal length in pixels for distance/lateral estimate")
    parser.add_argument("--log-detections", action="store_true", help="write ML detection bbox evidence to stderr")
    parser.add_argument("--every-n-frames", type=int, default=5, help="sample one frame every N frames in video mode")
    parser.add_argument("--limit", type=int, help="maximum detections to emit")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    if args.every_n_frames <= 0:
        print("--every-n-frames must be > 0", file=sys.stderr)
        return 2
    if args.confidence <= 0 or args.confidence > 1:
        print("--confidence must be in (0, 1]", file=sys.stderr)
        return 2
    if args.iou <= 0 or args.iou > 1:
        print("--iou must be in (0, 1]", file=sys.stderr)
        return 2
    if args.vehicle_width_m <= 0 or args.focal_px <= 0:
        print("--vehicle-width-m and --focal-px must be > 0", file=sys.stderr)
        return 2

    try:
        backend = make_backend(args)
        if args.synthetic is not None:
            count = min(args.synthetic, args.limit) if args.limit is not None else args.synthetic
            emitted = emit_jsonl(synthetic_samples(count, int(time.time() * 1000)), backend)
        else:
            emitted = emit_jsonl(video_samples(args.video, args.every_n_frames, args.limit), backend)
    except RuntimeError as exc:
        print(str(exc), file=sys.stderr)
        return 1

    if emitted == 0:
        print("no detections emitted", file=sys.stderr)
        return 3
    return 0


if __name__ == "__main__":
    sys.exit(main())
