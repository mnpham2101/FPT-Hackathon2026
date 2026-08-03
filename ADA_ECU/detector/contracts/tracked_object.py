"""R3 TrackedObject Python binding for the ADA detector subprocess.

Frozen against the R3 schema landed by 3.0.1.4 (``ADA_ECU/contracts/r3-tracked-object.schema.json``).
This module is the R12 detector-subprocess stdout wire shape: one R3 object per line as JSONL over
stdout — no FFI/RPC. Wire keys are the schema's exact JSON keys; the JSON key ``"class"`` (a Python
keyword) maps to the attribute ``object_class``. Unknown extra keys in incoming dicts are ignored by
design (additive evolution). Stdlib only; Python 3.11-compatible.
"""

import json
from dataclasses import dataclass
from typing import Any


@dataclass
class Vec2:
    """Ego-frame position, metres (schema ``position``)."""

    x: float
    y: float


@dataclass
class Timestamps:
    """Millisecond-epoch times (schema ``timestamps``); key names kept verbatim."""

    measured: int
    received: int
    lastUpdated: int


@dataclass
class TrackedObject:
    """One R3 tracked object; ``object_class`` carries the JSON key ``"class"``."""

    id: str
    object_class: str
    source: str
    position: Vec2
    distance: float
    speed: float
    confidence: float
    state: str
    timestamps: Timestamps


def to_dict(obj: TrackedObject) -> dict[str, Any]:
    """Serialize to a dict using the exact R3 wire keys (``object_class`` -> ``"class"``)."""
    return {
        "id": obj.id,
        "class": obj.object_class,
        "source": obj.source,
        "position": {"x": obj.position.x, "y": obj.position.y},
        "distance": obj.distance,
        "speed": obj.speed,
        "confidence": obj.confidence,
        "state": obj.state,
        "timestamps": {
            "measured": obj.timestamps.measured,
            "received": obj.timestamps.received,
            "lastUpdated": obj.timestamps.lastUpdated,
        },
    }


def from_dict(d: dict[str, Any]) -> TrackedObject:
    """Build a TrackedObject from a wire dict; unknown extra keys are ignored by design."""
    position = d["position"]
    timestamps = d["timestamps"]
    return TrackedObject(
        id=d["id"],
        object_class=d["class"],
        source=d["source"],
        position=Vec2(x=position["x"], y=position["y"]),
        distance=d["distance"],
        speed=d["speed"],
        confidence=d["confidence"],
        state=d["state"],
        timestamps=Timestamps(
            measured=timestamps["measured"],
            received=timestamps["received"],
            lastUpdated=timestamps["lastUpdated"],
        ),
    )


def to_jsonl(obj: TrackedObject) -> str:
    """Encode as one compact JSONL line (no embedded newline; caller appends the line break)."""
    return json.dumps(to_dict(obj), separators=(",", ":"))


def from_jsonl(line: str) -> TrackedObject:
    """Decode one JSONL line; tolerates an optional trailing newline."""
    return from_dict(json.loads(line.strip()))
