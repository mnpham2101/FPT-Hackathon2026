"""Frame-source seam — frame acquisition behind an interface (HLD D6).

This file is the only detector module allowed to import cv2 (layer rule); the
import is lazy inside ``FileFrameSource`` so ``SyntheticFrameSource`` works
without cv2 installed. A future live source (camera, stream) is one new
``FrameSource`` implementation. Pacing is not this module's job:
``iter_frames()`` is a plain generator assuming nothing about the consumer's
rate.
"""

from dataclasses import dataclass
from typing import Iterator, Optional, Protocol

import numpy as np

# Timestamp fallback when the container declares no usable fps.
_FALLBACK_FPS = 20.0


@dataclass
class Frame:
    index: int          # decoded-frame index, accumulating across loops (never resets)
    clip_index: int     # within-clip decoded-frame index (resets each loop)
    timestamp_ms: float # clip time = index / fps * 1000 — detector-local, advances across loops, never an R3 timestamp
    image: "np.ndarray"
    width: int
    height: int


class FrameSource(Protocol):
    declared_fps: Optional[float]

    def iter_frames(self) -> Iterator[Frame]: ...


class FileFrameSource:
    """Wraps ``cv2.VideoCapture`` over a clip on disk, yielding every stride-th decoded frame."""

    def __init__(self, path: str, stride: int, loop: bool):
        import cv2  # lazy: keeps this module importable (SyntheticFrameSource) without cv2

        self._cv2 = cv2
        self.path = path
        self.stride = stride
        self.loop = loop
        capture = cv2.VideoCapture(path)
        if not capture.isOpened():
            capture.release()
            raise RuntimeError(f"FileFrameSource cannot open video file: {path}")
        fps = capture.get(cv2.CAP_PROP_FPS)
        self.declared_fps: Optional[float] = fps if fps and fps > 0 else None
        self._capture = capture

    def iter_frames(self) -> Iterator[Frame]:
        capture = self._capture
        # Fall back to the default cadence when the container declares no fps.
        fps = self.declared_fps if self.declared_fps is not None else _FALLBACK_FPS
        # index accumulates across loops: the pacer computes deadlines from it, and a resetting index would move deadlines backwards.
        index = 0
        while True:
            clip_index = 0
            while True:
                ok, image = capture.read()
                if not ok:
                    break
                if clip_index % self.stride == 0:
                    height, width = image.shape[:2]
                    yield Frame(
                        index=index,
                        clip_index=clip_index,
                        timestamp_ms=index / fps * 1000.0,
                        image=image,
                        width=width,
                        height=height,
                    )
                index += 1
                clip_index += 1
            if not self.loop:
                capture.release()
                return
            # EOF with loop=True: re-open from frame 0 (a fresh capture is more reliable than seeking).
            capture.release()
            capture = self._cv2.VideoCapture(self.path)
            if not capture.isOpened():
                raise RuntimeError(f"FileFrameSource cannot re-open video file: {self.path}")
            self._capture = capture


class SyntheticFrameSource:
    """Deterministic in-memory frames, no clip on disk — the CI import-and-plumbing path, never a detection-evidence path."""

    def __init__(self, count: int, width: int = 640, height: int = 384, fps: float = 20.0, stride: int = 1):
        self.count = count
        self.width = width
        self.height = height
        self.stride = stride
        self.declared_fps: Optional[float] = fps

    def iter_frames(self) -> Iterator[Frame]:
        fps = self.declared_fps if self.declared_fps else _FALLBACK_FPS
        for i in range(self.count):
            index = i * self.stride  # the decoded-frame ordinal this sample would have had
            image = np.full((self.height, self.width, 3), 128, dtype=np.uint8)
            # Varying pixel strip: the top row encodes the frame ordinal so frames are distinct.
            image[0, :, :] = index % 256
            yield Frame(
                index=index,
                clip_index=index,
                timestamp_ms=index / fps * 1000.0,
                image=image,
                width=self.width,
                height=self.height,
            )
