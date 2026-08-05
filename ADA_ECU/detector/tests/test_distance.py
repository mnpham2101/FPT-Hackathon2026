"""12.3.2.3 — pinhole range and lateral offset: hand-computed values, monotonicity, sign, degenerate width."""

import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from distance import estimate_range, focal_px, lateral_offset_m  # noqa: E402


def test_hand_computed_ranges():
    # frame_w=1280, hfov=90 -> f_px = 640 / tan(45deg) = 640.0
    assert focal_px(1280, 90.0) == pytest.approx(640.0)
    # d = 1.8 * 640.0 / 64 = 18.0
    assert estimate_range(64.0, 1280, 90.0, 1.8) == pytest.approx(18.0)

    # frame_w=1920, hfov=60 -> f_px = 960 / tan(30deg) = 1662.7687752661222
    assert focal_px(1920, 60.0) == pytest.approx(1662.7687752661222)
    # d = 2.0 * 1662.7687752661222 / 100 = 33.255375505322444
    assert estimate_range(100.0, 1920, 60.0, 2.0) == pytest.approx(33.255375505322444)

    # frame_w=640, hfov=120 -> f_px = 320 / tan(60deg) = 184.75208614068024
    assert focal_px(640, 120.0) == pytest.approx(184.75208614068024)
    # d = 1.8 * 184.75208614068024 / 50 = 6.651075101064489
    assert estimate_range(50.0, 640, 120.0, 1.8) == pytest.approx(6.651075101064489)


def test_wider_bbox_is_nearer():
    near = estimate_range(120.0, 1280, 90.0, 1.8)
    far = estimate_range(40.0, 1280, 90.0, 1.8)
    assert near < far


def test_bbox_right_of_centre_gives_positive_offset():
    # frame_w=1280 -> centre u=640; u=800, d=18, f_px=640 -> y = (800-640)*18/640 = 4.5
    y = lateral_offset_m(800.0, 1280, 18.0, 90.0)
    assert y == pytest.approx(4.5)
    assert y > 0
    # left of centre is negative, mirror-symmetric
    assert lateral_offset_m(480.0, 1280, 18.0, 90.0) == pytest.approx(-4.5)


def test_degenerate_width_returns_none():
    assert estimate_range(0.0, 1280, 90.0, 1.8) is None
    assert estimate_range(-10.0, 1280, 90.0, 1.8) is None
