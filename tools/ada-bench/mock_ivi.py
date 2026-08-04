#!/usr/bin/env python3
"""ADA bench sink - the ``ROLE=ivi_mock`` role of the ``m1-ada-bench`` image.

Stands in for the IVI ECU: binds ``0.0.0.0:LISTEN_PORT``, parses every UDP
datagram as JSON and checks it against the shape of
``ADA_ECU/contracts/r4-ada-ivi.schema.json`` with **explicit field checks, not
schema validation** - the image stays Python standard library only (Alpine
plus ``python3`` and ``tcpdump``; no pip, no jsonschema).

Environment:

- ``LISTEN_PORT`` - default ``47300``, the value the isolated blueprint's sink
  node env block supplies (``requirements/car-sky-guide/
  blueprint-ada-isolated.json``); the default keeps a bare run in parity with
  a deployed one.
- ``SUMMARY_EVERY_S`` - default ``10``, seconds between ``[SUMMARY]`` lines.

Output contract - deployed checks grep these strings literally, so the
prefixes ``[RX]`` / ``[CHECK]`` / ``[SUMMARY]``, the tokens
``both_vehicles=yes`` / ``c_source_relayed=yes`` and every field name are
load-bearing. Two lines per accepted datagram, one summary per period:

    [RX] seq=3 from=10.99.0.12:51044 bytes=486 type=warning warningType=nlos_obstruction risk=medium cSource=v2x_relayed cPos=(41.2,1.7) bPos=(11.0,0.4)
    [CHECK] seq=3 both_vehicles=yes c_source_relayed=yes
    [SUMMARY] received=31 warnings=31 both_vehicles=31 rejected=0

Semantics:

- ``seq`` is this sink's own receive counter, starting at 1 for the first
  datagram (warning events carry no sequence number of their own).
- ``both_vehicles=yes`` requires ``geometry.vehicleB`` **and**
  ``geometry.vehicleC`` both present with numeric ``x`` and ``y`` (for a
  ``state`` message the container is ``vehicles``, the schema's name for it).
- ``c_source_relayed=yes`` requires ``object.source`` to be exactly
  ``v2x_relayed``.
- ``rejected`` counts datagrams that were not valid JSON, or whose ``type``
  was neither ``warning`` nor ``state``. A rejected datagram prints one
  ``[REJECT] seq=... reason=...`` line instead of the two lines above and
  never kills the sink. A **null ``geometry.vehicleC`` is legitimate** (before
  C is first tracked) and yields ``both_vehicles=no``, not a rejection.
- ``cPos``/``bPos`` print the vehicle's ``(x,y)`` to one decimal;  when the
  vehicle is null or missing the placeholder ``(-)`` is printed, e.g.
  ``cPos=(-)`` - the ``[CHECK]`` and ``[SUMMARY]`` shapes are unaffected.
- Missing ``warningType``/``riskState``/``object.source`` fields, and every
  such field of a ``state`` message, print as ``-``.
"""

from __future__ import annotations

import json
import os
import socket
import sys
import time

DEFAULT_LISTEN_PORT = 47300
DEFAULT_SUMMARY_EVERY_S = 10.0
ACCEPTED_TYPES = ("warning", "state")
RECV_BUFFER = 65535


def fail(message: str):
    print(f"[ERROR] mock_ivi: {message}", flush=True)
    sys.exit(2)


def env_number(name: str, default: float, cast) -> float:
    raw = os.environ.get(name)
    if raw is None or raw == "":
        return default
    try:
        return cast(raw)
    except ValueError:
        fail(f"environment variable {name}={raw!r} is not a number")


def is_number(value) -> bool:
    return isinstance(value, (int, float)) and not isinstance(value, bool)


def vehicle_ok(vehicle) -> bool:
    """Present with numeric x and y - the both_vehicles requirement per side."""
    return (isinstance(vehicle, dict)
            and is_number(vehicle.get("x")) and is_number(vehicle.get("y")))


def fmt_pos(vehicle) -> str:
    """(x,y) to one decimal; the (-) placeholder for a null/missing vehicle."""
    if vehicle_ok(vehicle):
        return f"({vehicle['x']:.1f},{vehicle['y']:.1f})"
    return "(-)"


def fmt_field(value) -> str:
    return str(value) if isinstance(value, str) and value else "-"


def main() -> int:
    listen_port = int(env_number("LISTEN_PORT", DEFAULT_LISTEN_PORT, int))
    summary_every_s = env_number("SUMMARY_EVERY_S", DEFAULT_SUMMARY_EVERY_S, float)
    if summary_every_s <= 0:
        fail(f"SUMMARY_EVERY_S must be > 0, got {summary_every_s}")

    received = warnings = both_vehicles_count = rejected = 0

    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as rx:
        rx.bind(("0.0.0.0", listen_port))
        print(f"[BOOT] mock_ivi listening on 0.0.0.0:{listen_port} "
              f"summary_every_s={summary_every_s}", flush=True)
        start = time.monotonic()
        summaries = 0
        while True:
            # Deadline-scheduled summaries: the n-th fires at start + n * period.
            deadline = start + (summaries + 1) * summary_every_s
            timeout = deadline - time.monotonic()
            if timeout <= 0:
                print(f"[SUMMARY] received={received} warnings={warnings} "
                      f"both_vehicles={both_vehicles_count} rejected={rejected}",
                      flush=True)
                summaries += 1
                continue
            rx.settimeout(timeout)
            try:
                payload, (src_host, src_port) = rx.recvfrom(RECV_BUFFER)
            except socket.timeout:
                continue

            received += 1
            seq = received
            source = f"{src_host}:{src_port}"
            try:
                msg = json.loads(payload.decode("utf-8"))
            except (UnicodeDecodeError, json.JSONDecodeError):
                rejected += 1
                print(f"[REJECT] seq={seq} from={source} bytes={len(payload)} "
                      f"reason=not_json", flush=True)
                continue
            msg_type = msg.get("type") if isinstance(msg, dict) else None
            if msg_type not in ACCEPTED_TYPES:
                rejected += 1
                print(f"[REJECT] seq={seq} from={source} bytes={len(payload)} "
                      f"reason=type_{msg_type!r}", flush=True)
                continue

            if msg_type == "warning":
                warnings += 1
                container = msg.get("geometry")
                obj = msg.get("object")
            else:  # state
                container = msg.get("vehicles")
                obj = None
            container = container if isinstance(container, dict) else {}
            vehicle_b = container.get("vehicleB")
            vehicle_c = container.get("vehicleC")
            obj_source = obj.get("source") if isinstance(obj, dict) else None

            both = vehicle_ok(vehicle_b) and vehicle_ok(vehicle_c)
            both_vehicles_count += 1 if both else 0
            relayed = obj_source == "v2x_relayed"

            print(f"[RX] seq={seq} from={source} bytes={len(payload)} "
                  f"type={msg_type} warningType={fmt_field(msg.get('warningType'))} "
                  f"risk={fmt_field(msg.get('riskState'))} "
                  f"cSource={fmt_field(obj_source)} "
                  f"cPos={fmt_pos(vehicle_c)} bPos={fmt_pos(vehicle_b)}",
                  flush=True)
            print(f"[CHECK] seq={seq} both_vehicles={'yes' if both else 'no'} "
                  f"c_source_relayed={'yes' if relayed else 'no'}", flush=True)


if __name__ == "__main__":
    sys.exit(main())
