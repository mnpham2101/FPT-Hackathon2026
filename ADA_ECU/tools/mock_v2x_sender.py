#!/usr/bin/env python3
"""Send one R2 sample to ADA over UDP for Phase 2 receiver smoke tests."""

import argparse
import json
import pathlib
import socket
import sys
import time


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=46002)
    parser.add_argument("--sample", default="ADA_ECU/testdata/r2_v2x_object.sample.json")
    parser.add_argument("--distances", help="comma-separated object.distance sequence to send, e.g. 40,25,24,36")
    parser.add_argument("--interval-ms", type=int, default=100)
    args = parser.parse_args()

    payload = pathlib.Path(args.sample).read_text()
    messages = [payload]
    if args.distances:
        base = json.loads(payload)
        messages = []
        for distance in args.distances.split(","):
            msg = dict(base)
            msg["object"] = dict(base["object"])
            msg["object"]["distance"] = float(distance)
            msg["object"]["position"] = dict(base["object"]["position"])
            msg["object"]["position"]["x"] = float(distance)
            messages.append(json.dumps(msg, separators=(",", ":")))

    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        for message in messages:
            sock.sendto(message.encode("utf-8"), (args.host, args.port))
            time.sleep(args.interval_ms / 1000)
    return 0


if __name__ == "__main__":
    sys.exit(main())
