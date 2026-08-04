"""Tests for the R19 zero-C audit ``tools/check_zero_c.py``.

Writes small JSONL detection-log and ``[EVT]``-stream fixtures and asserts
exit codes, the rule named on violation, and the examined counts printed on a
clean run. Runs the script as a subprocess, exercising the real CLI.
Standard library plus pytest only — no third-party fixtures.

Verified locally only — no CI lane collects ``ADA_ECU/tools/tests/``.
"""

from __future__ import annotations

import json
import subprocess
import sys
from pathlib import Path
from typing import Optional, Sequence

SCRIPT: Path = Path(__file__).resolve().parents[1] / "check_zero_c.py"


def detection_line(
    *,
    track_id: str = "own:1",
    source: str = "own_sensor",
    distance: float = 42.0,
    measured_ms: int = 1_000,
) -> str:
    """One R3 detection-log line with every schema field present."""
    return json.dumps(
        {
            "id": track_id,
            "class": "vehicle",
            "source": source,
            "position": {"x": 1.0, "y": distance},
            "distance": distance,
            "speed": 0.0,
            "confidence": 0.9,
            "state": "tracked",
            "timestamps": {
                "measured": measured_ms,
                "received": measured_ms,
                "lastUpdated": measured_ms,
            },
        }
    )


def evt_line(*, track_id: str, distance: float, epoch_ms: int) -> str:
    """One ``[EVT]`` track_transition line carrying a relayed range sample."""
    return "[EVT] " + json.dumps(
        {
            "event": "track_transition",
            "mono_ms": 123,
            "epoch_ms": epoch_ms,
            "payload": {
                "id": track_id,
                "source": "v2x_relayed",
                "from": "new",
                "to": "tracked",
                "distance": distance,
                "reason": "admit",
            },
        }
    )


CLEAN_LINES = [
    detection_line(track_id="own:1", distance=42.0, measured_ms=1_000),
    detection_line(track_id="own:2", distance=18.5, measured_ms=1_000),
    detection_line(track_id="own:1", distance=41.0, measured_ms=1_100),
    detection_line(track_id="own:2", distance=18.0, measured_ms=1_100),
]


def write_log(path: Path, lines: Sequence[str]) -> Path:
    path.write_text("\n".join(lines) + ("\n" if lines else ""), encoding="utf-8")
    return path


def run_script(args: Sequence[str]) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [sys.executable, str(SCRIPT), *args], capture_output=True, text=True
    )


def test_clean_log_exits_0_with_nonzero_counts(tmp_path: Path) -> None:
    log = write_log(tmp_path / "detections.jsonl", CLEAN_LINES)
    proc = run_script([str(log)])
    assert proc.returncode == 0, proc.stdout + proc.stderr
    assert "detection_lines=4" in proc.stdout
    assert "own_sensor_lines=4" in proc.stdout
    assert "rule3=skipped" in proc.stdout  # no --evt, stated rather than implied


def test_clean_log_with_far_relayed_track_exits_0(tmp_path: Path) -> None:
    # A relayed sample well outside the radius at a matching timestamp: the
    # comparison runs (count visible) and passes.
    log = write_log(tmp_path / "detections.jsonl", CLEAN_LINES)
    evt = write_log(
        tmp_path / "evt.log",
        [
            "some non-EVT stderr noise that must be ignored",
            evt_line(track_id="v2x:42:1", distance=90.0, epoch_ms=1_000),
        ],
    )
    proc = run_script([str(log), "--evt", str(evt)])
    assert proc.returncode == 0, proc.stdout + proc.stderr
    assert "relayed_evt_samples=1" in proc.stdout
    # All four own lines (measured 1000 and 1100) sit within the 500 ms
    # tolerance of epoch 1000, so all four comparisons run.
    assert "time_matched_comparisons=4" in proc.stdout


