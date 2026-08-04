"""Detector configuration loader — the detector's only env reader (HLD §6).

Every tunable of the R12 detector enters through ``load_config``: the env keys,
their defaults and their validation live in the ``_SPEC`` table here and nowhere
else. No other module in ``detector/`` touches ``os.environ``.

Validation rules cover only single-value domains (type, range, non-emptiness of
each variable on its own); cross-variable consistency is out of scope here.
"""

import os
from dataclasses import dataclass
from typing import Callable, Mapping, Optional


@dataclass(frozen=True)
class DetectorConfig:
    video_clip_path: str
    frame_stride: int
    loop: bool
    realtime_pacing: bool
    clip_fps: float
    start_delay_s: float
    model_path: str
    conf_threshold: float
    iou_threshold: float
    track_iou_min: float
    vehicle_width_m: float
    camera_hfov_deg: float


def _non_empty_str(name: str, raw: str) -> str:
    if not raw:
        raise ValueError(f"{name} must be a non-empty path, got {raw!r}")
    return raw


def _int_min(minimum: int) -> Callable[[str, str], int]:
    def parse(name: str, raw: str) -> int:
        try:
            value = int(raw)
        except ValueError:
            value = None
        if value is None or value < minimum:
            raise ValueError(f"{name} must be an integer >= {minimum}, got {raw!r}")
        return value

    return parse


def _bool(name: str, raw: str) -> bool:
    lowered = raw.lower()
    if lowered == "true":
        return True
    if lowered == "false":
        return False
    raise ValueError(f"{name} must be 'true' or 'false', got {raw!r}")


def _float_in(
    low: float,
    high: Optional[float],
    low_exclusive: bool,
    high_exclusive: bool,
    domain: str,
) -> Callable[[str, str], float]:
    def parse(name: str, raw: str) -> float:
        try:
            value = float(raw)
        except ValueError:
            raise ValueError(f"{name} must be a float {domain}, got {raw!r}") from None
        low_ok = value > low if low_exclusive else value >= low
        high_ok = True
        if high is not None:
            high_ok = value < high if high_exclusive else value <= high
        if not (low_ok and high_ok):
            raise ValueError(f"{name} must be a float {domain}, got {raw!r}")
        return value

    return parse


_float_positive = _float_in(0.0, None, True, False, "> 0")
_float_non_negative = _float_in(0.0, None, False, False, ">= 0")
_float_unit_interval = _float_in(0.0, 1.0, False, False, "in [0, 1]")
_float_hfov = _float_in(0.0, 180.0, True, True, "in (0, 180) exclusive")

# The single table of tunables: field name -> (env key, default, parser).
# Order matches the DetectorConfig field order.
_SPEC = {
    "video_clip_path": ("VIDEO_CLIP_PATH", "/app/media/ego-b-occluding-c.mp4", _non_empty_str),
    "frame_stride": ("DETECTOR_FRAME_STRIDE", "4", _int_min(1)),
    "loop": ("DETECTOR_LOOP", "true", _bool),
    "realtime_pacing": ("DETECTOR_REALTIME_PACING", "true", _bool),
    "clip_fps": ("DETECTOR_CLIP_FPS", "20.0", _float_positive),
    "start_delay_s": ("DETECTOR_START_DELAY_S", "0.0", _float_non_negative),
    "model_path": ("MODEL_PATH", "/app/models/yolo11n.onnx", _non_empty_str),
    "conf_threshold": ("CONF_THRESHOLD", "0.35", _float_unit_interval),
    "iou_threshold": ("IOU_THRESHOLD", "0.45", _float_unit_interval),
    "track_iou_min": ("TRACK_IOU_MIN", "0.3", _float_unit_interval),
    "vehicle_width_m": ("VEHICLE_WIDTH_M", "1.8", _float_positive),
    "camera_hfov_deg": ("CAMERA_HFOV_DEG", "60", _float_hfov),
}


def load_config(env: Optional[Mapping[str, str]] = None) -> DetectorConfig:
    """Build a DetectorConfig from ``env``, or from ``os.environ`` when None.

    Raises ValueError naming the offending env variable on any invalid value.
    """
    if env is None:
        env = os.environ
    fields = {
        field: parser(key, env.get(key, default))
        for field, (key, default, parser) in _SPEC.items()
    }
    return DetectorConfig(**fields)
