"""R3 record construction and JSONL emission."""

from __future__ import annotations

import json
import time
from collections.abc import Iterable

from detector.models import DetectionBackend, FrameInput


def r3_vehicle_b(
    distance_m: float,
    lateral_m: float,
    confidence: float,
    timestamp_ms: int,
    speed_mps: float,
) -> dict:
    received_ms = int(time.time() * 1000)
    return {
        "id": "own:B",
        "class": "vehicle",
        "source": "own_sensor",
        "position": {
            "x": round(distance_m, 3),
            "y": round(lateral_m, 3),
            "confidence": confidence,
        },
        "distance": round(distance_m, 3),
        "speed": round(speed_mps, 3),
        "confidence": confidence,
        "state": "not_tracked",
        "timestamps": {
            "measured": timestamp_ms,
            "received": received_ms,
            "lastUpdated": received_ms,
        },
    }


def emit_jsonl(samples: Iterable[FrameInput], backend: DetectionBackend) -> int:
    count = 0
    for sample in samples:
        for detection in backend.detect(sample):
            print(json.dumps(detection, separators=(",", ":")), flush=True)
            count += 1
    return count
