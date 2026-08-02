#!/usr/bin/env python3
"""Validate that an R3 detector log contains no relayed/vehicle-C claims."""

from __future__ import annotations

import argparse
import json
import math
import pathlib
import sys
from dataclasses import dataclass


@dataclass(frozen=True)
class TimedPosition:
    timestamp_ms: int
    x: float
    y: float


def load_c_positions(path: pathlib.Path | None) -> dict[int, TimedPosition]:
    if path is None:
        return {}
    positions: dict[int, TimedPosition] = {}
    with path.open(encoding="utf-8") as source:
        for line_number, raw_line in enumerate(source, start=1):
            if not raw_line.strip():
                continue
            body = json.loads(raw_line)
            timestamp_ms = int(body["timestampMs"])
            positions[timestamp_ms] = TimedPosition(timestamp_ms, float(body["x"]), float(body["y"]))
    return positions


def main() -> int:
    parser = argparse.ArgumentParser(description="Fail if own-sensor R3 evidence claims vehicle C or relayed data.")
    parser.add_argument("r3_log", type=pathlib.Path)
    parser.add_argument("--vehicle-c-log", type=pathlib.Path, help="optional JSONL rows: timestampMs, x, y in ego coordinates")
    parser.add_argument("--radius-m", type=float, default=5.0)
    args = parser.parse_args()

    if args.radius_m <= 0:
        print("--radius-m must be > 0", file=sys.stderr)
        return 2

    try:
        c_positions = load_c_positions(args.vehicle_c_log)
    except (OSError, ValueError, KeyError, json.JSONDecodeError) as exc:
        print(f"cannot load vehicle-C positions: {exc}", file=sys.stderr)
        return 2

    examined = 0
    spatial_checks = 0
    try:
        with args.r3_log.open(encoding="utf-8") as source:
            for line_number, raw_line in enumerate(source, start=1):
                if not raw_line.strip():
                    continue
                body = json.loads(raw_line)
                examined += 1
                if body.get("source") != "own_sensor":
                    print(f"rule 1 failed at line {line_number}: detector source is not own_sensor", file=sys.stderr)
                    return 1
                if str(body.get("id", "")).startswith("v2x:"):
                    print(f"rule 2 failed at line {line_number}: detector minted a v2x id", file=sys.stderr)
                    return 1

                measured = int(body.get("timestamps", {}).get("measured", -1))
                vehicle_c = c_positions.get(measured)
                if vehicle_c is not None:
                    position = body.get("position", {})
                    separation = math.hypot(float(position["x"]) - vehicle_c.x, float(position["y"]) - vehicle_c.y)
                    spatial_checks += 1
                    if separation <= args.radius_m:
                        print(
                            f"rule 3 failed at line {line_number}: own_sensor object is {separation:.3f} m from vehicle C",
                            file=sys.stderr,
                        )
                        return 1
    except (OSError, ValueError, KeyError, TypeError, json.JSONDecodeError) as exc:
        print(f"invalid R3 evidence: {exc}", file=sys.stderr)
        return 2

    if examined == 0:
        print("zero-C check failed: no R3 objects examined", file=sys.stderr)
        return 2

    print(f"zero-C check: pass (examined={examined}, spatialChecks={spatial_checks})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
