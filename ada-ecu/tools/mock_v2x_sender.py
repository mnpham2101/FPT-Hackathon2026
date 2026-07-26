#!/usr/bin/env python3
"""Send one R2 sample to ADA over UDP for Phase 2 receiver smoke tests."""

import argparse
import pathlib
import socket
import sys


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=46002)
    parser.add_argument("--sample", default="ada-ecu/testdata/r2_v2x_object.sample.json")
    args = parser.parse_args()

    payload = pathlib.Path(args.sample).read_bytes()
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        sock.sendto(payload, (args.host, args.port))
    return 0


if __name__ == "__main__":
    sys.exit(main())
