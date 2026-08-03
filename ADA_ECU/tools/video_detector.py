#!/usr/bin/env python3
"""Emit Phase 3 R3 own-sensor vehicle detections as JSONL."""

from __future__ import annotations

import argparse
import os
import pathlib
import sys
import time

ADA_ROOT = pathlib.Path(__file__).resolve().parents[1]
if str(ADA_ROOT) not in sys.path:
    sys.path.insert(0, str(ADA_ROOT))

# Imports remain public for compatibility with existing detector tests/tools.
from detector.backends import (  # noqa: E402
    PlaceholderVehicleBackend,
    YoloOnnxVehicleBackend,
)
from detector.emission import emit_jsonl, r3_vehicle_b  # noqa: E402
from detector.frame_source import synthetic_samples, video_samples  # noqa: E402
from detector.models import Detection, DetectionBackend, FrameInput  # noqa: E402

__all__ = [
    "Detection",
    "DetectionBackend",
    "FrameInput",
    "PlaceholderVehicleBackend",
    "YoloOnnxVehicleBackend",
    "emit_jsonl",
    "main",
    "make_backend",
    "parse_args",
    "r3_vehicle_b",
    "synthetic_samples",
    "video_samples",
]


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


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Emit R3 own-sensor detections from a video file as JSONL."
    )
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--video", help="input video file decoded by OpenCV")
    mode.add_argument(
        "--synthetic",
        type=int,
        metavar="COUNT",
        help="emit COUNT synthetic frame detections without OpenCV",
    )
    parser.add_argument(
        "--backend",
        choices=["placeholder", "yolo-onnx"],
        default="placeholder",
        help="detection backend to run",
    )
    parser.add_argument(
        "--model",
        default=os.getenv("MODEL_PATH", "ADA_ECU/models/yolo11n.onnx"),
        help="YOLO11 ONNX model path",
    )
    parser.add_argument(
        "--confidence",
        type=float,
        default=float(os.getenv("CONF_THRESHOLD", "0.25")),
        help="YOLO confidence threshold",
    )
    parser.add_argument(
        "--iou",
        type=float,
        default=float(os.getenv("IOU_THRESHOLD", "0.45")),
        help="YOLO NMS IoU threshold",
    )
    parser.add_argument(
        "--input-size",
        type=int,
        default=int(os.getenv("MODEL_INPUT_SIZE", "640")),
        help="YOLO square input size",
    )
    parser.add_argument(
        "--vehicle-width-m",
        type=float,
        default=float(os.getenv("VEHICLE_WIDTH_M", "1.8")),
        help="nominal vehicle width for distance estimate",
    )
    parser.add_argument(
        "--focal-px",
        type=float,
        default=float(os.getenv("CAMERA_FOCAL_PX", "2000")),
        help="camera focal length in pixels for distance/lateral estimate",
    )
    parser.add_argument(
        "--log-detections",
        action="store_true",
        help="write ML detection bbox evidence to stderr",
    )
    parser.add_argument(
        "--every-n-frames",
        type=int,
        default=5,
        help="sample one frame every N frames in video mode",
    )
    parser.add_argument("--limit", type=int, help="maximum detections to emit")
    parser.add_argument(
        "--realtime",
        action="store_true",
        help="pace sampled frames using clip timestamps",
    )
    parser.add_argument(
        "--loop", action="store_true", help="restart the clip after its final frame"
    )
    return parser.parse_args()


def validate_args(args: argparse.Namespace) -> str | None:
    if args.every_n_frames <= 0:
        return "--every-n-frames must be > 0"
    if args.confidence <= 0 or args.confidence > 1:
        return "--confidence must be in (0, 1]"
    if args.iou <= 0 or args.iou > 1:
        return "--iou must be in (0, 1]"
    if args.vehicle_width_m <= 0 or args.focal_px <= 0:
        return "--vehicle-width-m and --focal-px must be > 0"
    return None


def main() -> int:
    args = parse_args()
    error = validate_args(args)
    if error:
        print(error, file=sys.stderr)
        return 2
    try:
        backend = make_backend(args)
        if args.synthetic is not None:
            count = (
                min(args.synthetic, args.limit)
                if args.limit is not None
                else args.synthetic
            )
            samples = synthetic_samples(count, int(time.time() * 1000))
        else:
            samples = video_samples(
                args.video,
                args.every_n_frames,
                args.limit,
                args.realtime,
                args.loop,
            )
        emitted = emit_jsonl(samples, backend)
    except RuntimeError as exc:
        print(str(exc), file=sys.stderr)
        return 1
    if emitted == 0:
        print("no detections emitted", file=sys.stderr)
        return 3
    return 0


if __name__ == "__main__":
    sys.exit(main())
