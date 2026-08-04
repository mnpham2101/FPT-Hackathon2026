"""R3 JSONL emitter for the ADA detector subprocess (R12).

One R3 line per detection per sampled frame, serialized through the frozen Phase 0 binding
``contracts.tracked_object`` (design D1: no second model is declared here). Every line carries
``source == "own_sensor"``; the emitter cannot produce any other source value.
Stdlib only; Python 3.11-compatible.
"""

import time
from dataclasses import dataclass
from typing import Callable, Dict, Optional, TextIO, Tuple

from contracts.tracked_object import Timestamps, TrackedObject, Vec2, to_jsonl

# Constants per design D6 + HLD section 10.2 — one value per fixed field.
_SOURCE = "own_sensor"
_CLASS = "vehicle"
# The store is the sole state writer (D3); the detector always emits not_tracked.
_STATE = "not_tracked"


@dataclass
class EmitItem:
    """One detection on one sampled frame, as the tracker hands it to the emitter."""

    track_id: str      # the tracker's 'own:<n>'
    distance_m: float  # pinhole range
    lateral_m: float   # lateral offset
    confidence: float  # detection score
    clip_ts_ms: float  # the sampled frame's clip-time Frame.timestamp_ms (detector-local, exact under CFR)


class R3Emitter:
    """Maps EmitItems to R3 TrackedObject JSONL lines, one line per detection per sampled frame."""

    def __init__(self, now_ms: Optional[Callable[[], int]] = None) -> None:
        # CLOCK_REALTIME epoch ms — the wire clock per design D10.
        self.now_ms: Callable[[], int] = now_ms if now_ms is not None else (lambda: int(time.time() * 1000))
        # Per-track (distance_m, clip_ts_ms) of the previous sampled frame, for the speed estimate.
        self._last: Dict[str, Tuple[float, float]] = {}

    def emit_frame(self, items: "list[EmitItem]", measured_ms: int, stream: TextIO) -> None:
        """Emit one R3 JSONL line per item, then flush the stream once for the frame batch."""
        received_ms = self.now_ms()
        for item in items:
            # Frozen R3 bounds speed at minimum 0, so a signed closing rate is not representable;
            # the sign lives in the distance series (D6).
            speed = 0.0
            prev = self._last.get(item.track_id)
            if prev is not None:
                prev_distance, prev_ts = prev
                dt_s = (item.clip_ts_ms - prev_ts) / 1000.0
                if dt_s > 0:
                    speed = abs(item.distance_m - prev_distance) / dt_s
            self._last[item.track_id] = (item.distance_m, item.clip_ts_ms)

            # Schema minimum 0 on distance — a negative pinhole range cannot reach the wire.
            distance = max(0.0, item.distance_m)
            confidence = min(1.0, max(0.0, item.confidence))

            obj = TrackedObject(
                id=item.track_id,
                object_class=_CLASS,
                source=_SOURCE,
                position=Vec2(x=item.distance_m, y=item.lateral_m),
                distance=distance,
                speed=speed,
                confidence=confidence,
                state=_STATE,
                timestamps=Timestamps(
                    measured=measured_ms,
                    received=received_ms,
                    # lastUpdated mirrors received — the store overwrites it
                    # (as in ADA_ECU/tests/fixtures/own_sensor_mock.jsonl).
                    lastUpdated=received_ms,
                ),
            )
            stream.write(to_jsonl(obj) + "\n")
        stream.flush()
