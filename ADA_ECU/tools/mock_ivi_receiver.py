#!/usr/bin/env python3
"""Listen for one R4 ADA->IVI warning over UDP and print it."""

from __future__ import annotations

import argparse
import json
import socket
import sys


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--host", default="0.0.0.0")
    parser.add_argument("--port", type=int, default=46004)
    parser.add_argument("--timeout", type=float, default=5.0)
    parser.add_argument("--count", type=int, default=1)
    parser.add_argument("--expect-min", type=int, default=1)
    parser.add_argument("--output", type=argparse.FileType("w"))
    args = parser.parse_args()

    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        sock.bind((args.host, args.port))
        sock.settimeout(args.timeout)
        received = 0
        for _ in range(args.count):
            try:
                payload, sender = sock.recvfrom(65535)
            except TimeoutError:
                break

            message = json.loads(payload.decode("utf-8"))
            if message.get("type") != "warning":
                print("received non-warning R4 message", file=sys.stderr)
                return 2
            if message.get("warningType") != "nlos_obstruction":
                print("received unexpected warningType", file=sys.stderr)
                return 3
            if message.get("riskState") not in {"low", "medium", "high"}:
                print("received unexpected riskState", file=sys.stderr)
                return 4
            geometry = message.get("geometry", {})
            if "vehicleB" not in geometry or "vehicleC" not in geometry:
                print("R4 warning missing vehicleB/vehicleC geometry", file=sys.stderr)
                return 5
            if not any(
                obj.get("id") == "own:B" for obj in message.get("trackedObjects", [])
            ):
                print("R4 warning missing own:B tracked object", file=sys.stderr)
                return 6

            line = json.dumps(message, separators=(",", ":"))
            if args.output:
                args.output.write(line + "\n")
                args.output.flush()
            print(
                json.dumps(
                    {"sender": sender[0], "port": sender[1], "r4": message},
                    separators=(",", ":"),
                )
            )
            received += 1
    if received < args.expect_min:
        print(
            f"expected at least {args.expect_min} R4 messages, got {received}",
            file=sys.stderr,
        )
        return 7
    return 0


if __name__ == "__main__":
    sys.exit(main())
