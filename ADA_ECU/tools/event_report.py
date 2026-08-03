#!/usr/bin/env python3
"""Render track, risk, and R4 events from an ADA JSONL event log."""

from __future__ import annotations

import argparse
import json
import pathlib


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Render ADA collision-risk event list."
    )
    parser.add_argument("event_log", type=pathlib.Path)
    parser.add_argument("--summary", action="store_true")
    args = parser.parse_args()

    print(
        "timestamp_ms\tevent\tobject\tsource\tstate\tdistance_ac\tttc\tsent\tr4_bytes"
    )
    assessments: dict[str, dict] = {}
    counts = {"tracked": 0, "low": 0, "medium": 0, "high": 0, "r4": 0, "sent": 0}
    with args.event_log.open(encoding="utf-8") as source:
        for raw_line in source:
            line = raw_line.strip()
            if line.startswith("[EVT] "):
                line = line[6:]
            elif line.startswith("[CAP]") or not line.startswith("{"):
                continue
            if not line:
                continue
            event = json.loads(line)
            event_name = event.get("event")
            payload = event.get("payload", {})
            if event_name == "assessment":
                assessments[payload.get("trackId", "")] = payload
                continue
            if event_name not in {"track_transition", "risk_transition", "r4_tx"}:
                continue
            track_id = payload.get("id", payload.get("trackId", ""))
            assessment = assessments.get(track_id, {})
            state = payload.get("state", payload.get("riskState", ""))
            if (
                event_name == "track_transition"
                and state == "tracked"
                and payload.get("changed", True)
            ):
                counts["tracked"] += 1
            if event_name == "risk_transition" and state in {"low", "medium", "high"}:
                counts[state] += 1
            if event_name == "r4_tx":
                counts["r4"] += 1
                counts["sent"] += int(bool(payload.get("sent")))
            print(
                f"{event.get('ts', '')}\t{event_name}\t{track_id}\t{payload.get('source', '')}\t"
                f"{state}\t{assessment.get('distanceAC', '')}\t{assessment.get('ttc', '')}\t"
                f"{payload.get('sent', '')}\t{payload.get('length', '')}"
            )
    if args.summary:
        print(
            "summary\t"
            f"tracked={counts['tracked']} low={counts['low']} medium={counts['medium']} "
            f"high={counts['high']} r4={counts['r4']} sent={counts['sent']}"
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
