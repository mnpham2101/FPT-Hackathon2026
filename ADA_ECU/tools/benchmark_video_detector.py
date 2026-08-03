#!/usr/bin/env python3
"""Run the full Phase 3 clip and report measurable R12 acceptance KPIs."""

from __future__ import annotations

import argparse
import json
import math
import pathlib
import subprocess
import sys
import time
from itertools import pairwise


def video_frame_count(video_path: pathlib.Path) -> int:
    try:
        import cv2  # type: ignore
    except ModuleNotFoundError as exc:
        raise RuntimeError(
            "OpenCV is required; install ADA_ECU/requirements.txt"
        ) from exc
    capture = cv2.VideoCapture(str(video_path))
    if not capture.isOpened():
        raise RuntimeError(f"cannot open video: {video_path}")
    try:
        return int(capture.get(cv2.CAP_PROP_FRAME_COUNT))
    finally:
        capture.release()


def main() -> int:
    repo_root = pathlib.Path(__file__).resolve().parents[2]
    parser = argparse.ArgumentParser(
        description="Measure full-clip YOLO11n R12 acceptance KPIs."
    )
    parser.add_argument(
        "--video",
        type=pathlib.Path,
        default=repo_root / "ADA_ECU/media/ego-b-occluding-c.mp4",
    )
    parser.add_argument(
        "--model", type=pathlib.Path, default=repo_root / "ADA_ECU/models/yolo11n.onnx"
    )
    parser.add_argument(
        "--output",
        type=pathlib.Path,
        default=pathlib.Path("/tmp/ada_phase3_yolo11n.jsonl"),
    )
    parser.add_argument("--every-n-frames", type=int, default=4)
    parser.add_argument("--confidence", type=float, default=0.20)
    parser.add_argument("--focal-px", type=float, default=2000.0)
    parser.add_argument("--gate-m", type=float, default=30.0)
    args = parser.parse_args()

    if args.every_n_frames <= 0:
        print("--every-n-frames must be > 0", file=sys.stderr)
        return 2

    detector = pathlib.Path(__file__).resolve().with_name("video_detector.py")
    total_frames = video_frame_count(args.video)
    sampled_frames = math.ceil(total_frames / args.every_n_frames)
    command = [
        sys.executable,
        str(detector),
        "--video",
        str(args.video),
        "--backend",
        "yolo-onnx",
        "--model",
        str(args.model),
        "--every-n-frames",
        str(args.every_n_frames),
        "--confidence",
        str(args.confidence),
        "--focal-px",
        str(args.focal_px),
    ]

    started = time.perf_counter()
    process = subprocess.Popen(
        command, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, bufsize=1
    )
    assert process.stdout is not None
    rows: list[dict] = []
    first_detection_s: float | None = None
    with args.output.open("w", encoding="utf-8") as evidence:
        for line in process.stdout:
            if first_detection_s is None:
                first_detection_s = time.perf_counter() - started
            evidence.write(line)
            rows.append(json.loads(line))
    stderr = process.stderr.read() if process.stderr is not None else ""
    return_code = process.wait()
    elapsed_s = time.perf_counter() - started
    if return_code != 0:
        print(stderr, file=sys.stderr, end="")
        return return_code

    distances = [float(row["distance"]) for row in rows]
    non_increasing_steps = sum(
        current <= previous for previous, current in pairwise(distances)
    )
    transitions = max(len(distances) - 1, 0)
    gate_crossings = sum(
        (previous >= args.gate_m) != (current >= args.gate_m)
        for previous, current in pairwise(distances)
    )
    result = {
        "model": args.model.name,
        "focalPx": args.focal_px,
        "totalFrames": total_frames,
        "sampledFrames": sampled_frames,
        "detections": len(rows),
        "coverage": round(len(rows) / sampled_frames, 4) if sampled_frames else 0.0,
        "elapsedSeconds": round(elapsed_s, 4),
        "effectiveHz": round(sampled_frames / elapsed_s, 3) if elapsed_s else 0.0,
        "warmupSeconds": round(first_detection_s, 4)
        if first_detection_s is not None
        else None,
        "distanceFirstM": distances[0] if distances else None,
        "distanceLastM": distances[-1] if distances else None,
        "distanceMinM": min(distances) if distances else None,
        "distanceMaxM": max(distances) if distances else None,
        "nonIncreasingStepRatio": round(non_increasing_steps / transitions, 4)
        if transitions
        else None,
        "gateM": args.gate_m,
        "gateCrossings": gate_crossings,
        "evidence": str(args.output),
    }
    print(json.dumps(result, indent=2))

    if result["coverage"] < 0.90:
        print("KPI failed: detection coverage is below 90%", file=sys.stderr)
        return 10
    if result["effectiveHz"] < 5.0:
        print("KPI failed: effective inference rate is below 5 Hz", file=sys.stderr)
        return 11
    if distances and distances[-1] >= distances[0]:
        print(
            "KPI failed: distance trend does not approach ego from first to last detection",
            file=sys.stderr,
        )
        return 12
    if gate_crossings != 1:
        print(
            f"KPI failed: expected one {args.gate_m:g} m gate crossing, got {gate_crossings}",
            file=sys.stderr,
        )
        return 13
    return 0


if __name__ == "__main__":
    sys.exit(main())
