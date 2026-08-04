#!/usr/bin/env python3
"""ADA bench emitter - the ``ROLE=v2x_mock`` role of the ``m1-ada-bench`` image.

Stands in for the V2X ECU at its output edge only: one R2 relayed-object JSON
datagram per tick to ``TARGET_HOST:TARGET_PORT`` at ``RATE_HZ``, after
``START_DELAY_S`` seconds so the ADA node is listening first. No decoding, no
encoded frames, Python 3 standard library only (the image is Alpine plus
``python3`` and ``tcpdump``; no pip).

Every topology and cadence value is an environment variable, none is a literal:

- ``TARGET_HOST`` / ``TARGET_PORT`` - the ADA node; **required, no default**.
- ``PROFILE`` (``approaching`` | ``out_of_range``), ``RATE_HZ``,
  ``START_DELAY_S``, ``STATION_ID``, ``OBJECT_ID``, ``START_DISTANCE_M``,
  ``MIN_DISTANCE_M``, ``CLOSING_RATE_MPS``, ``LATERAL_M``, ``OBJECT_SPEED_MPS``
  - defaults match the bench node's env block in the isolated blueprint
  (``requirements/car-sky-guide/blueprint-ada-isolated.json``).

Profiles:

- ``approaching``: ``object.distance`` and ``object.position.x`` walk from
  ``START_DISTANCE_M`` down to ``MIN_DISTANCE_M`` at ``CLOSING_RATE_MPS``,
  then hold; ``object.position.y`` is ``LATERAL_M``.
- ``out_of_range``: the distance holds at ``START_DISTANCE_M`` - beyond the
  drop gate, so no track is ever admitted.

Any other ``PROFILE`` value fails loudly with a non-zero exit.

Why the defaults keep C occluded: the emitted distances are **B->C ranges**
(vehicle C as seen by vehicle B, ``stationId`` = B, ``objectId`` = C). The ADA
node composes ego->C as ``d_AB + d_BC``, so C sits beyond B by exactly the
emitted B->C range at every instant - at least ``MIN_DISTANCE_M`` (5 m) by
construction, whatever B's own range does. The clip's B walks ~60 m down to
~10 m (``ADA_ECU/media/ego-b-occluding-c.source.md`` section "What this
obliges downstream"); because the composition adds the two ranges, C never
crosses in front of B and stays genuinely non-line-of-sight for the whole run.
The blueprint's ``START_DISTANCE_M=45`` / ``MIN_DISTANCE_M=5`` /
``CLOSING_RATE_MPS=5`` therefore give a C that closes on B without ever
reaching it.

The message body matches ``ADA_ECU/contracts/r2-v2x-object.schema.json`` field
for field. ``object.confidence`` and ``sender.speed`` are populated so no
nullable field is exercised by accident; units are SI and ``classification``
is ``vehicle``. The schema's description derives ``distance`` as
``hypot(position.x, position.y)``; this emitter follows the walkthrough's
section 2.3 literally instead (``distance`` and ``position.x`` share the same
walk value), a divergence of at most ``hypot(d, LATERAL_M) - d`` (~0.14 m at
the 5 m minimum) that the schema does not constrain.

One log line per datagram, in the exact shape deployed checks grep:

    [TX] seq=42 objectId=7 distance=30.5 bytes=318 -> 10.99.0.12:47200
"""

from __future__ import annotations

import itertools
import json
import os
import socket
import sys
import time

# Message-content constants of the frozen-sample family
# (ADA_ECU/tests/fixtures/samples/r2-object.json) - content of the mock's
# message, not tunables of any node.
SENDER_LAT = 21.028511
SENDER_LON = 105.804817
SENDER_HEADING_DEG = 90.0
SENDER_SPEED_MPS = 16.7
TIME_OF_MEASUREMENT_MS = -50
POSITION_CONFIDENCE_M = 0.9
OBJECT_CONFIDENCE = 0.95
CLASSIFICATION = "vehicle"

PROFILES = ("approaching", "out_of_range")

# Defaults matching the isolated blueprint's bench node env block.
ENV_DEFAULTS = {
    "PROFILE": "approaching",
    "RATE_HZ": "10",
    "START_DELAY_S": "20",
    "STATION_ID": "1201",
    "OBJECT_ID": "7",
    "START_DISTANCE_M": "45",
    "MIN_DISTANCE_M": "5",
    "CLOSING_RATE_MPS": "5",
    "LATERAL_M": "1.2",
    "OBJECT_SPEED_MPS": "15.2",
}


