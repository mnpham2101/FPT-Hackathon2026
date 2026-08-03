#!/usr/bin/env python3
"""Download the YOLO ONNX model used by the Phase 3 detector demo."""

from __future__ import annotations

import argparse
import pathlib
import sys
import urllib.request

DEFAULT_URL = (
    "https://github.com/ultralytics/assets/releases/download/v8.4.0/yolo11n.onnx"
)


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Download pretrained YOLO11n ONNX model for ADA Phase 3."
    )
    parser.add_argument("--url", default=DEFAULT_URL)
    parser.add_argument("--output", default="ADA_ECU/models/yolo11n.onnx")
    args = parser.parse_args()

    output = pathlib.Path(args.output)
    output.parent.mkdir(parents=True, exist_ok=True)
    if output.exists() and output.stat().st_size > 1_000_000:
        print(f"model already exists: {output} ({output.stat().st_size} bytes)")
        return 0

    print(f"downloading {args.url} -> {output}")
    try:
        urllib.request.urlretrieve(args.url, output)
    except Exception as exc:  # noqa: BLE001 - CLI should surface download failures clearly.
        print(f"failed to download model: {exc}", file=sys.stderr)
        return 1

    if output.stat().st_size < 1_000_000:
        print(
            f"downloaded file is too small to be a YOLO model: {output.stat().st_size} bytes",
            file=sys.stderr,
        )
        return 2
    print(f"downloaded model: {output} ({output.stat().st_size} bytes)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
