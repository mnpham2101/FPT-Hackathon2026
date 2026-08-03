#!/usr/bin/env python3
"""Render track, risk, and R4 events from an ADA JSONL event log."""

from __future__ import annotations

import argparse
import json
import pathlib


def main() -> int:
    parser = argparse.ArgumentParser(description="Render ADA collision-risk event list.")
    parser.add_argument("event_log", type=pathlib.Path)
    args = parser.parse_args()

    print("timestamp_ms\tevent\tobject\tstate\tdistance\tr4_bytes")
    with args.event_log.open(encoding="utf-8") as source:
        for raw_line in source:
            line = raw_line.strip()
            if line.startswith("[EVT] "):
                line = line[6:]
            if not line:
                continue
            event = json.loads(line)
            if event.get("event") not in {"track_transition", "risk_transition", "r4_tx"}:
                continue
            payload = event.get("payload", {})
            print(
                f"{event.get('ts', '')}\t{event.get('event', '')}\t"
                f"{payload.get('id', payload.get('trackId', ''))}\t"
                f"{payload.get('state', payload.get('riskState', ''))}\t"
                f"{payload.get('distance', '')}\t{payload.get('length', '')}"
            )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
