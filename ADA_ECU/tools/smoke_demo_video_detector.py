#!/usr/bin/env python3
"""Smoke-test the demo video path.

This validates the Phase 3 demo contract: a real video file is decoded by
OpenCV, sampled by the detector seam, and converted into R3 own-sensor JSONL.
The current backend is intentionally `placeholder`; it proves the video-to-R3
pipeline before the YOLO backend is wired.
"""

from __future__ import annotations

import json
import pathlib
import subprocess
import sys


def main() -> int:
    repo_root = pathlib.Path(__file__).resolve().parents[2]
    video_path = repo_root / "ADA_ECU/media/ego-b-occluding-c.mp4"
    detector_path = repo_root / "ADA_ECU/tools/video_detector.py"

    if not video_path.exists():
        print(f"demo video not found: {video_path}", file=sys.stderr)
        return 2

    result = subprocess.run(
        [
            sys.executable,
            str(detector_path),
            "--video",
            str(video_path),
            "--backend",
            "placeholder",
            "--every-n-frames",
            "30",
            "--limit",
            "5",
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
            "expected at least one R3 detection from demo video, got none",
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
        if float(detection.get("distance", -1)) <= 0:
            print(f"invalid distance: {detection.get('distance')}", file=sys.stderr)
            return 14

    print(f"phase3 demo video detector: pass ({len(detections)} R3 objects)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
