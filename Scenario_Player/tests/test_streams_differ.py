"""11.1.6.4 - the two committed scenario YAMLs yield differing ``CpmContent`` sequences.

Runs via ``python -m pytest Scenario_Player/tests`` from the repo root (the CI invocation).
Model-level half of the R11 acceptance box (SP HLD section 9; the live half is 11.1.10.4): both
committed scenarios are sampled over the same time grid with the same ``reference_time_ms``, and
the streams must differ **only** in the config-driven kinematics (SP HLD D3 - one model, no code
branches): default approaches (x wire strictly decreasing, matching 60,0 m - 2,5 m/s * t), while
c-out-of-range stays static beyond the 35 m exit gate; every non-kinematic field is identical.
"""

import sys
from pathlib import Path
from typing import Any, Dict, List

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from player.config import load_scenario  # noqa: E402
from player.contracts.cpm_content import CpmContent, to_dict  # noqa: E402
from player.scenario import Scenario  # noqa: E402

SCENARIOS_DIR = Path(__file__).resolve().parents[1] / "scenarios"

#: Arbitrary valid TimestampIts (the golden nominal.json referenceTime) - fixed across the grid so
#: any stream difference is attributable to the scenario configs alone.
REFERENCE_TIME_MS = 716084805123

#: Shared sampling grid: t = 0..20 s step 1 s, covering both YAMLs' full duration_s = 20,0 s.
TIME_GRID = [float(t) for t in range(21)]

#: 35 m exit gate in 0,01 m wire units (c-out-of-range must stay above it - YAML pairing comment).
EXIT_GATE_WIRE = 3500


def _sample_stream(yaml_name: str) -> List[CpmContent]:
    scenario = Scenario(load_scenario(SCENARIOS_DIR / yaml_name))
    return [scenario.sample(t, REFERENCE_TIME_MS) for t in TIME_GRID]


@pytest.fixture(scope="module")
def default_stream() -> List[CpmContent]:
    return _sample_stream("default.yaml")


@pytest.fixture(scope="module")
def out_of_range_stream() -> List[CpmContent]:
    return _sample_stream("c-out-of-range.yaml")


# --- material difference: approaching vs static --------------------------------------------------


def test_x_sequences_differ_at_every_positive_t(default_stream, out_of_range_stream):
    """The object-x wire sequences diverge at every t > 0 (both start at 60,0 m, so t = 0 ties)."""
    for t, default, static in zip(TIME_GRID, default_stream, out_of_range_stream):
        if t > 0:
            assert default.object.position.x != static.object.position.x, f"x equal at t={t}"


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
    """default.yaml: x(t) = 60,0 m - 2,5 m/s * t -> round((60.0 - 2.5*t) * 100) wire, exactly."""
    for t, content in zip(TIME_GRID, default_stream):
        assert content.object.position.x == round((60.0 - 2.5 * t) * 100)


def test_out_of_range_x_static_beyond_exit_gate(out_of_range_stream):
    """c-out-of-range.yaml: x wire == 6000 (60,0 m) at every t, above the 35 m exit gate."""
    for content in out_of_range_stream:
        assert content.object.position.x == 6000
        assert content.object.position.x > EXIT_GATE_WIRE


def test_velocity_matches_each_yaml(default_stream, out_of_range_stream):
    """Velocity x mirrors each YAML's closing_speed_mps: -2,5 m/s -> -250 wire vs static 0."""
    for default, static in zip(default_stream, out_of_range_stream):
        assert default.object.velocity.x == -250
        assert static.object.velocity.x == 0


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
