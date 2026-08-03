"""Value types and the detector backend seam."""

from __future__ import annotations

from collections.abc import Iterable
from dataclasses import dataclass
from typing import Protocol


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
