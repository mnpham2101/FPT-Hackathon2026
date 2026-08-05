#!/usr/bin/env python3
"""Mock V2X sender - Phase 2 test equipment, never part of the image.

Sends R2 JSON datagrams (compact JSON, one per perceived-object update) to
``host:port``, standing in for the Phase 1 producer ``V2X_ECU/src/forward/ada_forwarder``.
Python 3 stdlib only.

Profiles mirror the bench's two committed scenarios; the geometry is READ from
the scenario YAMLs via a minimal line parser, never re-derived here:

- ``approaching``    <- ``Scenario_Player/scenarios/default.yaml``
                        (distance ramp 70.0 m -> 20.5 m at 5.0 m/s)
- ``out-of-range``   <- ``Scenario_Player/scenarios/c-out-of-range.yaml``
                        (static 60.0 m)

The target peer comes from ``--host``/``--port`` or the ``ADA_ECU_HOST``/
``ADA_ECU_PORT`` env vars - there is no hardcoded peer.

Modes:

- default:        send ``--count`` datagrams at ``--rate`` Hz; one stdout line
                  per datagram for correlation.
- ``--validate``: check every line of ``tests/fixtures/own_sensor_mock.jsonl``
                  against the synced R3 schema, and every generated R2 body of
                  both profiles against the synced R2 schema. Schemas are loaded
                  from ``ADA_ECU/contracts/`` - field lists are never restated.
- ``--loopback``: send every datagram of the selected profile to a socket bound
                  on 127.0.0.1 and verify each is received byte-identical.
"""

from __future__ import annotations

import argparse
import json
import math
import os
import socket
import sys
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Dict, List, Optional

TOOLS_DIR = Path(__file__).resolve().parent
ADA_DIR = TOOLS_DIR.parent
REPO_ROOT = ADA_DIR.parent
CONTRACTS_DIR = ADA_DIR / "contracts"
R2_SCHEMA_PATH = CONTRACTS_DIR / "r2-v2x-object.schema.json"
R3_SCHEMA_PATH = CONTRACTS_DIR / "r3-tracked-object.schema.json"
FIXTURE_PATH = ADA_DIR / "tests" / "fixtures" / "own_sensor_mock.jsonl"
DEFAULT_SCENARIO_DIR = REPO_ROOT / "Scenario_Player" / "scenarios"

# profile name -> scenario file (the bench's two committed scenarios)
PROFILE_FILES = (("approaching", "default.yaml"), ("out-of-range", "c-out-of-range.yaml"))

# Frozen-sample family constants (ADA_ECU/tests/fixtures/samples/r2-object.json):
# message content of the mock, not tunables of any node.
TIME_OF_MEASUREMENT_MS = -50
POSITION_CONFIDENCE_M = 0.9
ENV_HOST = "ADA_ECU_HOST"
ENV_PORT = "ADA_ECU_PORT"
LOOPBACK_TIMEOUT_S = 2.0


def classification_name(code: int) -> str:
    """Wire-native class code (scenario YAML) -> R2 classification string."""
    if code == 5:
        return "vehicle"
    raise ValueError(f"unmapped classification code {code}")


@dataclass(frozen=True)
class ScenarioParams:
    """The scenario values the R2 stream is built from, as read from the YAML."""

    name: str
    cpm_rate_hz: float
    duration_s: float
    station_id: int
    lat: float
    lon: float
    heading_deg: float
    object_id: int
    initial_distance_m: float
    closing_speed_mps: float
    lateral_offset_m: float
    classification_code: int
    confidence_percent: float

    @property
    def default_count(self) -> int:
        return max(1, round(self.duration_s * self.cpm_rate_hz))


def _parse_yaml_lines(path: Path) -> Dict[str, Any]:
    """Minimal line parser for the flat two-level scenario YAMLs (no library)."""
    top: Dict[str, Any] = {}
    section: Optional[str] = None
    for raw in path.read_text(encoding="utf-8").splitlines():
        line = raw.split("#", 1)[0].rstrip()
        if not line.strip():
            continue
        indented = line[0] in (" ", "\t")
        key, sep, value = line.strip().partition(":")
        if not sep:
            continue
        key, value = key.strip(), value.strip()
        if not indented:
            if value:
                top[key] = value
                section = None
            else:
                section = key
                top[key] = {}
        elif section is not None:
            top[section][key] = value
    return top


