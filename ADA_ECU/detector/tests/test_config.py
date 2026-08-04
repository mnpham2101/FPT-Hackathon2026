"""12.3.2.1 — detector config loader: defaults, per-key overrides, rejections, os.environ path.

Runs via ``python -m pytest ADA_ECU/detector/tests/test_config.py`` from the repo root.
Tests pass env dicts to ``load_config``; only the monkeypatch fixture (sanctioned) touches
the process env, and only for the ``env=None`` path.
"""

import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import config  # noqa: E402
from config import DetectorConfig, load_config  # noqa: E402

DEFAULTS = {
    "video_clip_path": "/app/media/ego-b-occluding-c.mp4",
    "frame_stride": 4,
    "loop": True,
    "realtime_pacing": True,
    "clip_fps": 20.0,
    "start_delay_s": 0.0,
    "model_path": "/app/models/yolo11n.onnx",
    "conf_threshold": 0.35,
    "iou_threshold": 0.45,
    "track_iou_min": 0.3,
    "vehicle_width_m": 2.6,
    "camera_hfov_deg": 34.4,
}

# field -> (env key, override raw value, expected parsed value)
OVERRIDES = {
    "video_clip_path": ("VIDEO_CLIP_PATH", "/data/other.mp4", "/data/other.mp4"),
    "frame_stride": ("DETECTOR_FRAME_STRIDE", "2", 2),
    "loop": ("DETECTOR_LOOP", "false", False),
    "realtime_pacing": ("DETECTOR_REALTIME_PACING", "FALSE", False),
    "clip_fps": ("DETECTOR_CLIP_FPS", "30", 30.0),
    "start_delay_s": ("DETECTOR_START_DELAY_S", "1.5", 1.5),
    "model_path": ("MODEL_PATH", "/models/custom.onnx", "/models/custom.onnx"),
    "conf_threshold": ("CONF_THRESHOLD", "0.5", 0.5),
    "iou_threshold": ("IOU_THRESHOLD", "0.6", 0.6),
    "track_iou_min": ("TRACK_IOU_MIN", "0.25", 0.25),
    "vehicle_width_m": ("VEHICLE_WIDTH_M", "2.1", 2.1),
    "camera_hfov_deg": ("CAMERA_HFOV_DEG", "90", 90.0),
}

REJECTIONS = [
    ("DETECTOR_FRAME_STRIDE", "0"),
    ("DETECTOR_FRAME_STRIDE", "four"),
    ("CONF_THRESHOLD", "1.5"),
    ("IOU_THRESHOLD", "-0.1"),
    ("TRACK_IOU_MIN", "2"),
    ("CAMERA_HFOV_DEG", "0"),
    ("CAMERA_HFOV_DEG", "180"),
    ("VEHICLE_WIDTH_M", "0"),
    ("DETECTOR_CLIP_FPS", "0"),
    ("DETECTOR_START_DELAY_S", "-1"),
    ("DETECTOR_LOOP", "maybe"),
]


def test_all_defaults_from_empty_env():
    assert load_config({}) == DetectorConfig(**DEFAULTS)


@pytest.mark.parametrize("field", sorted(OVERRIDES))
def test_each_key_override_takes_effect(field):
    key, raw, expected = OVERRIDES[field]
    cfg = load_config({key: raw})
    assert getattr(cfg, field) == expected
    # Every other field keeps its default.
    for other, default in DEFAULTS.items():
        if other != field:
            assert getattr(cfg, other) == default


@pytest.mark.parametrize("key,raw", REJECTIONS)
def test_invalid_value_raises_naming_the_variable(key, raw):
    with pytest.raises(ValueError) as excinfo:
        load_config({key: raw})
    assert key in str(excinfo.value)


def test_env_none_reads_os_environ(monkeypatch):
    monkeypatch.setenv("DETECTOR_FRAME_STRIDE", "7")
    monkeypatch.setenv("DETECTOR_LOOP", "false")
    cfg = load_config()
    assert cfg.frame_stride == 7
    assert cfg.loop is False


def test_config_is_frozen():
    cfg = load_config({})
    with pytest.raises(Exception):
        cfg.frame_stride = 9
