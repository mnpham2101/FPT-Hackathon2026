"""Pinhole known-width range and lateral offset — pure functions, all inputs parameters (D6,
ADA_ECU/doc/ada-ecu-design-decisions.md)."""

from math import radians, tan
from typing import Optional


def focal_px(frame_w: int, hfov_deg: float) -> float:
    """Focal length in pixels from the horizontal FOV: f_px = (frame_w / 2) / tan(hfov / 2) (D6)."""
    return (frame_w / 2) / tan(radians(hfov_deg) / 2)


def estimate_range(
    bbox_w_px: float, frame_w: int, hfov_deg: float, vehicle_width_m: float
) -> Optional[float]:
    """Known-width pinhole range d = vehicle_width_m * f_px / bbox_w_px (D6); None on degenerate width."""
    if bbox_w_px <= 0:
        return None
    return vehicle_width_m * focal_px(frame_w, hfov_deg) / bbox_w_px


def lateral_offset_m(
    bbox_u_center: float, frame_w: int, distance_m: float, hfov_deg: float
) -> float:
    """Lateral offset y = (bbox_u_center - frame_w / 2) * d / f_px (D6); positive right of centre."""
    return (bbox_u_center - frame_w / 2) * distance_m / focal_px(frame_w, hfov_deg)