def load_scenario(path: Path) -> ScenarioParams:
    data = _parse_yaml_lines(path)
    sender, obj = data["sender"], data["object"]
    return ScenarioParams(
        name=str(data["name"]),
        cpm_rate_hz=float(data["cpm_rate_hz"]),
        duration_s=float(data["duration_s"]),
        station_id=int(sender["station_id"]),
        lat=float(sender["lat"]),
        lon=float(sender["lon"]),
        heading_deg=float(sender["heading_deg"]),
        object_id=int(obj["object_id"]),
        initial_distance_m=float(obj["initial_distance_m"]),
        closing_speed_mps=float(obj["closing_speed_mps"]),
        lateral_offset_m=float(obj["lateral_offset_m"]),
        classification_code=int(obj["classification"]),
        confidence_percent=float(obj["confidence"]),
    )


def scenario_path(profile: str, scenario_dir: Path) -> Path:
    for name, filename in PROFILE_FILES:
        if name == profile:
            return scenario_dir / filename
    raise ValueError(f"unknown profile {profile!r}")


def build_body(params: ScenarioParams, seq: int, rate_hz: float) -> Dict[str, Any]:
    """One R2 body at step ``seq`` of the profile's distance ramp."""
    t = seq / rate_hz
    y = params.lateral_offset_m
    # Ramp, clamped so distance stays >= lateral offset (schema minimum, sqrt domain).
    d = max(params.initial_distance_m - params.closing_speed_mps * t, y)
    x = math.sqrt(max(d * d - y * y, 0.0))
    speed: Optional[float] = None if seq == 0 else 0.0  # F1: null until derived; B is static
    return {
        "schemaVersion": 1,
        "type": "v2x_object",
        "stationId": params.station_id,
        "rxTime": int(time.time() * 1000),
        "sender": {
            "lat": params.lat,
            "lon": params.lon,
            "heading": params.heading_deg,
            "speed": speed,
        },
        "object": {
            "objectId": params.object_id,
            "timeOfMeasurement": TIME_OF_MEASUREMENT_MS,
            "distance": round(d, 3),
            "position": {
                "x": round(x, 3),
                "y": y,
                "confidence": POSITION_CONFIDENCE_M,
            },
            "speed": params.closing_speed_mps,
            "classification": classification_name(params.classification_code),
            "confidence": params.confidence_percent / 100.0,
        },
    }


def encode(body: Dict[str, Any]) -> bytes:
    """Compact JSON, the Phase 1 producer's wire shape."""
    return json.dumps(body, separators=(",", ":")).encode("utf-8")


# --- minimal JSON-schema subset validator ------------------------------------
# Covers exactly what the synced R2/R3 schemas use: type (incl. type lists),
# const, enum, minimum, maximum, required, properties. Nothing more.

def _type_ok(value: Any, type_name: str) -> bool:
    if type_name == "object":
        return isinstance(value, dict)
    if type_name == "string":
        return isinstance(value, str)
    if type_name == "integer":
        return isinstance(value, int) and not isinstance(value, bool)
    if type_name == "number":
        return isinstance(value, (int, float)) and not isinstance(value, bool)
    if type_name == "null":
        return value is None
    if type_name == "boolean":
        return isinstance(value, bool)
    if type_name == "array":
        return isinstance(value, list)
    return False


def validate_subset(value: Any, schema: Dict[str, Any], path: str = "$") -> List[str]:
    errors: List[str] = []
    if "const" in schema and value != schema["const"]:
        errors.append(f"{path}: expected const {schema['const']!r}, got {value!r}")
    if "enum" in schema and value not in schema["enum"]:
        errors.append(f"{path}: {value!r} not in enum {schema['enum']}")
    if "type" in schema:
        types = schema["type"] if isinstance(schema["type"], list) else [schema["type"]]
        if not any(_type_ok(value, t) for t in types):
            errors.append(f"{path}: type {type(value).__name__} not in {types}")
    if isinstance(value, (int, float)) and not isinstance(value, bool):
        if "minimum" in schema and value < schema["minimum"]:
            errors.append(f"{path}: {value} < minimum {schema['minimum']}")
        if "maximum" in schema and value > schema["maximum"]:
            errors.append(f"{path}: {value} > maximum {schema['maximum']}")
    if isinstance(value, dict):
        for req in schema.get("required", []):
            if req not in value:
                errors.append(f"{path}: missing required property {req!r}")
        for name, sub in schema.get("properties", {}).items():
            if name in value:
                errors.extend(validate_subset(value[name], sub, f"{path}.{name}"))
    return errors


def load_schema(path: Path) -> Dict[str, Any]:
    return json.loads(path.read_text(encoding="utf-8"))


# --- modes --------------------------------------------------------------------

