"""R1 CpmContent Python binding for the Scenario Player (bench).

This is the bench side of the R1 codec seam: the logical content of one profiled CPM, mirroring the
V2X_ECU codec-seam struct ``CpmContent`` 1:1 in **wire-native integer units** (1e-7 degree, 0,01 m,
0,01 m/s, 0,1 degree, ms). The normative home of every field is ``contracts/r1-cpm-profile.md``
sections 3-4; the node-local schema mirror this module is frozen against is
``Scenario_Player/contracts/r1-cpm-content.schema.json``.

Seam rule - this binding is a pure representation transform, nothing more:

* **No unit conversion.** Values stay in their wire-native integer units; no SI floats.
* **No derived fields.** Range/bearing (``hypot(x, y)``) and any other computation live above the
  seam in R9/F7, never here.
* **No validation-by-clamping.** Out-of-range values are carried through unchanged; range checking
  is the schema's job, not this module's.

Unknown extra keys in an incoming dict are ignored by design (additive evolution). UPER/ASN.1
encoding is deliberately absent: how the bench reaches the R1 encoder is open item F3, decided by
project-architecture in the R11 HLD. Stdlib only; Python 3.11-compatible.
"""

import json
from dataclasses import dataclass
from typing import Any, Dict


@dataclass
class ReferencePosition:
    """Sender B's WGS84 reference position (schema ``referencePosition``), 1e-7 degree."""

    latitude: int
    longitude: int


@dataclass
class ObjectPosition:
    """Perceived object's offset in the sender-B frame (schema ``object.position``), 0,01 m."""

    x: int
    y: int
    xConfidence: int
    yConfidence: int


@dataclass
class ObjectVelocity:
    """Perceived object's velocity in the sender-B frame (schema ``object.velocity``), 0,01 m/s."""

    x: int
    y: int


@dataclass
class PerceivedObject:
    """The single perceived object C carried by the profiled CPM (schema ``object``)."""

    objectId: int
    measurementDeltaTime: int
    position: ObjectPosition
    velocity: ObjectVelocity
    classification: int
    classConfidence: int


@dataclass
class CpmContent:
    """One profiled R1 CPM's logical content, in wire-native integer units."""

    stationId: int
    referenceTime: int
    referencePosition: ReferencePosition
    orientationAngle: int
    object: PerceivedObject


def to_dict(content: CpmContent) -> Dict[str, Any]:
    """Serialize to a dict using the exact R1 wire keys; values pass through unconverted."""
    return {
        "stationId": content.stationId,
        "referenceTime": content.referenceTime,
        "referencePosition": {
            "latitude": content.referencePosition.latitude,
            "longitude": content.referencePosition.longitude,
        },
        "orientationAngle": content.orientationAngle,
        "object": {
            "objectId": content.object.objectId,
            "measurementDeltaTime": content.object.measurementDeltaTime,
            "position": {
                "x": content.object.position.x,
                "y": content.object.position.y,
                "xConfidence": content.object.position.xConfidence,
                "yConfidence": content.object.position.yConfidence,
            },
            "velocity": {
                "x": content.object.velocity.x,
                "y": content.object.velocity.y,
            },
            "classification": content.object.classification,
            "classConfidence": content.object.classConfidence,
        },
    }


def from_dict(d: Dict[str, Any]) -> CpmContent:
    """Build a CpmContent from a wire dict; unknown extra keys are ignored by design."""
    reference_position = d["referencePosition"]
    perceived_object = d["object"]
    position = perceived_object["position"]
    velocity = perceived_object["velocity"]
    return CpmContent(
        stationId=d["stationId"],
        referenceTime=d["referenceTime"],
        referencePosition=ReferencePosition(
            latitude=reference_position["latitude"],
            longitude=reference_position["longitude"],
        ),
        orientationAngle=d["orientationAngle"],
        object=PerceivedObject(
            objectId=perceived_object["objectId"],
            measurementDeltaTime=perceived_object["measurementDeltaTime"],
            position=ObjectPosition(
                x=position["x"],
                y=position["y"],
                xConfidence=position["xConfidence"],
                yConfidence=position["yConfidence"],
            ),
            velocity=ObjectVelocity(x=velocity["x"], y=velocity["y"]),
            classification=perceived_object["classification"],
            classConfidence=perceived_object["classConfidence"],
        ),
    )


def to_json(content: CpmContent) -> str:
    """Encode as a compact JSON text (no UPER/ASN.1 here - that is the V2X ECU codec's job)."""
    return json.dumps(to_dict(content), separators=(",", ":"))


def from_json(text: str) -> CpmContent:
    """Decode one JSON text; tolerates surrounding whitespace."""
    return from_dict(json.loads(text))
