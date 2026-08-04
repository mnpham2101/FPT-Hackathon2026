#!/usr/bin/env python3
"""Zero-C audit for the R19 detection log (ADA design decision D6).

Makes the R19 claim — the ego's own sensors never saw vehicle C — falsifiable
by scanning the detector's captured stdout (the R3 JSONL detection log, one
JSON object per line) and, optionally, the ADA ``[EVT]`` stream. Standard
library only; runs on the host, never deployed.

Three rules, each a violation that fails the run (exit 1, naming the line
number, an excerpt of its content, and the rule that fired):

1. A detection-log line whose ``source`` is ``v2x_relayed``. The detector can
   only produce ``own_sensor``; a relayed source in its log is a violation.
2. A detection-log line whose ``id`` is in the ``v2x:`` namespace. Own-sensor
   tracks are ``own:<n>``; the two namespaces cannot mint each other's ids.
3. Only when ``--evt`` is given: an own-sensor detection whose range sits
   within ``--radius-m`` of a relayed C sample at the same timestamp
   (``timestamps.measured`` within ``--time-tolerance-ms`` of the relayed
   sample's ``epoch_ms``). Relayed samples are extracted liberally from the
   ``[EVT]`` stream: any event payload carrying a ``v2x:``-namespace id and a
   numeric ``distance`` (e.g. ``track_transition``, ``r2_ingest``).

Interpretation note for rule 3: R2 relays C's *distance*; a raw ego-frame
position for C does not exist in the detection log's frame, so the comparison
is on the range series — the quantity both logs share. ``--time-tolerance-ms``
is an implementation parameter the design leaves open; its default is a CLI
default here, not a designed constant.

There is deliberately no fourth rule asserting the detector found nothing but
B — adjacent-lane and oncoming vehicles are expected in correct footage
(``ADA_ECU/media/ego-b-occluding-c.source.md`` § What this obliges downstream).

Exit codes: 0 clean — the examined counts are printed so a vacuous pass is
visible; 1 at least one rule fired; 2 nothing usable to examine — an empty
detection log (zero parseable lines) or an unparseable JSON line, because a
corrupt or empty log is not evidence.
"""

from __future__ import annotations

import argparse
import json
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Iterator, Optional, Sequence

_EVT_PREFIX = "[EVT]"
_V2X_NAMESPACE = "v2x:"
_OWN_SOURCE = "own_sensor"
_RELAYED_SOURCE = "v2x_relayed"
_EXCERPT_CHARS = 120


class LogError(RuntimeError):
    """The input cannot serve as evidence (empty or corrupt)."""


@dataclass(frozen=True)
class Detection:
    """One parsed detection-log line, with its provenance kept for messages."""

    line_no: int
    raw: str
    obj: dict


@dataclass(frozen=True)
class RelayedSample:
    """One relayed range sample extracted from the ``[EVT]`` stream."""

    line_no: int
    raw: str
    track_id: str
    epoch_ms: float
    distance: float


@dataclass(frozen=True)
class Violation:
    rule: int
    line_no: int
    raw: str
    detail: str


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument(
        "detection_log",
        type=Path,
        help="R3 JSONL detection log (the ADA detector's captured stdout)",
    )
    parser.add_argument(
        "--radius-m",
        type=float,
        default=5.0,
        help="rule-3 spatial radius in metres on the range series (default 5.0)",
    )
    parser.add_argument(
        "--time-tolerance-ms",
        type=float,
        default=500.0,
        help=(
            "rule-3 timestamp-matching tolerance in milliseconds between a "
            "detection's timestamps.measured and a relayed sample's epoch_ms "
            "(default 500; an implementation parameter the design leaves open)"
        ),
    )
    parser.add_argument(
        "--evt",
        type=Path,
        default=None,
        help="optional ADA [EVT] stream; enables rule 3 (non-[EVT] lines ignored)",
    )
    return parser


