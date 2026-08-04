"""11.1.6.4 - the two committed scenario YAMLs yield differing ``CpmContent`` sequences.

Runs via ``python -m pytest Scenario_Player/tests`` from the repo root (the CI invocation).
Model-level half of the R11 acceptance box (SP HLD section 9; the live half is 11.1.10.4): both
committed scenarios are sampled over the same time grid with the same ``reference_time_ms``, and
the streams must differ **only** in the config-driven kinematics (SP HLD D3 - one model, no code
branches). ``default.yaml`` carries the R22 demo cycle and approaches (x wire strictly decreasing),
while ``c-out-of-range.yaml`` stays static beyond the 35 m exit gate; every non-kinematic field is
identical.

Every expected kinematic value here is **derived from the loaded** ``ScenarioConfig``, never
re-typed: a re-typed copy of the YAML is what silently broke this file when the demo cycle was
retimed. The scenario geometry itself is pinned as literals in exactly one place -
``test_config.py``'s ``TestCommittedScenarioVariants`` - so intent has one home and this file
checks the wiring from that data to the wire.
"""

import sys
from pathlib import Path
from typing import Any, Dict, List

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from player.config import ScenarioConfig, load_scenario  # noqa: E402
from player.contracts.cpm_content import CpmContent, to_dict  # noqa: E402
from player.scenario import Scenario  # noqa: E402

SCENARIOS_DIR = Path(__file__).resolve().parents[1] / "scenarios"

#: Arbitrary valid TimestampIts (the golden nominal.json referenceTime) - fixed across the grid so
#: any stream difference is attributable to the scenario configs alone.
REFERENCE_TIME_MS = 716084805123

DEFAULT_CONFIG: ScenarioConfig = load_scenario(SCENARIOS_DIR / "default.yaml")
OUT_OF_RANGE_CONFIG: ScenarioConfig = load_scenario(SCENARIOS_DIR / "c-out-of-range.yaml")

#: Shared sampling grid, 1,0 s steps over the shorter of the two committed cycles with the endpoint
#: excluded: the generator emits t in [0, duration_s), so a grid reaching duration_s would sample a
#: cycle that never goes on the wire. Taken from the loaded configs rather than written down, so a
#: later retune of either duration_s cannot leave this file sampling past the run.
GRID_STEP_S = 1.0
_GRID_SPAN_S = min(DEFAULT_CONFIG.duration_s, OUT_OF_RANGE_CONFIG.duration_s)
TIME_GRID = [n * GRID_STEP_S for n in range(int(_GRID_SPAN_S / GRID_STEP_S))]

#: 35 m exit gate in 0,01 m wire units (c-out-of-range must stay above it - YAML pairing comment).
EXIT_GATE_WIRE = 3500

#: m -> 0,01 m and m/s -> 0,01 m/s, mirroring player.scenario's conversion table (callflow 4.2).
_UNITS_PER_METRE = 100
_UNITS_PER_MPS = 100


def _sample_stream(config: ScenarioConfig) -> List[CpmContent]:
    scenario = Scenario(config)
    return [scenario.sample(t, REFERENCE_TIME_MS) for t in TIME_GRID]


@pytest.fixture(scope="module")
def default_stream() -> List[CpmContent]:
    return _sample_stream(DEFAULT_CONFIG)


@pytest.fixture(scope="module")
def out_of_range_stream() -> List[CpmContent]:
    return _sample_stream(OUT_OF_RANGE_CONFIG)


# --- material difference: approaching vs static --------------------------------------------------


def test_x_sequences_differ_and_cross_at_most_once(default_stream, out_of_range_stream):
    """R11's observable difference, stated as what the two models can actually guarantee.

    A strictly decreasing sequence meets a constant one at most once, so equality at a single grid
    point is the geometry - ``default.yaml`` passing through the static variant's range - and not a
    defect. What R11 requires is that the two streams are not the same stream.
    """
    default_xs = [content.object.position.x for content in default_stream]
    static_xs = [content.object.position.x for content in out_of_range_stream]
    assert default_xs != static_xs
    ties = [t for t, moving, static in zip(TIME_GRID, default_xs, static_xs) if moving == static]
    assert len(ties) <= 1, f"a strictly decreasing stream cannot tie a constant one twice: {ties}"


def test_default_x_strictly_decreases(default_stream):
    """default.yaml is the approaching variant: x wire strictly decreases across the grid."""
    xs = [content.object.position.x for content in default_stream]
    assert all(later < earlier for earlier, later in zip(xs, xs[1:]))


def test_out_of_range_x_constant(out_of_range_stream):
    """c-out-of-range.yaml is the static variant: x wire never changes across the grid."""
    xs = [content.object.position.x for content in out_of_range_stream]
    assert len(set(xs)) == 1


# --- each stream matches its own YAML kinematics --------------------------------------------------


def test_default_x_matches_yaml_kinematics(default_stream):
    """x(t) = initial_distance_m - closing_speed_mps * t, in 0,01 m wire units, exactly."""
    obj = DEFAULT_CONFIG.object
    for t, content in zip(TIME_GRID, default_stream):
        expected = round((obj.initial_distance_m - obj.closing_speed_mps * t) * _UNITS_PER_METRE)
        assert content.object.position.x == expected, f"x mismatch at t={t}"


def test_out_of_range_x_static_beyond_exit_gate(out_of_range_stream):
    """c-out-of-range.yaml: x wire holds its initial distance at every t, above the 35 m gate."""
    expected = round(OUT_OF_RANGE_CONFIG.object.initial_distance_m * _UNITS_PER_METRE)
    for content in out_of_range_stream:
        assert content.object.position.x == expected
        assert content.object.position.x > EXIT_GATE_WIRE


def test_velocity_matches_each_yaml(default_stream, out_of_range_stream):
    """Velocity x mirrors each YAML's closing_speed_mps, negated into B's frame; y is always 0."""
    default_expected = round(-DEFAULT_CONFIG.object.closing_speed_mps * _UNITS_PER_MPS)
    static_expected = round(-OUT_OF_RANGE_CONFIG.object.closing_speed_mps * _UNITS_PER_MPS)
    assert default_expected != static_expected, "the two variants must differ in velocity (D3)"
    for default, static in zip(default_stream, out_of_range_stream):
        assert default.object.velocity.x == default_expected
        assert static.object.velocity.x == static_expected


# --- the difference is purely config-driven kinematics (SP HLD D3) --------------------------------


def _without_kinematics(content: CpmContent) -> Dict[str, Any]:
    """Wire dict with the two kinematic fields (object x position/velocity) removed."""
    wire = to_dict(content)
    del wire["object"]["position"]["x"]
    del wire["object"]["velocity"]["x"]
    return wire


def test_non_kinematic_fields_identical_at_equal_t(default_stream, out_of_range_stream):
    """Everything but object x position/velocity is equal at equal t: sender pose, IDs,
    classification/confidence - so the stream difference is config-driven kinematics only (D3)."""
    for default, static in zip(default_stream, out_of_range_stream):
        assert _without_kinematics(default) == _without_kinematics(static)
