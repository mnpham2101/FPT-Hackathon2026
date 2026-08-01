"""11.1.6.3 - constant-velocity kinematics: sampled ``CpmContent`` vs hand-computed wire values.

Runs via ``python -m pytest Scenario_Player/tests`` from the repo root (the CI invocation).
The config below mirrors ``scenarios/default.yaml`` in code (frozen-sample family values), so the
hand-computed wire values are exact: 60,0 m closing at 2,5 m/s -> x wire 6000/3500/1000 at
t = 0/10/20 s. Covers the section-4.2 conversions (lat/lon 1e-7 degree, heading 0,1 degree,
position 0,01 m, velocity 0,01 m/s), pass-through fields, the F9 |mdt| <= 2047 bound (both
signs), and determinism.
"""

import json
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from player.config import ObjectConfig, ScenarioConfig, SenderConfig  # noqa: E402
from player.contracts.cpm_content import CpmContent  # noqa: E402
from player.scenario import COORDINATE_CONFIDENCE, Scenario  # noqa: E402

GOLDEN_NOMINAL = Path(__file__).resolve().parents[1] / "tests" / "fixtures" / "golden" / "nominal.json"

#: Arbitrary valid TimestampIts (the golden nominal.json referenceTime) - sample() must echo it.
REFERENCE_TIME_MS = 716084805123


def _default_config() -> ScenarioConfig:
    """``scenarios/default.yaml`` mirrored in code (SP HLD D3 shape)."""
    return ScenarioConfig(
        name="c-approaching",
        cpm_rate_hz=10.0,
        duration_s=20.0,
        loop=True,
        sender=SenderConfig(
            station_id=1201,
            lat=21.028511,
            lon=105.804817,
            heading_deg=90.0,
        ),
        object=ObjectConfig(
            object_id=7,
            initial_distance_m=60.0,
            closing_speed_mps=2.5,
            lateral_offset_m=1.2,
            classification=5,
            confidence=95,
        ),
    )


@pytest.fixture()
def scenario() -> Scenario:
    return Scenario(_default_config())


@pytest.mark.parametrize(
    ("t", "expected_x_wire"),
    [(0.0, 6000), (10.0, 3500), (20.0, 1000)],
    ids=["t0-start", "t10-mid", "t20-late"],
)
def test_position_x_matches_hand_computed(scenario: Scenario, t: float, expected_x_wire: int):
    """x(t) = 60,0 m - 2,5 m/s * t, converted to 0,01 m wire units exactly."""
    content = scenario.sample(t, REFERENCE_TIME_MS)
    assert content.object.position.x == expected_x_wire


def test_lateral_offset_constant_across_time(scenario: Scenario):
    """y is the fixed lateral_offset_m (1,2 m -> 120 in 0,01 m units) at every t."""
    for t in (0.0, 10.0, 20.0):
        assert scenario.sample(t, REFERENCE_TIME_MS).object.position.y == 120


def test_velocity_wire_units(scenario: Scenario):
    """Velocity x = -closing_speed_mps in B's frame (-2,5 m/s -> -250 in 0,01 m/s); y = 0."""
    for t in (0.0, 10.0, 20.0):
        velocity = scenario.sample(t, REFERENCE_TIME_MS).object.velocity
        assert velocity.x == -250
        assert velocity.y == 0


def test_sender_pose_wire_conversions_exact(scenario: Scenario):
    """Static sender pose: lat/lon -> 1e-7 degree, heading -> 0,1 degree, identical at every t."""
    for t in (0.0, 10.0, 20.0):
        content = scenario.sample(t, REFERENCE_TIME_MS)
        assert content.referencePosition.latitude == 210285110
        assert content.referencePosition.longitude == 1058048170
        assert content.orientationAngle == 900


def test_passthrough_fields(scenario: Scenario):
    """IDs, classification, confidence, referenceTime and mdt pass through unconverted."""
    content = scenario.sample(5.0, REFERENCE_TIME_MS, measurement_delta_ms=-50)
    assert content.stationId == 1201
    assert content.object.objectId == 7
    assert content.object.classification == 5
    assert content.object.classConfidence == 95
    assert content.referenceTime == REFERENCE_TIME_MS
    assert content.object.measurementDeltaTime == -50


def test_coordinate_confidence_mirrors_golden_nominal(scenario: Scenario):
    """The module constant is wire data frozen against the golden vector, not a tunable."""
    golden = json.loads(GOLDEN_NOMINAL.read_text(encoding="utf-8"))
    assert COORDINATE_CONFIDENCE == golden["object"]["position"]["xConfidence"]
    assert COORDINATE_CONFIDENCE == golden["object"]["position"]["yConfidence"]

    position = scenario.sample(0.0, REFERENCE_TIME_MS).object.position
    assert position.xConfidence == COORDINATE_CONFIDENCE
    assert position.yConfidence == COORDINATE_CONFIDENCE


@pytest.mark.parametrize("mdt", [2048, -2048], ids=["plus-2048", "minus-2048"])
def test_f9_bound_rejected(scenario: Scenario, mdt: int):
    """F9 bans |mdt| > 2047 - including -2048, which the wire type alone would allow."""
    with pytest.raises(ValueError, match="measurementDeltaTime"):
        scenario.sample(0.0, REFERENCE_TIME_MS, measurement_delta_ms=mdt)


@pytest.mark.parametrize("mdt", [2047, -2047], ids=["plus-2047", "minus-2047"])
def test_f9_bound_accepted(scenario: Scenario, mdt: int):
    """|mdt| = 2047 is the last profile-valid value on both sides."""
    content = scenario.sample(0.0, REFERENCE_TIME_MS, measurement_delta_ms=mdt)
    assert content.object.measurementDeltaTime == mdt


def test_sample_is_deterministic(scenario: Scenario):
    """Identical arguments always yield an equal CpmContent (pure function of its inputs)."""
    first = scenario.sample(7.5, REFERENCE_TIME_MS, measurement_delta_ms=-50)
    second = scenario.sample(7.5, REFERENCE_TIME_MS, measurement_delta_ms=-50)
    assert isinstance(first, CpmContent)
    assert first == second
