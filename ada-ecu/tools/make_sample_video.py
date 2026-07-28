#!/usr/bin/env python3
"""Create a tiny synthetic ego video for Phase 3 detector smoke tests."""

from __future__ import annotations

import argparse
import pathlib
import sys


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", default="/tmp/ada_phase3_sample.mp4")
    parser.add_argument("--frames", type=int, default=12)
    parser.add_argument("--width", type=int, default=640)
    parser.add_argument("--height", type=int, default=360)
    parser.add_argument("--fps", type=float, default=10.0)
    return parser.parse_args()


def main() -> int:
    try:
        import cv2  # type: ignore
        import numpy as np
    except ModuleNotFoundError:
        print("OpenCV/numpy not installed. Run: python3 -m pip install -r ada-ecu/requirements.txt", file=sys.stderr)
        return 1

    args = parse_args()
    output = pathlib.Path(args.output)
    output.parent.mkdir(parents=True, exist_ok=True)

    writer = cv2.VideoWriter(
        str(output),
        cv2.VideoWriter_fourcc(*"mp4v"),
        args.fps,
        (args.width, args.height),
    )
    if not writer.isOpened():
        print(f"cannot open video writer: {output}", file=sys.stderr)
        return 2

    for frame_index in range(args.frames):
        frame = np.zeros((args.height, args.width, 3), dtype=np.uint8)
        frame[:] = (32, 32, 32)
        lane_y = int(args.height * 0.62)
        cv2.line(frame, (0, lane_y), (args.width, lane_y), (100, 100, 100), 2)
        car_w, car_h = 80, 36
        car_x = int(args.width * 0.45 + frame_index * 4)
        car_y = int(args.height * 0.48)
        cv2.rectangle(frame, (car_x, car_y), (car_x + car_w, car_y + car_h), (240, 240, 240), -1)
        cv2.rectangle(frame, (car_x, car_y), (car_x + car_w, car_y + car_h), (40, 180, 255), 2)
        cv2.putText(frame, "B", (car_x + 30, car_y + 25), cv2.FONT_HERSHEY_SIMPLEX, 0.8, (0, 0, 0), 2)
        writer.write(frame)

    writer.release()
    print(output)
    return 0


if __name__ == "__main__":
    sys.exit(main())

