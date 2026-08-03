#!/usr/bin/env python3
"""Validate the ADA Phase 3/4 event chain from a JSONL event log."""

from __future__ import annotations

import argparse
import json
import pathlib
import sys

R3_FIELDS = {
    "id",
    "class",
    "source",
    "position",
    "distance",
    "speed",
    "confidence",
    "state",
    "timestamps",
}


def load_events(path: pathlib.Path) -> list[dict]:
    events: list[dict] = []
    with path.open(encoding="utf-8") as source:
        for line_number, raw_line in enumerate(source, start=1):
            line = raw_line.strip()
            if line.startswith("[EVT] "):
                line = line[6:]
            elif line.startswith("[CAP]") or not line.startswith("{"):
                continue
            if not line:
                continue
            try:
                events.append(json.loads(line))
            except json.JSONDecodeError as exc:
                raise ValueError(f"line {line_number}: {exc}") from exc
    return events


def tracked_object(events: list[dict], source: str) -> dict | None:
    return next(
        (
            event.get("payload", {}).get("object")
            for event in events
            if event.get("event") == "track_transition"
            and event.get("payload", {}).get("source") == source
            and event.get("payload", {}).get("state") == "tracked"
            and R3_FIELDS <= set(event.get("payload", {}).get("object", {}))
        ),
        None,
    )


def validate_schema(body: dict, schema_path: pathlib.Path) -> None:
    import jsonschema

    schema = json.loads(schema_path.read_text(encoding="utf-8"))
    r3 = json.loads(
        (schema_path.parent / "r3-tracked-object.schema.json").read_text(
            encoding="utf-8"
        )
    )

    def inline_r3(value):
        if isinstance(value, dict):
            if value.get("$ref") == "r3-tracked-object.schema.json":
                replacement = dict(r3)
                replacement.pop("$id", None)
                return replacement
            return {key: inline_r3(item) for key, item in value.items()}
        if isinstance(value, list):
            return [inline_r3(item) for item in value]
        return value

    try:
        jsonschema.Draft202012Validator(inline_r3(schema)).validate(body)
    except jsonschema.ValidationError as exc:
        raise ValueError(str(exc)) from exc


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Check ADA EVT log contains the complete B/C-to-R4 chain."
    )
    parser.add_argument("event_log", type=pathlib.Path)
    parser.add_argument("--r4-schema", type=pathlib.Path)
    parser.add_argument("--allow-unsent", action="store_true")
    args = parser.parse_args()

    try:
        events = load_events(args.event_log)
    except (OSError, ValueError) as exc:
        print(f"event log invalid: {exc}", file=sys.stderr)
        return 2

    if not tracked_object(events, "own_sensor"):
        print("missing full tracked own_sensor vehicle B transition", file=sys.stderr)
        return 10
    if not tracked_object(events, "v2x_relayed"):
        print("missing full tracked v2x_relayed vehicle C transition", file=sys.stderr)
        return 11

    risk_indexes = [
        index
        for index, event in enumerate(events)
        if event.get("event") == "risk_transition"
    ]
    tx_indexes = [
        index for index, event in enumerate(events) if event.get("event") == "r4_tx"
    ]
    if not tx_indexes:
        print("missing r4_tx", file=sys.stderr)
        return 12
    if not risk_indexes or risk_indexes[0] > tx_indexes[0]:
        print("r4_tx has no preceding risk_transition", file=sys.stderr)
        return 13

    if len(risk_indexes) != len(tx_indexes):
        print("risk_transition/r4_tx counts differ", file=sys.stderr)
        return 17
    if not args.allow_unsent and any(
        not events[index].get("payload", {}).get("sent") for index in tx_indexes
    ):
        print("one or more r4_tx records were not sent", file=sys.stderr)
        return 18

    body = events[tx_indexes[0]].get("payload", {}).get("body", {})
    tracked_objects = body.get("trackedObjects", [])
    if not any(
        item.get("source") == "own_sensor" and item.get("state") == "tracked"
        for item in tracked_objects
    ):
        print("r4_tx body is missing full tracked vehicle B", file=sys.stderr)
        return 14
    if not any(
        item.get("source") == "v2x_relayed" and item.get("state") == "tracked"
        for item in tracked_objects
    ):
        print("r4_tx body is missing full tracked vehicle C", file=sys.stderr)
        return 15
    if body.get("geometry", {}).get("vehicleB") is None:
        print("r4_tx body has null/missing geometry.vehicleB", file=sys.stderr)
        return 16

    if args.r4_schema:
        try:
            for index in tx_indexes:
                validate_schema(events[index]["payload"]["body"], args.r4_schema)
        except (ImportError, KeyError, OSError, TypeError, ValueError) as exc:
            print(f"R4 schema validation failed: {exc}", file=sys.stderr)
            return 19

    print(
        "ADA EVT chain: pass "
        f"(events={len(events)}, riskTransitions={len(risk_indexes)}, r4Tx={len(tx_indexes)})"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
