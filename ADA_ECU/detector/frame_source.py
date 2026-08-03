"""Synthetic and OpenCV file frame sources."""

from __future__ import annotations

import time
from collections.abc import Iterator

from detector.models import FrameInput


def synthetic_samples(count: int, start_ms: int) -> Iterator[FrameInput]:
    for frame_index in range(count):
        yield FrameInput(
            frame_index=frame_index,
            timestamp_ms=start_ms + frame_index * 100,
            width=1280,
            height=720,
        )


def video_samples(
    video_path: str,
    every_n_frames: int,
    limit: int | None,
    realtime: bool = False,
    loop: bool = False,
) -> Iterator[FrameInput]:
    try:
        import cv2  # type: ignore
    except ModuleNotFoundError as exc:
        raise RuntimeError(
            "OpenCV is not installed. Install with: python3 -m pip install -r "
            "ADA_ECU/requirements.txt"
        ) from exc

    capture = cv2.VideoCapture(video_path)
    if not capture.isOpened():
        raise RuntimeError(f"cannot open video: {video_path}")
    fps = capture.get(cv2.CAP_PROP_FPS)
    if not fps or fps <= 0:
        fps = 10.0
    emitted = 0
    frame_index = 0
    loop_start = time.monotonic()
    try:
        while True:
            ok, frame = capture.read()
            if not ok:
                if not loop:
                    break
                capture.set(cv2.CAP_PROP_POS_FRAMES, 0)
                frame_index = 0
                loop_start = time.monotonic()
                continue
            if frame_index % every_n_frames == 0:
                timestamp_ms = int(frame_index * 1000 / fps)
                if realtime:
                    delay_s = loop_start + timestamp_ms / 1000.0 - time.monotonic()
                    if delay_s > 0:
                        time.sleep(delay_s)
                height, width = frame.shape[:2]
                yield FrameInput(
                    frame_index,
                    timestamp_ms,
                    int(width),
                    int(height),
                    frame,
                )
                emitted += 1
                if limit is not None and emitted >= limit:
                    break
            frame_index += 1
    finally:
        capture.release()
