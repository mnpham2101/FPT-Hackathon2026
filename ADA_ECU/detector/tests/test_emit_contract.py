"""12.3.2.6 — R3Emitter contract tests: emitted JSONL validates against the frozen R3 schema
loaded from disk, field conventions hold, and the speed/timestamp rules behave as designed."""

import io
import json
import sys
from pathlib import Path

import jsonschema

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from emit import EmitItem, R3Emitter  # noqa: E402

SCHEMA_PATH = Path(__file__).resolve().parents[2] / "contracts" / "r3-tracked-object.schema.json"
SCHEMA = json.loads(SCHEMA_PATH.read_text(encoding="utf-8"))


def _emit_lines(emitter, items, measured_ms):
    stream = io.StringIO()
    emitter.emit_frame(items, measured_ms, stream)
    text = stream.getvalue()
    assert text == "" or text.endswith("\n")
    return [json.loads(line) for line in text.splitlines()]


def _item(track_id="own:1", distance_m=40.0, lateral_m=1.5, confidence=0.9, clip_ts_ms=0.0):
    return EmitItem(
        track_id=track_id,
        distance_m=distance_m,
        lateral_m=lateral_m,
        confidence=confidence,
        clip_ts_ms=clip_ts_ms,
    )


def test_emitted_lines_validate_against_schema_from_disk():
    emitter = R3Emitter(now_ms=lambda: 1_700_000_000_500)
    items = [
        _item(track_id="own:1", distance_m=40.0, lateral_m=1.5, confidence=0.9, clip_ts_ms=0.0),
        # Adversarial inputs: negative range and out-of-range confidence must still validate.
        _item(track_id="own:2", distance_m=-3.0, lateral_m=-0.5, confidence=1.7, clip_ts_ms=0.0),
        _item(track_id="own:3", distance_m=12.0, lateral_m=0.0, confidence=-0.2, clip_ts_ms=0.0),
    ]
    lines = _emit_lines(emitter, items, measured_ms=1_700_000_000_000)
    assert len(lines) == 3
    for obj in lines:
        jsonschema.validate(obj, SCHEMA)
    # The clamps, explicitly.
    assert lines[1]["distance"] == 0.0
    assert lines[1]["confidence"] == 1.0
    assert lines[2]["confidence"] == 0.0


def test_id_source_class_conventions_and_no_v2x():
    emitter = R3Emitter(now_ms=lambda: 1_700_000_000_500)
    stream = io.StringIO()
    emitter.emit_frame(
        [_item(track_id="own:7", distance_m=-1.0, confidence=2.0)],
        measured_ms=1_700_000_000_000,
        stream=stream,
    )
    emitted = stream.getvalue()
    obj = json.loads(emitted)
    assert obj["id"] == "own:7"
    assert obj["source"] == "own_sensor"
    assert obj["class"] == "vehicle"
    assert obj["state"] == "not_tracked"
    # No v2x_relayed line is producible: the module hardcodes own_sensor.
    assert "v2x" not in emitted
    source_text = (Path(__file__).resolve().parents[1] / "emit.py").read_text(encoding="utf-8")
    code_lines = [
        line for line in source_text.splitlines()
        if not line.lstrip().startswith("#") and not line.lstrip().startswith('"')
    ]
    assert all("v2x_relayed" not in line for line in code_lines)


def test_first_sampled_frame_speed_is_zero():
    emitter = R3Emitter(now_ms=lambda: 1_700_000_000_500)
    (obj,) = _emit_lines(emitter, [_item(distance_m=40.0, clip_ts_ms=0.0)], 1_700_000_000_000)
    assert obj["speed"] == 0.0


def test_second_frame_speed_is_hand_computed_delta():
    emitter = R3Emitter(now_ms=lambda: 1_700_000_000_500)
    _emit_lines(emitter, [_item(distance_m=40.0, clip_ts_ms=0.0)], 1_700_000_000_000)
    (obj,) = _emit_lines(emitter, [_item(distance_m=39.0, clip_ts_ms=200.0)], 1_700_000_000_200)
    # |40 - 39| m / 0.2 s = 5.0 m/s
    assert obj["speed"] == 5.0


def test_closing_track_emits_positive_speed_after_first_frame():
    emitter = R3Emitter(now_ms=lambda: 1_700_000_000_500)
    distances = [50.0, 47.5, 44.0]
    clip_ts = [0.0, 200.0, 400.0]
    speeds = []
    for d, ts in zip(distances, clip_ts):
        (obj,) = _emit_lines(emitter, [_item(distance_m=d, clip_ts_ms=ts)], 1_700_000_000_000 + int(ts))
        speeds.append(obj["speed"])
    assert speeds[0] == 0.0
    assert all(s > 0.0 for s in speeds[1:])


def test_measured_precedes_received_and_both_advance():
    clock = iter(range(1_700_000_001_000, 1_700_000_010_000, 1_000))
    emitter = R3Emitter(now_ms=lambda: next(clock))
    measured_values = [1_700_000_000_000, 1_700_000_000_200]
    results = []
    for measured in measured_values:
        (obj,) = _emit_lines(emitter, [_item(clip_ts_ms=float(measured))], measured)
        results.append(obj["timestamps"])
    for ts, measured in zip(results, measured_values):
        assert ts["measured"] == measured
        assert ts["measured"] < ts["received"]
        assert ts["lastUpdated"] == ts["received"]
    assert results[1]["measured"] > results[0]["measured"]
    assert results[1]["received"] > results[0]["received"]
