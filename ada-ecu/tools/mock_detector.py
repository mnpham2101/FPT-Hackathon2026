#!/usr/bin/env python3
"""Phase 3 subprocess seam mock: emit R3 JSONL own-sensor detections.

Replace the body with OpenCV + YOLO11n ONNX Runtime CPU inference once the
provided video format is confirmed. The stdout contract should stay stable.
"""

import json
import sys
import time


def main() -> int:
    now_ms = int(time.time() * 1000)
    for frame in range(3):
        event = {
            "id": "own:B",
            "class": "vehicle",
            "source": "own_sensor",
            "position": {"x": 12.0 + frame * 0.2, "y": 0.0, "confidence": 0.9},
            "distance": 12.0 + frame * 0.2,
            "speed": 15.0,
            "confidence": 0.92,
            "state": "tentative",
            "timestamps": {
                "measured": now_ms + frame * 100,
                "received": now_ms + frame * 100,
                "lastUpdated": now_ms + frame * 100,
            },
        }
        print(json.dumps(event), flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())

