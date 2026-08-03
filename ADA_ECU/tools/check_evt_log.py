#!/usr/bin/env python3
"""Validate the ADA Phase 3/4 event chain from a JSONL event log."""

from __future__ import annotations

import argparse
import json
import pathlib
import sys


def load_events(path: pathlib.Path) -> list[dict]:
    events: list[dict] = []
    with path.open(encoding="utf-8") as source:
        for line_number, raw_line in enumerate(source, start=1):
            line = raw_line.strip()
            if line.startswith("[EVT] "):
                line = line[6:]
            if not line:
                continue
            try:
                events.append(json.loads(line))
            except json.JSONDecodeError as exc:
                raise ValueError(f"line {line_number}: {exc}") from exc
    return events


def tracked_transition(events: list[dict], source: str) -> bool:
    return any(
        event.get("event") == "track_transition"
        and event.get("payload", {}).get("source") == source
        and event.get("payload", {}).get("state") == "tracked"
        for event in events
    )


def main() -> int:
    parser = argparse.ArgumentParser(description="Check ADA EVT log contains the complete B/C-to-R4 chain.")
    parser.add_argument("event_log", type=pathlib.Path)
    args = parser.parse_args()

    try:
        events = load_events(args.event_log)
    except (OSError, ValueError) as exc:
        print(f"event log invalid: {exc}", file=sys.stderr)
        return 2

    if not tracked_transition(events, "own_sensor"):
        print("missing tracked own_sensor vehicle B transition", file=sys.stderr)
        return 10
    if not tracked_transition(events, "v2x_relayed"):
        print("missing tracked v2x_relayed vehicle C transition", file=sys.stderr)
        return 11

    risk_indexes = [index for index, event in enumerate(events) if event.get("event") == "risk_transition"]
    tx_indexes = [index for index, event in enumerate(events) if event.get("event") == "r4_tx"]
    if not tx_indexes:
        print("missing r4_tx", file=sys.stderr)
        return 12
    if not risk_indexes or risk_indexes[0] > tx_indexes[0]:
        print("r4_tx has no preceding risk_transition", file=sys.stderr)
        return 13

    body = events[tx_indexes[0]].get("payload", {}).get("body", {})
    tracked_objects = body.get("trackedObjects", [])
    if not any(item.get("source") == "own_sensor" and item.get("state") == "tracked" for item in tracked_objects):
        print("r4_tx body is missing full tracked vehicle B", file=sys.stderr)
        return 14
    if not any(item.get("source") == "v2x_relayed" and item.get("state") == "tracked" for item in tracked_objects):
        print("r4_tx body is missing full tracked vehicle C", file=sys.stderr)
        return 15
    if body.get("geometry", {}).get("vehicleB") is None:
        print("r4_tx body has null/missing geometry.vehicleB", file=sys.stderr)
        return 16

    print(
        "ADA EVT chain: pass "
        f"(events={len(events)}, riskTransitions={len(risk_indexes)}, r4Tx={len(tx_indexes)})"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
