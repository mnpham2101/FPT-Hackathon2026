#!/usr/bin/env python3
"""Run the Phase 2/3/4 deterministic IVI timeline demo.

Demo contract:
- t=1.00s: video detector has vehicle B (`own:B`) from R3.
- t=1.01s: V2X R2 message has vehicle C (`v2x:1201:7`).
- t=1.02s: ADA sends R4 to IVI with both vehicles and fused geometry.
"""

from __future__ import annotations

import argparse
import json
import os
import pathlib
import socket
import subprocess
import sys
import time


def wait_until(start: float, target_s: float) -> None:
    remaining = start + target_s - time.monotonic()
    if remaining > 0:
        time.sleep(remaining)


def reserve_udp_port() -> tuple[socket.socket, int]:
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind(("127.0.0.1", 0))
    sock.settimeout(3.0)
    return sock, sock.getsockname()[1]


def validate_r4(message: dict) -> None:
    tracked = message.get("trackedObjects", [])
    geometry = message.get("geometry", {})
    ids = {obj.get("id") for obj in tracked}
    if "own:B" not in ids:
        raise RuntimeError("R4 trackedObjects is missing vehicle B own:B")
    if "v2x:1201:7" not in ids:
        raise RuntimeError("R4 trackedObjects is missing vehicle C v2x:1201:7")
    if "vehicleB" not in geometry or "vehicleC" not in geometry:
        raise RuntimeError("R4 geometry is missing vehicleB/vehicleC")

    b = next(obj for obj in tracked if obj.get("id") == "own:B")
    c = next(obj for obj in tracked if obj.get("id") == "v2x:1201:7")
    if b.get("timestamps", {}).get("measured") != 1000:
        raise RuntimeError(
            f"vehicle B measured timestamp must be 1000ms, got {b.get('timestamps')}"
        )
    c_timestamps = c.get("timestamps", {})
    if c_timestamps.get("measured") != 1010 or c_timestamps.get("received") != 1010:
        raise RuntimeError(f"vehicle C timestamp must be 1010ms, got {c_timestamps}")


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Demo t=1.00 B, t=1.01 C, t=1.02 ADA->IVI R4 fusion."
    )
    parser.add_argument("--build-dir", default="ADA_ECU/build-runtime")
    parser.add_argument("--config", default="ADA_ECU/config/ada-ecu.conf")
    parser.add_argument(
        "--r3", default="ADA_ECU/testdata/demo_timeline_r3_own_sensor.jsonl"
    )
    parser.add_argument("--r2", default="ADA_ECU/testdata/demo_timeline_r2_v2x_c.json")
    args = parser.parse_args()

    repo_root = pathlib.Path(__file__).resolve().parents[2]
    ada_bin = repo_root / args.build_dir / "ada_ecu"
    if not ada_bin.exists():
        print(f"ADA binary not found: {ada_bin}", file=sys.stderr)
        return 2

    start = time.monotonic()
    sock, ivi_port = reserve_udp_port()
    env = os.environ.copy()
    env["IVI_HOST"] = "127.0.0.1"
    env["IVI_PORT"] = str(ivi_port)
    env["RISK_DWELL_MS"] = "0"
    ada = subprocess.Popen(
        [
            str(ada_bin),
            "--config",
            str(repo_root / args.config),
            "--mock",
            "--r2-sample",
            str(repo_root / args.r2),
            "--mock-received-ms",
            "1010",
            "--mock-start-delay-ms",
            "1020",
            "--own-sensor-sample",
            str(repo_root / args.r3),
        ],
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        env=env,
    )
    try:
        wait_until(start, 1.00)
        print("t=1.00s video: vehicle B detected from R3 own_sensor sample")

        wait_until(start, 1.01)
        print("t=1.01s v2x: vehicle C received from R2 v2x sample")

        wait_until(start, 1.02)
        print(f"t=1.02s ada: sending R4 to IVI UDP 127.0.0.1:{ivi_port}")

        payload, _ = sock.recvfrom(65535)
        r4 = json.loads(payload.decode("utf-8"))
        validate_r4(r4)
        _stdout, stderr = ada.communicate(timeout=3.0)
        if ada.returncode != 0:
            print(stderr, file=sys.stderr)
            return ada.returncode or 1
        elapsed_s = time.monotonic() - start
        print(
            f"t=1.02s ivi: received R4 warning with vehicleB and vehicleC (wall-clock {elapsed_s:.2f}s)"
        )
        print(json.dumps(r4, separators=(",", ":")))
        return 0
    except (TimeoutError, RuntimeError, json.JSONDecodeError) as exc:
        print(f"timeline demo failed: {exc}", file=sys.stderr)
        return 1
    finally:
        if ada.poll() is None:
            ada.terminate()
        sock.close()


if __name__ == "__main__":
    sys.exit(main())
