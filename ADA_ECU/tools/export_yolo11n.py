#!/usr/bin/env python3
"""One-off Ultralytics -> ONNX export of YOLO11n for the R12 detector (ADA HLD D6).

The exported ``ADA_ECU/models/yolo11n.onnx`` is COMMITTED, so the image build stays
offline-reproducible and neither CI nor the runtime image ever depends on the
Ultralytics package (AGPL-3.0 -- accepted for this one-off tooling use,
requirements report section 4). This script is host-side tooling only: it is not
part of any CI lane, never enters the image, and its Ultralytics import is
deliberately absent from detector/requirements.txt.

Export parameters (recorded so a re-export is verifiable against the committed
artifact):

  input size : 640x640 (tensor ``images``, float32, 1x3x640x640)
  opset      : 12 (passed explicitly below; a re-export must state the same)
  weights    : yolo11n.pt from the Ultralytics release assets
  weights URL: https://github.com/ultralytics/assets/releases/download/v8.3.0/yolo11n.pt
  sha256 yolo11n.pt   : 0ebbc80d4a7680d14987a577cd21342b65ecfd94632bd9a8da63ae6417644ee1
  sha256 yolo11n.onnx : fbec30c6704d9b4478e5774574f2ed8d4417eb36b2cebd577400403261aebbcd

The weights download needs egress to the Ultralytics release assets. Where the
export host has none, a human fetches yolo11n.pt and passes it via --weights;
the export then runs entirely against the local file.

Usage:
  python ADA_ECU/tools/export_yolo11n.py [--weights /path/to/yolo11n.pt]
                                         [--out ADA_ECU/models/yolo11n.onnx]
"""

from __future__ import annotations

import argparse
import hashlib
import shutil
import sys
from pathlib import Path

INPUT_SIZE = 640
OPSET = 12


def sha256_of(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as fh:
        for chunk in iter(lambda: fh.read(1 << 20), b""):
            digest.update(chunk)
    return digest.hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument(
        "--weights",
        default="yolo11n.pt",
        help="path to (or auto-download name of) the Ultralytics .pt weights",
    )
    parser.add_argument(
        "--out",
        default=str(Path(__file__).resolve().parents[1] / "models" / "yolo11n.onnx"),
        help="destination for the exported ONNX file",
    )
    args = parser.parse_args()

    try:
        from ultralytics import YOLO
    except ImportError:
        print(
            "ultralytics is not installed. This one-off exporter needs it; "
            "install it in a scratch environment (it must NOT enter "
            "detector/requirements.txt).",
            file=sys.stderr,
        )
        return 2

    weights = Path(args.weights)
    if weights.exists():
        print(f"weights: {weights} sha256={sha256_of(weights)}")
    else:
        print(f"weights {weights} not local - Ultralytics will download them")

    model = YOLO(str(weights))
    exported = Path(model.export(format="onnx", imgsz=INPUT_SIZE, opset=OPSET))

    out = Path(args.out)
    out.parent.mkdir(parents=True, exist_ok=True)
    if exported.resolve() != out.resolve():
        shutil.copyfile(exported, out)
    print(f"exported: {out} sha256={sha256_of(out)}")
    print("Record both SHA-256 values in this script's header when re-exporting.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