def run_validate(scenario_dir: Path) -> int:
    failures = 0

    r3_schema = load_schema(R3_SCHEMA_PATH)
    lines = FIXTURE_PATH.read_text(encoding="utf-8").splitlines()
    bad = 0
    for i, line in enumerate(lines, start=1):
        errors = validate_subset(json.loads(line), r3_schema)
        for err in errors:
            print(f"[VAL] fixture line {i}: {err}")
        bad += 1 if errors else 0
    print(f"[VAL] fixture {FIXTURE_PATH.name}: {len(lines)} lines, "
          f"{len(lines) - bad} valid, {bad} invalid (schema {R3_SCHEMA_PATH.name})")
    failures += bad

    r2_schema = load_schema(R2_SCHEMA_PATH)
    for profile, _ in PROFILE_FILES:
        params = load_scenario(scenario_path(profile, scenario_dir))
        count = params.default_count
        bad = 0
        for seq in range(count):
            errors = validate_subset(build_body(params, seq, params.cpm_rate_hz), r2_schema)
            for err in errors:
                print(f"[VAL] {profile} seq={seq}: {err}")
            bad += 1 if errors else 0
        print(f"[VAL] profile {profile}: {count} bodies, {count - bad} valid, "
              f"{bad} invalid (schema {R2_SCHEMA_PATH.name})")
        failures += bad

    print(f"[VAL] result: {'PASS' if failures == 0 else f'FAIL ({failures} invalid)'}")
    return 0 if failures == 0 else 1


def run_loopback(params: ScenarioParams, rate_hz: float, count: int) -> int:
    mismatches = 0
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as rx, \
            socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as tx:
        rx.bind(("127.0.0.1", 0))
        rx.settimeout(LOOPBACK_TIMEOUT_S)
        host, port = rx.getsockname()
        for seq in range(count):
            payload = encode(build_body(params, seq, rate_hz))
            tx.sendto(payload, (host, port))
            try:
                received, _ = rx.recvfrom(65535)
            except socket.timeout:
                received = b""
            ok = received == payload
            mismatches += 0 if ok else 1
            print(f"[LOOP] seq={seq} bytes={len(payload)} "
                  f"{'byte-identical' if ok else 'MISMATCH'}")
    print(f"[LOOP] result: {count} datagrams on 127.0.0.1:{port}, "
          f"{count - mismatches} byte-identical, {mismatches} mismatched -> "
          f"{'PASS' if mismatches == 0 else 'FAIL'}")
    return 0 if mismatches == 0 else 1


def run_send(params: ScenarioParams, host: str, port: int, rate_hz: float, count: int) -> int:
    interval = 1.0 / rate_hz
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as tx:
        start = time.monotonic()
        for seq in range(count):
            body = build_body(params, seq, rate_hz)
            payload = encode(body)
            tx.sendto(payload, (host, port))
            print(f"[TX] seq={seq} profile={params.name} "
                  f"distance={body['object']['distance']} bytes={len(payload)} "
                  f"-> {host}:{port}")
            deadline = start + (seq + 1) * interval
            delay = deadline - time.monotonic()
            if delay > 0 and seq + 1 < count:
                time.sleep(delay)
    return 0


def main(argv: Optional[List[str]] = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--profile", choices=[name for name, _ in PROFILE_FILES],
                        default="approaching",
                        help="which committed bench scenario to mirror")
    parser.add_argument("--host", default=os.environ.get(ENV_HOST),
                        help=f"target host (default: env {ENV_HOST})")
    parser.add_argument("--port", type=int,
                        default=int(os.environ[ENV_PORT]) if os.environ.get(ENV_PORT) else None,
                        help=f"target UDP port (default: env {ENV_PORT})")
    parser.add_argument("--rate", type=float, default=None,
                        help="datagrams per second (default: scenario cpm_rate_hz)")
    parser.add_argument("--count", type=int, default=None,
                        help="datagrams to send (default: scenario duration_s x rate)")
    parser.add_argument("--scenario-dir", type=Path, default=DEFAULT_SCENARIO_DIR,
                        help="folder holding the bench scenario YAMLs")
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--validate", action="store_true",
                      help="validate the R3 fixture and every generated R2 body, then exit")
    mode.add_argument("--loopback", action="store_true",
                      help="send/receive every datagram byte-identical on 127.0.0.1, then exit")
    args = parser.parse_args(argv)

    if args.validate:
        return run_validate(args.scenario_dir)

    params = load_scenario(scenario_path(args.profile, args.scenario_dir))
    rate_hz = args.rate if args.rate is not None else params.cpm_rate_hz
    if rate_hz <= 0:
        parser.error("--rate must be > 0")
    count = args.count if args.count is not None else params.default_count
    if count <= 0:
        parser.error("--count must be > 0")

    if args.loopback:
        return run_loopback(params, rate_hz, count)

    if args.host is None or args.port is None:
        parser.error(f"target peer required: --host/--port or env {ENV_HOST}/{ENV_PORT} "
                     "(no hardcoded peer)")
    return run_send(params, args.host, args.port, rate_hz, count)


if __name__ == "__main__":
    sys.exit(main())