def test_rule_1_relayed_source_in_detection_log(tmp_path: Path) -> None:
    lines = CLEAN_LINES + [
        detection_line(track_id="own:3", source="v2x_relayed", distance=47.0)
    ]
    log = write_log(tmp_path / "detections.jsonl", lines)
    proc = run_script([str(log)])
    assert proc.returncode == 1
    assert "VIOLATION rule 1" in proc.stdout
    assert ":5:" in proc.stdout  # the offending line number
    assert "v2x_relayed" in proc.stdout


def test_rule_2_v2x_namespace_id_in_detection_log(tmp_path: Path) -> None:
    lines = CLEAN_LINES + [detection_line(track_id="v2x:7:3", distance=47.0)]
    log = write_log(tmp_path / "detections.jsonl", lines)
    proc = run_script([str(log)])
    assert proc.returncode == 1
    assert "VIOLATION rule 2" in proc.stdout
    assert "v2x:7:3" in proc.stdout


def test_rule_3_own_track_within_radius_of_relayed_c(tmp_path: Path) -> None:
    # own:2 at 18.5 m / measured 1000 vs relayed v2x:42:1 at 20.0 m /
    # epoch 1000: |18.5 - 20.0| <= 5.0 within the 500 ms tolerance.
    log = write_log(tmp_path / "detections.jsonl", CLEAN_LINES)
    evt = write_log(
        tmp_path / "evt.log",
        [evt_line(track_id="v2x:42:1", distance=20.0, epoch_ms=1_000)],
    )
    proc = run_script([str(log), "--evt", str(evt)])
    assert proc.returncode == 1
    assert "VIOLATION rule 3" in proc.stdout
    assert "v2x:42:1" in proc.stdout
    assert ":2:" in proc.stdout  # the own:2 line at 18.5 m


def test_rule_3_needs_matching_timestamp(tmp_path: Path) -> None:
    # Same ranges, but the relayed sample is 10 s away: outside the tolerance,
    # no comparison, no violation.
    log = write_log(tmp_path / "detections.jsonl", CLEAN_LINES)
    evt = write_log(
        tmp_path / "evt.log",
        [evt_line(track_id="v2x:42:1", distance=20.0, epoch_ms=11_000)],
    )
    proc = run_script([str(log), "--evt", str(evt)])
    assert proc.returncode == 0, proc.stdout + proc.stderr
    assert "time_matched_comparisons=0" in proc.stdout


def test_radius_is_a_cli_argument(tmp_path: Path) -> None:
    # 90 m apart passes at the default radius; --radius-m 100 makes it fire.
    log = write_log(tmp_path / "detections.jsonl", CLEAN_LINES)
    evt = write_log(
        tmp_path / "evt.log",
        [evt_line(track_id="v2x:42:1", distance=90.0, epoch_ms=1_000)],
    )
    proc = run_script([str(log), "--evt", str(evt), "--radius-m", "100"])
    assert proc.returncode == 1
    assert "VIOLATION rule 3" in proc.stdout


def test_empty_log_is_not_a_pass(tmp_path: Path) -> None:
    log = write_log(tmp_path / "empty.jsonl", [])
    proc = run_script([str(log)])
    assert proc.returncode == 2
    assert "zero parseable" in proc.stderr


def test_blank_lines_only_is_not_a_pass(tmp_path: Path) -> None:
    log = write_log(tmp_path / "blank.jsonl", ["", "   ", ""])
    proc = run_script([str(log)])
    assert proc.returncode == 2


def test_unparseable_line_names_the_line(tmp_path: Path) -> None:
    lines = CLEAN_LINES[:2] + ["{not json"] + CLEAN_LINES[2:]
    log = write_log(tmp_path / "corrupt.jsonl", lines)
    proc = run_script([str(log)])
    assert proc.returncode == 2
    assert ":3:" in proc.stderr  # the corrupt line's number
    assert "not evidence" in proc.stderr


def test_missing_file_is_usage_error(tmp_path: Path) -> None:
    proc = run_script([str(tmp_path / "does-not-exist.jsonl")])
    assert proc.returncode == 2