def _excerpt(raw: str) -> str:
    text = raw.strip()
    if len(text) > _EXCERPT_CHARS:
        return text[: _EXCERPT_CHARS - 3] + "..."
    return text


def parse_detection_log(path: Path) -> list[Detection]:
    """Parse every non-blank line as JSON; raise LogError on corruption/emptiness."""
    detections: list[Detection] = []
    with path.open("r", encoding="utf-8") as handle:
        for line_no, raw in enumerate(handle, start=1):
            if not raw.strip():
                continue
            try:
                obj = json.loads(raw)
            except json.JSONDecodeError as exc:
                raise LogError(
                    f"unparseable JSON at {path.name}:{line_no}: {exc.msg} -- "
                    f"line: {_excerpt(raw)} (a corrupt log is not evidence)"
                ) from exc
            if not isinstance(obj, dict):
                raise LogError(
                    f"non-object JSON at {path.name}:{line_no}: {_excerpt(raw)} "
                    "(a corrupt log is not evidence)"
                )
            detections.append(Detection(line_no=line_no, raw=raw, obj=obj))
    if not detections:
        raise LogError(
            f"{path.name} holds zero parseable detection lines -- "
            "nothing examined is not a pass"
        )
    return detections


def _walk(value: object) -> Iterator[dict]:
    """Yield every dict reachable inside value, depth-first."""
    if isinstance(value, dict):
        yield value
        for child in value.values():
            yield from _walk(child)
    elif isinstance(value, list):
        for child in value:
            yield from _walk(child)


def _numeric(value: object) -> Optional[float]:
    if isinstance(value, bool):
        return None
    if isinstance(value, (int, float)):
        return float(value)
    return None


def parse_evt_stream(path: Path) -> list[RelayedSample]:
    """Extract relayed range samples from the ``[EVT]`` stream.

    Liberal by design: any ``[EVT]`` event whose payload (walked recursively)
    holds a dict carrying an ``id`` in the ``v2x:`` namespace pairs that id
    with every numeric ``distance`` found beside it, stamped with the event's
    ``epoch_ms``. Lines without the ``[EVT]`` prefix are ignored.
    """
    samples: list[RelayedSample] = []
    with path.open("r", encoding="utf-8") as handle:
        for line_no, raw in enumerate(handle, start=1):
            stripped = raw.strip()
            if not stripped.startswith(_EVT_PREFIX):
                continue
            body = stripped[len(_EVT_PREFIX):].strip()
            try:
                event = json.loads(body)
            except json.JSONDecodeError as exc:
                raise LogError(
                    f"unparseable [EVT] JSON at {path.name}:{line_no}: {exc.msg} -- "
                    f"line: {_excerpt(raw)} (a corrupt log is not evidence)"
                ) from exc
            if not isinstance(event, dict):
                continue
            epoch_ms = _numeric(event.get("epoch_ms"))
            if epoch_ms is None:
                continue  # no shared clock value -> the sample cannot be time-matched
            payload = event.get("payload")
            for node in _walk(payload):
                track_id = node.get("id")
                if not (isinstance(track_id, str) and track_id.startswith(_V2X_NAMESPACE)):
                    continue
                distance = _numeric(node.get("distance"))
                if distance is None:
                    continue
                samples.append(
                    RelayedSample(
                        line_no=line_no,
                        raw=raw,
                        track_id=track_id,
                        epoch_ms=epoch_ms,
                        distance=distance,
                    )
                )
    return samples


def _measured_ms(detection: Detection) -> Optional[float]:
    timestamps = detection.obj.get("timestamps")
    if not isinstance(timestamps, dict):
        return None
    return _numeric(timestamps.get("measured"))


