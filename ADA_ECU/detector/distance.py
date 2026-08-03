"""Monocular pinhole projection and frame-to-frame range rate."""

from __future__ import annotations


class PinholeProjector:
    def __init__(self, vehicle_width_m: float, focal_px: float) -> None:
        if vehicle_width_m <= 0 or focal_px <= 0:
            raise ValueError("vehicle width and focal length must be > 0")
        self.vehicle_width_m = vehicle_width_m
        self.focal_px = focal_px

    def project(
        self, bbox_width_px: float, center_x: float, frame_width: int
    ) -> tuple[float, float]:
        if bbox_width_px <= 1.0:
            raise ValueError("bbox width must be > 1 px")
        if frame_width <= 0:
            raise ValueError("frame width must be > 0")
        distance_m = max(
            0.1, (self.vehicle_width_m * self.focal_px) / bbox_width_px
        )
        lateral_m = (
            (center_x - frame_width / 2.0) / self.focal_px
        ) * distance_m
        return distance_m, lateral_m


class RangeRateEstimator:
    def __init__(self) -> None:
        self.previous_distance_m: float | None = None
        self.previous_timestamp_ms: int | None = None

    def update(self, distance_m: float, timestamp_ms: int) -> float:
        speed_mps = 0.0
        if (
            self.previous_distance_m is not None
            and self.previous_timestamp_ms is not None
        ):
            elapsed_s = (timestamp_ms - self.previous_timestamp_ms) / 1000.0
            if elapsed_s > 0:
                speed_mps = abs(distance_m - self.previous_distance_m) / elapsed_s
        self.previous_distance_m = distance_m
        self.previous_timestamp_ms = timestamp_ms
        return speed_mps
