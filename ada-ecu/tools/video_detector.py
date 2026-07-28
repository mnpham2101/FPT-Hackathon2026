#!/usr/bin/env python3
"""Phase 3 video detector skeleton.

This process is the ADA detector subprocess seam. It reads video frames with
OpenCV and emits R3 TrackedObject JSONL on stdout. The current implementation
does not run YOLO yet; it emits a deterministic placeholder B detection per
sampled frame so the ADA C++ core can consume the real subprocess contract.
"""

from __future__ import annotations

import argparse
import json
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


class DetectionBackend(Protocol):
    def detect(self, frame: FrameInput) -> Iterable[dict]:
        """Return R3-compatible own-sensor detections for one frame."""


class PlaceholderVehicleBackend:
    """Deterministic B detection used until YOLO11n ONNX is wired."""

    def detect(self, frame: FrameInput) -> Iterable[dict]:
        longitudinal_m = 12.0 + frame.frame_index * 0.05
        yield {
            "id": "own:B",
            "class": "vehicle",
            "source": "own_sensor",
            "position": {
                "x": round(longitudinal_m, 3),
                "y": 0.0,
                "confidence": 0.5,
            },
            "distance": round(longitudinal_m, 3),
            "speed": 0.0,
            "confidence": 0.5,
            "state": "tentative",
            "timestamps": {
                "measured": frame.timestamp_ms,
                "received": frame.timestamp_ms,
                "lastUpdated": frame.timestamp_ms,
            },
        }


def make_backend(name: str) -> DetectionBackend:
    if name == "placeholder":
        return PlaceholderVehicleBackend()
    raise RuntimeError(f"unsupported detector backend: {name}")


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
        raise RuntimeError("OpenCV is not installed. Install with: python3 -m pip install -r ada-ecu/requirements.txt") from exc

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
    parser.add_argument("--backend", choices=["placeholder"], default="placeholder", help="detection backend to run")
    parser.add_argument("--every-n-frames", type=int, default=5, help="sample one frame every N frames in video mode")
    parser.add_argument("--limit", type=int, help="maximum detections to emit")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    if args.every_n_frames <= 0:
        print("--every-n-frames must be > 0", file=sys.stderr)
        return 2

    try:
        backend = make_backend(args.backend)
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