def fail(message: str):
    print(f"[ERROR] mock_v2x: {message}", flush=True)
    sys.exit(2)


def env_required(name: str) -> str:
    value = os.environ.get(name)
    if value is None or value == "":
        fail(f"required environment variable {name} is unset (no default; "
             "the blueprint node config must supply it)")
    return value


def env_default(name: str) -> str:
    return os.environ.get(name) or ENV_DEFAULTS[name]


def env_float(name: str, required: bool = False) -> float:
    raw = env_required(name) if required else env_default(name)
    try:
        return float(raw)
    except ValueError:
        fail(f"environment variable {name}={raw!r} is not a number")


def env_int(name: str, required: bool = False) -> int:
    raw = env_required(name) if required else env_default(name)
    try:
        return int(raw)
    except ValueError:
        fail(f"environment variable {name}={raw!r} is not an integer")


def distance_at(seq: int, rate_hz: float, profile: str,
                start_m: float, min_m: float, closing_mps: float) -> float:
    """The B->C range at tick ``seq`` of the profile."""
    if profile == "out_of_range":
        return start_m
    # approaching: walk down at CLOSING_RATE_MPS, then hold at MIN_DISTANCE_M.
    return max(start_m - closing_mps * (seq / rate_hz), min_m)


def build_body(seq: int, rate_hz: float, profile: str, station_id: int,
               object_id: int, start_m: float, min_m: float,
               closing_mps: float, lateral_m: float,
               object_speed_mps: float) -> dict:
    """One R2 body, matching r2-v2x-object.schema.json field for field."""
    d = round(distance_at(seq, rate_hz, profile, start_m, min_m, closing_mps), 3)
    return {
        "schemaVersion": 1,
        "type": "v2x_object",
        "stationId": station_id,
        "rxTime": int(time.time() * 1000),
        "sender": {
            "lat": SENDER_LAT,
            "lon": SENDER_LON,
            "heading": SENDER_HEADING_DEG,
            "speed": SENDER_SPEED_MPS,
        },
        "object": {
            "objectId": object_id,
            "timeOfMeasurement": TIME_OF_MEASUREMENT_MS,
            "distance": d,
            "position": {
                "x": d,
                "y": lateral_m,
                "confidence": POSITION_CONFIDENCE_M,
            },
            "speed": object_speed_mps,
            "classification": CLASSIFICATION,
            "confidence": OBJECT_CONFIDENCE,
        },
    }


def encode(body: dict) -> bytes:
    """Compact JSON, the real producer's wire shape."""
    return json.dumps(body, separators=(",", ":")).encode("utf-8")


def main() -> int:
    target_host = env_required("TARGET_HOST")
    target_port = env_int("TARGET_PORT", required=True)
    profile = env_default("PROFILE")
    if profile not in PROFILES:
        fail(f"unknown PROFILE {profile!r}; expected one of {PROFILES}")
    rate_hz = env_float("RATE_HZ")
    if rate_hz <= 0:
        fail(f"RATE_HZ must be > 0, got {rate_hz}")
    start_delay_s = env_float("START_DELAY_S")
    station_id = env_int("STATION_ID")
    object_id = env_int("OBJECT_ID")
    start_m = env_float("START_DISTANCE_M")
    min_m = env_float("MIN_DISTANCE_M")
    closing_mps = env_float("CLOSING_RATE_MPS")
    lateral_m = env_float("LATERAL_M")
    object_speed_mps = env_float("OBJECT_SPEED_MPS")

    print(f"[BOOT] mock_v2x profile={profile} rate_hz={rate_hz} "
          f"start_delay_s={start_delay_s} -> {target_host}:{target_port}",
          flush=True)
    if start_delay_s > 0:
        time.sleep(start_delay_s)

    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as tx:
        start = time.monotonic()
        for seq in itertools.count():
            body = build_body(seq, rate_hz, profile, station_id, object_id,
                              start_m, min_m, closing_mps, lateral_m,
                              object_speed_mps)
            payload = encode(body)
            tx.sendto(payload, (target_host, target_port))
            print(f"[TX] seq={seq} objectId={object_id} "
                  f"distance={body['object']['distance']} bytes={len(payload)} "
                  f"-> {target_host}:{target_port}", flush=True)
            # Deadline-scheduled ticks: each tick fires at start + n/RATE_HZ
            # on the monotonic clock, so sleep error does not accumulate.
            deadline = start + (seq + 1) / rate_hz
            delay = deadline - time.monotonic()
            if delay > 0:
                time.sleep(delay)
    return 0


if __name__ == "__main__":
    sys.exit(main())
