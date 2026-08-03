#!/usr/bin/env python3
"""Smoke-test real ML vehicle detection on the committed demo video."""

from __future__ import annotations

import json
import pathlib
import subprocess
import sys


def main() -> int:
    repo_root = pathlib.Path(__file__).resolve().parents[2]
    video_path = repo_root / "ADA_ECU/media/ego-b-occluding-c.mp4"
    model_path = repo_root / "ADA_ECU/models/yolo11n.onnx"
    detector_path = repo_root / "ADA_ECU/tools/video_detector.py"

    if not model_path.exists():
        print(
            f"YOLO model not found: {model_path}\n"
            "Run: python ADA_ECU/tools/download_yolo_model.py",
            file=sys.stderr,
        )
        return 2

    result = subprocess.run(
        [
            sys.executable,
            str(detector_path),
            "--video",
            str(video_path),
            "--backend",
            "yolo-onnx",
            "--model",
            str(model_path),
            "--every-n-frames",
            "20",
            "--limit",
            "5",
            "--confidence",
            "0.20",
            "--log-detections",
        ],
        check=False,
        text=True,
        capture_output=True,
    )
    if result.returncode != 0:
        print(result.stderr, file=sys.stderr)
        return result.returncode

    detections = [
        json.loads(line) for line in result.stdout.splitlines() if line.strip()
    ]
    if not detections:
        print(
            "expected at least one ML R3 detection from demo video, got none",
            file=sys.stderr,
        )
        return 10

    for detection in detections:
        if detection.get("id") != "own:B":
            print(f"unexpected object id: {detection.get('id')}", file=sys.stderr)
            return 11
        if detection.get("source") != "own_sensor":
            print(f"unexpected source: {detection.get('source')}", file=sys.stderr)
            return 12
        if detection.get("class") != "vehicle":
            print(f"unexpected class: {detection.get('class')}", file=sys.stderr)
            return 13
        if float(detection.get("confidence", 0)) <= 0.0:
            print(
                f"invalid model confidence: {detection.get('confidence')}",
                file=sys.stderr,
            )
            return 14
        if float(detection.get("distance", -1)) <= 0:
            print(
                f"invalid distance estimate: {detection.get('distance')}",
                file=sys.stderr,
            )
            return 15

    print(result.stderr, file=sys.stderr, end="")
    print(f"phase3 ML video detector: pass ({len(detections)} R3 objects)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
