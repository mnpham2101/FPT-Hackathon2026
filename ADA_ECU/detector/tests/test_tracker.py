"""12.3.2.4 — greedy IoU tracker: id survival, rebirth below iou_min, no swap, no id reuse."""

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from tracker import Tracker  # noqa: E402


def test_id_survives_small_drift():
    t = Tracker(iou_min=0.3)
    (first,) = t.assign([(100.0, 100.0, 50.0, 50.0)])
    (second,) = t.assign([(104.0, 102.0, 50.0, 50.0)])
    assert first == second == "own:0"


def test_jump_below_iou_min_yields_new_id():
    t = Tracker(iou_min=0.3)
    (first,) = t.assign([(0.0, 0.0, 10.0, 10.0)])
    (second,) = t.assign([(500.0, 500.0, 10.0, 10.0)])  # zero overlap
    assert first == "own:0"
    assert second == "own:1"


def test_two_concurrent_tracks_keep_distinct_ids_and_do_not_swap():
    t = Tracker(iou_min=0.3)
    a0, b0 = t.assign([(0.0, 0.0, 50.0, 50.0), (200.0, 0.0, 50.0, 50.0)])
    assert a0 != b0
    # both drift slightly; input order reversed to prove matching is by IoU, not position
    b1, a1 = t.assign([(203.0, 2.0, 50.0, 50.0), (3.0, 2.0, 50.0, 50.0)])
    assert a1 == a0
    assert b1 == b0


def test_ids_are_never_reused():
    t = Tracker(iou_min=0.3)
    (first,) = t.assign([(0.0, 0.0, 20.0, 20.0)])
    assert first == "own:0"
    assert t.assign([]) == []  # track dropped
    (fresh,) = t.assign([(0.0, 0.0, 20.0, 20.0)])  # same place, new object
    assert fresh == "own:1"
    assert fresh != first