def check_rules_1_and_2(detections: Sequence[Detection]) -> list[Violation]:
    violations: list[Violation] = []
    for det in detections:
        if det.obj.get("source") == _RELAYED_SOURCE:
            violations.append(
                Violation(
                    rule=1,
                    line_no=det.line_no,
                    raw=det.raw,
                    detail=(
                        f"source={_RELAYED_SOURCE!r} in the detection log -- "
                        "the detector can only produce own_sensor"
                    ),
                )
            )
        track_id = det.obj.get("id")
        if isinstance(track_id, str) and track_id.startswith(_V2X_NAMESPACE):
            violations.append(
                Violation(
                    rule=2,
                    line_no=det.line_no,
                    raw=det.raw,
                    detail=(
                        f"id={track_id!r} is in the {_V2X_NAMESPACE!r} namespace -- "
                        "own-sensor tracks cannot mint relayed ids"
                    ),
                )
            )
    return violations


def check_rule_3(
    own_detections: Sequence[Detection],
    samples: Sequence[RelayedSample],
    radius_m: float,
    time_tolerance_ms: float,
) -> tuple[list[Violation], int]:
    """Compare own-sensor ranges against time-matched relayed samples.

    Returns (violations, comparisons) where comparisons counts the
    time-matched (own line, relayed sample) pairs actually examined —
    printed on a clean run so a vacuous pass is visible.
    """
    violations: list[Violation] = []
    comparisons = 0
    for det in own_detections:
        measured = _measured_ms(det)
        own_distance = _numeric(det.obj.get("distance"))
        if measured is None or own_distance is None:
            continue  # nothing comparable on this line
        for sample in samples:
            if abs(sample.epoch_ms - measured) > time_tolerance_ms:
                continue
            comparisons += 1
            if abs(own_distance - sample.distance) <= radius_m:
                violations.append(
                    Violation(
                        rule=3,
                        line_no=det.line_no,
                        raw=det.raw,
                        detail=(
                            f"own-sensor range {own_distance:g} m sits within "
                            f"{radius_m:g} m of relayed {sample.track_id} at "
                            f"{sample.distance:g} m (epoch_ms={sample.epoch_ms:g}, "
                            f"measured={measured:g}, tolerance "
                            f"{time_tolerance_ms:g} ms; EVT line {sample.line_no})"
                        ),
                    )
                )
                break  # one match already fails this line; move on
    return violations, comparisons


def main(argv: Optional[Sequence[str]] = None) -> int:
    args = build_parser().parse_args(argv)

    if not args.detection_log.is_file():
        print(f"error: no such file: {args.detection_log}", file=sys.stderr)
        return 2
    if args.evt is not None and not args.evt.is_file():
        print(f"error: no such file: {args.evt}", file=sys.stderr)
        return 2

    try:
        detections = parse_detection_log(args.detection_log)
        samples = parse_evt_stream(args.evt) if args.evt is not None else []
    except LogError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2

    own_detections = [d for d in detections if d.obj.get("source") == _OWN_SOURCE]

    violations = check_rules_1_and_2(detections)
    comparisons = 0
    if args.evt is not None:
        rule3_violations, comparisons = check_rule_3(
            own_detections, samples, args.radius_m, args.time_tolerance_ms
        )
        violations.extend(rule3_violations)

    # Deliberately no rule 4 ("detector saw nothing but B"): adjacent-lane and
    # oncoming vehicles are expected in correct footage — see
    # ADA_ECU/media/ego-b-occluding-c.source.md § What this obliges downstream.

    if violations:
        for v in sorted(violations, key=lambda v: (v.line_no, v.rule)):
            print(
                f"VIOLATION rule {v.rule} at {args.detection_log.name}:{v.line_no}: "
                f"{v.detail}\n  line: {_excerpt(v.raw)}"
            )
        print(f"{len(violations)} violation(s) -- the zero-C claim is falsified")
        return 1

    parts = [
        f"OK {args.detection_log.name}:",
        f"detection_lines={len(detections)}",
        f"own_sensor_lines={len(own_detections)}",
    ]
    if args.evt is not None:
        parts.append(f"relayed_evt_samples={len(samples)}")
        parts.append(f"time_matched_comparisons={comparisons}")
    else:
        parts.append("rule3=skipped (no --evt)")
    print(" ".join(parts))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
