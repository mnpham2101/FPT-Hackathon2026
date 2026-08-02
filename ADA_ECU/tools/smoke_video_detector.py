#!/usr/bin/env python3
"""Smoke-test the Phase 3 video detector path with a generated video."""

from __future__ import annotations

import json
import pathlib
import subprocess
import sys
import tempfile


def main() -> int:
    repo_root = pathlib.Path(__file__).resolve().parents[2]
    video_path = pathlib.Path(tempfile.gettempdir()) / "ada_phase3_sample.mp4"

    make_video = subprocess.run(
        [
            sys.executable,
            str(repo_root / "ADA_ECU/tools/make_sample_video.py"),
            "--output",
            str(video_path),
        ],
        check=False,
        text=True,
        capture_output=True,
    )
    if make_video.returncode != 0:
        print(make_video.stderr, file=sys.stderr)
        return make_video.returncode

    detector = subprocess.run(
        [
            sys.executable,
            str(repo_root / "ADA_ECU/tools/video_detector.py"),
            "--video",
            str(video_path),
            "--every-n-frames",
            "3",
            "--limit",
            "2",
        ],
        check=False,
        text=True,
        capture_output=True,
    )
    if detector.returncode != 0:
        print(detector.stderr, file=sys.stderr)
        return detector.returncode

    lines = [line for line in detector.stdout.splitlines() if line.strip()]
    if len(lines) != 2:
        print(f"expected 2 R3 JSONL lines, got {len(lines)}", file=sys.stderr)
        return 10

    for line in lines:
        event = json.loads(line)
        assert event["id"] == "own:B"
        assert event["source"] == "own_sensor"
        assert event["class"] == "vehicle"
        assert event["distance"] >= 0
        assert event["timestamps"]["measured"] >= 0

    print("phase3 video detector smoke: pass")
    return 0


if __name__ == "__main__":
    sys.exit(main())

