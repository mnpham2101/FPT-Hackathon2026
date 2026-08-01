"""Constant-velocity scenario kinematics for the Scenario Player (bench).

SP HLD D3: **one kinematic model, never code branches per scenario** - every difference between
scenarios comes from ``ScenarioConfig`` values alone. Given scenario time ``t``, ``Scenario.sample``
yields one profiled ``CpmContent``: sender B's static WGS84 pose + heading, and object C closing
along x in B's cartesian frame (``x(t) = initial_distance_m - closing_speed_mps * t``, fixed
lateral offset, velocity x = ``-closing_speed_mps``), converted to wire-native integer units per
the authoritative conversion table (callflow note section 4.2, mirrored by
``Scenario_Player/contracts/r1-cpm-content.schema.json``).

Validation posture (F9): the bench validates **before** encode - ``measurementDeltaTime`` is
bounded to |mdt| <= 2047 and every produced position/coordinate wire integer is checked against
its schema bound; a violation raises ``ValueError`` naming the field. Stdlib only; Python
3.11-compatible.
"""

from player.config import ScenarioConfig
from player.contracts.cpm_content import (
    CpmContent,
    ObjectPosition,
    ObjectVelocity,
    PerceivedObject,
    ReferencePosition,
)

#: CoordinateConfidence for both object position axes (0,01 m wire units). This mirrors the frozen
#: golden vector ``tests/fixtures/golden/nominal.json`` (xConfidence/yConfidence = 90) and is
#: **wire data, not a scenario tunable**: the D3 scenario-YAML shape is frozen and carries no
#: confidence-of-position field, so changing this value is a contract-sample change to flag to the
#: user, never a config knob to absorb here.
COORDINATE_CONFIDENCE: int = 90

# Conversion factors - callflow note section 4.2 (wire-format definitions, not tunables).
_WGS84_UNITS_PER_DEGREE: int = 10_000_000  # degrees -> 1e-7 degree (referencePosition lat/lon)
_ANGLE_UNITS_PER_DEGREE: int = 10  # degrees -> 0,1 degree (orientationAngle)
_UNITS_PER_METRE: int = 100  # m -> 0,01 m (object position)
_UNITS_PER_MPS: int = 100  # m/s -> 0,01 m/s (object velocity)
_ORIENTATION_MODULUS: int = 3600  # full circle in 0,1-degree units

# Schema bounds guarded pre-encode (r1-cpm-content.schema.json). Latitude/longitude maxima exclude
# the schema's +1 "unavailable" sentinels (900000001 / 1800000001) - the model never emits them.
_LATITUDE_BOUND: int = 900_000_000
_LONGITUDE_BOUND: int = 1_800_000_000
_POSITION_MIN: int = -131_072
_POSITION_MAX: int = 131_071
_VELOCITY_BOUND: int = 16_383
_MDT_LIMIT: int = 2_047  # F9: profile-valid |measurementDeltaTime|


def _checked(field: str, value: int, minimum: int, maximum: int) -> int:
    """Return ``value`` if within the wire bound, else raise ``ValueError`` naming the field."""
    if not minimum <= value <= maximum:
        raise ValueError(
            f"{field} = {value} outside wire bound {minimum}..{maximum}"
            " (bench validates pre-encode, F9 posture)"
        )
    return value


class Scenario:
    """The single constant-velocity kinematic model (SP HLD D3) over one ``ScenarioConfig``."""

    def __init__(self, config: ScenarioConfig) -> None:
        self._config = config

    def sample(
        self,
        t: float,
        reference_time_ms: int,
        measurement_delta_ms: int = 0,
    ) -> CpmContent:
        """Sample the model at scenario time ``t`` (seconds) into one wire-native ``CpmContent``.

        Pure and deterministic in its arguments: the generator supplies the wall-clock
        ``reference_time_ms`` (TimestampIts, ms) and the per-message ``measurement_delta_ms``;
        identical arguments always yield an equal ``CpmContent``.
        """
        # F9 - asserted before any content is built. Note: the wire type
        # DeltaTimeMilliSecondSigned allows -2048, but F9 bans it (|mdt| <= 2047), so -2048 is
        # rejected here even though it would encode.
        if abs(measurement_delta_ms) > _MDT_LIMIT:
            raise ValueError(
                f"object.measurementDeltaTime = {measurement_delta_ms} violates F9"
                f" (|measurementDeltaTime| <= {_MDT_LIMIT})"
            )

        sender = self._config.sender
        obj = self._config.object

        # Sender B: static WGS84 pose + heading from config, per-message conversion only.
        latitude = _checked(
            "referencePosition.latitude",
            round(sender.lat * _WGS84_UNITS_PER_DEGREE),
            -_LATITUDE_BOUND,
            _LATITUDE_BOUND,
        )
        longitude = _checked(
            "referencePosition.longitude",
            round(sender.lon * _WGS84_UNITS_PER_DEGREE),
            -_LONGITUDE_BOUND,
            _LONGITUDE_BOUND,
        )
        # Normalized into the schema's legal real-heading range 0..3599 (3600+ are the
        # doNotUse/unavailable sentinels); Python % keeps negative headings non-negative.
        orientation_angle = (
            round(sender.heading_deg * _ANGLE_UNITS_PER_DEGREE) % _ORIENTATION_MODULUS
        )

        # Object C: constant-velocity closing along x in B's cartesian frame (callflow F-note:
        # PerceivedObject velocity is expressed in the sender's frame).
        x_metres = obj.initial_distance_m - obj.closing_speed_mps * t
        position_x = _checked(
            "object.position.x", round(x_metres * _UNITS_PER_METRE), _POSITION_MIN, _POSITION_MAX
        )
        position_y = _checked(
            "object.position.y",
            round(obj.lateral_offset_m * _UNITS_PER_METRE),
            _POSITION_MIN,
            _POSITION_MAX,
        )
        velocity_x = _checked(
            "object.velocity.x",
            round(-obj.closing_speed_mps * _UNITS_PER_MPS),
            -_VELOCITY_BOUND,
            _VELOCITY_BOUND,
        )
        velocity_y = 0

        return CpmContent(
            stationId=sender.station_id,
            referenceTime=reference_time_ms,
            referencePosition=ReferencePosition(latitude=latitude, longitude=longitude),
            orientationAngle=orientation_angle,
            object=PerceivedObject(
                objectId=obj.object_id,
                measurementDeltaTime=measurement_delta_ms,
                position=ObjectPosition(
                    x=position_x,
                    y=position_y,
                    xConfidence=COORDINATE_CONFIDENCE,
                    yConfidence=COORDINATE_CONFIDENCE,
                ),
                velocity=ObjectVelocity(x=velocity_x, y=velocity_y),
                classification=obj.classification,
                classConfidence=obj.confidence,
            ),
        )
