"""11.1.6.1 - env + scenario-YAML loader validation (SP HLD D3/S5).

Runs via ``python -m pytest Scenario_Player/tests`` from the repo root (the CI invocation).
Covers: the valid D3 shape loading with exact field values, the F8 ``cpm_rate_hz`` default,
every rejection class (missing key, mistyped value incl. bool-vs-number, non-positive
rate/duration, unreadable file) asserting the offending key/path is named, and the HLD S5
env set (defaults, overrides, strict port parsing).
"""

import copy
import sys
from pathlib import Path
from typing import Any

import pytest
import yaml

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from player.config import (  # noqa: E402
    DEFAULT_CPM_RATE_HZ,
    EnvConfig,
    load_env,
    load_scenario,
)

PHASED_VALID: dict = {
    "name": "phased-test",
    "loop": True,
    "sender": {
        "station_id": 1001,
        "lat": 21.028511,
        "lon": 105.804817,
        "heading_deg": 90.0,
    },
    "phases": {
        "waiting_s": 10.0,
        "two_vehicle_s": 5.0,
        "three_vehicle_s": 5.0,
    },
    "two_vehicle_object": {
        "object_id": 7,
        "initial_distance_m": 60.0,
        "closing_speed_mps": 0.0,
        "lateral_offset_m": 1.2,
        "classification": 5,
        "confidence": 95,
    },
    "three_vehicle_object": {
        "object_id": 7,
        "initial_distance_m": 25.0,
        "closing_speed_mps": 3.0,
        "lateral_offset_m": 1.2,
        "classification": 5,
        "confidence": 95,
    },
}

VALID: dict = {
    "name": "approach-test",
    "cpm_rate_hz": 5.0,
    "duration_s": 12.5,
    "loop": True,
    "sender": {
        "station_id": 1001,
        "lat": 21.028511,
        "lon": 105.804817,
        "heading_deg": 90.0,
    },
    "object": {
        "object_id": 42,
        "initial_distance_m": 60.0,
        "closing_speed_mps": 5.0,
        "lateral_offset_m": 0.5,
        "classification": 5,
        "confidence": 95,
    },
}


def _write(tmp_path: Path, data: Any, name: str = "scenario.yaml") -> Path:
    path = tmp_path / name
    path.write_text(yaml.safe_dump(data), encoding="utf-8")
    return path


def _without(data: dict, dotted: str) -> dict:
    """Deep-copy ``data`` with the dotted key removed."""
    mutated = copy.deepcopy(data)
    *parents, leaf = dotted.split(".")
    node = mutated
    for parent in parents:
        node = node[parent]
    del node[leaf]
    return mutated


def _with(data: dict, dotted: str, value: Any) -> dict:
    """Deep-copy ``data`` with the dotted key replaced by ``value``."""
    mutated = copy.deepcopy(data)
    *parents, leaf = dotted.split(".")
    node = mutated
    for parent in parents:
        node = node[parent]
    node[leaf] = value
    return mutated


# --- valid loads -------------------------------------------------------------------------------


def test_valid_yaml_loads_exact_field_values(tmp_path):
    scenario = load_scenario(_write(tmp_path, VALID))

    assert scenario.name == "approach-test"
    assert scenario.cpm_rate_hz == 5.0
    assert scenario.duration_s == 12.5
    assert scenario.loop is True

    assert scenario.sender.station_id == 1001
    assert scenario.sender.lat == 21.028511
    assert scenario.sender.lon == 105.804817
    assert scenario.sender.heading_deg == 90.0

    assert scenario.object.object_id == 42
    assert scenario.object.initial_distance_m == 60.0
    assert scenario.object.closing_speed_mps == 5.0
    assert scenario.object.lateral_offset_m == 0.5
    assert scenario.object.classification == 5
    assert scenario.object.confidence == 95


def test_cpm_rate_hz_omitted_defaults_to_10(tmp_path):
    scenario = load_scenario(_write(tmp_path, _without(VALID, "cpm_rate_hz")))
    assert scenario.cpm_rate_hz == DEFAULT_CPM_RATE_HZ == 10.0


def test_scenario_config_is_frozen(tmp_path):
    scenario = load_scenario(_write(tmp_path, VALID))
    with pytest.raises(AttributeError):
        scenario.name = "mutated"  # type: ignore[misc]


def test_accepts_path_and_str(tmp_path):
    path = _write(tmp_path, VALID)
    assert load_scenario(str(path)) == load_scenario(path)


# --- rejection: missing keys -------------------------------------------------------------------


@pytest.mark.parametrize(
    "dotted",
    [
        "name",
        "duration_s",
        "loop",
        "sender",
        "object",
        "sender.station_id",
        "sender.lat",
        "sender.lon",
        "sender.heading_deg",
        "object.object_id",
        "object.initial_distance_m",
        "object.closing_speed_mps",
        "object.lateral_offset_m",
        "object.classification",
        "object.confidence",
    ],
)
def test_missing_key_rejected_and_named(tmp_path, dotted):
    with pytest.raises(ValueError, match=dotted.replace(".", r"\.")):
        load_scenario(_write(tmp_path, _without(VALID, dotted)))


# --- rejection: mistyped values ----------------------------------------------------------------


@pytest.mark.parametrize(
    ("dotted", "bad_value"),
    [
        ("name", 42),
        ("loop", "yes"),
        ("loop", 1),
        ("duration_s", True),  # bool is an int subclass - must not pass as a number
        ("cpm_rate_hz", "fast"),
        ("sender", [1, 2, 3]),
        ("object", "car"),
        ("sender.lat", "north"),
        ("sender.station_id", 1.5),
        ("object.object_id", True),  # bool must not pass as an integer
        ("object.classification", "vehicle"),
        ("object.confidence", 0.95),
    ],
)
def test_mistyped_value_rejected_and_named(tmp_path, dotted, bad_value):
    with pytest.raises(ValueError, match=dotted.replace(".", r"\.")):
        load_scenario(_write(tmp_path, _with(VALID, dotted, bad_value)))


# --- rejection: out-of-range values ------------------------------------------------------------


@pytest.mark.parametrize(
    ("dotted", "bad_value"),
    [
        ("cpm_rate_hz", 0),
        ("cpm_rate_hz", -2.5),
        ("duration_s", 0),
        ("duration_s", -1),
    ],
)
def test_non_positive_value_rejected_and_named(tmp_path, dotted, bad_value):
    with pytest.raises(ValueError, match=dotted):
        load_scenario(_write(tmp_path, _with(VALID, dotted, bad_value)))


# --- phases: the D8 three-phase demo-cycle shape ------------------------------------------------


def test_phased_yaml_loads_exact_field_values(tmp_path):
    scenario = load_scenario(_write(tmp_path, PHASED_VALID))

    assert scenario.duration_s is None
    assert scenario.object is None

    assert scenario.phases.waiting_s == 10.0
    assert scenario.phases.two_vehicle_s == 5.0
    assert scenario.phases.three_vehicle_s == 5.0
    assert scenario.phases.cycle_length_s == 20.0

    assert scenario.two_vehicle_object.initial_distance_m == 60.0
    assert scenario.two_vehicle_object.closing_speed_mps == 0.0
    assert scenario.three_vehicle_object.initial_distance_m == 25.0
    assert scenario.three_vehicle_object.closing_speed_mps == 3.0


@pytest.mark.parametrize(
    "dotted",
    [
        "phases.waiting_s",
        "phases.two_vehicle_s",
        "phases.three_vehicle_s",
        "two_vehicle_object",
        "three_vehicle_object",
        "two_vehicle_object.initial_distance_m",
        "three_vehicle_object.closing_speed_mps",
    ],
)
def test_phased_missing_key_rejected_and_named(tmp_path, dotted):
    with pytest.raises(ValueError, match=dotted.replace(".", r"\.")):
        load_scenario(_write(tmp_path, _without(PHASED_VALID, dotted)))


@pytest.mark.parametrize(
    ("dotted", "bad_value"),
    [
        ("phases.waiting_s", 0),
        ("phases.two_vehicle_s", -1.0),
        ("phases.three_vehicle_s", 0),
    ],
)
def test_phased_non_positive_phase_duration_rejected_and_named(tmp_path, dotted, bad_value):
    with pytest.raises(ValueError, match=dotted):
        load_scenario(_write(tmp_path, _with(PHASED_VALID, dotted, bad_value)))


def test_phased_scenario_omits_duration_s_and_object_without_error(tmp_path):
    """A phased YAML carries no top-level ``duration_s``/``object`` - and none is required."""
    scenario = load_scenario(_write(tmp_path, PHASED_VALID))
    assert scenario.duration_s is None
    assert scenario.object is None


# --- rejection: file / document problems -------------------------------------------------------


def test_nonexistent_file_rejected_naming_path(tmp_path):
    missing = tmp_path / "no-such-scenario.yaml"
    with pytest.raises(ValueError, match="no-such-scenario.yaml"):
        load_scenario(missing)


def test_non_mapping_document_rejected_naming_path(tmp_path):
    path = _write(tmp_path, ["not", "a", "mapping"], name="list-doc.yaml")
    with pytest.raises(ValueError, match="list-doc.yaml"):
        load_scenario(path)


def test_unparseable_yaml_rejected_naming_path(tmp_path):
    path = tmp_path / "broken.yaml"
    path.write_text("name: [unclosed", encoding="utf-8")
    with pytest.raises(ValueError, match="broken.yaml"):
        load_scenario(path)


# --- env loading (HLD S5) ----------------------------------------------------------------------


def test_load_env_empty_mapping_gives_blueprint_defaults():
    config = load_env({})
    assert config == EnvConfig()
    assert config.scenario_config == "/app/scenarios/default.yaml"
    assert config.v2x_ecu_host == "10.99.0.11"
    assert config.v2x_ecu_port == 47100
    assert config.encoder_path == "/app/cpm_encode"


def test_load_env_overrides_parsed():
    config = load_env(
        {
            "SCENARIO_CONFIG": "/app/scenarios/c-out-of-range.yaml",
            "V2X_ECU_HOST": "192.0.2.7",
            "V2X_ECU_PORT": "50000",
            "ENCODER_PATH": "/opt/cpm_encode",
        }
    )
    assert config == EnvConfig(
        scenario_config="/app/scenarios/c-out-of-range.yaml",
        v2x_ecu_host="192.0.2.7",
        v2x_ecu_port=50000,
        encoder_path="/opt/cpm_encode",
    )


def test_load_env_partial_override_keeps_other_defaults():
    config = load_env({"V2X_ECU_HOST": "192.0.2.7"})
    assert config.v2x_ecu_host == "192.0.2.7"
    assert config.v2x_ecu_port == 47100
    assert config.scenario_config == "/app/scenarios/default.yaml"
    assert config.encoder_path == "/app/cpm_encode"


@pytest.mark.parametrize("bad_port", ["", "abc", "47100.0", "0", "-5", "65536", "1e4"])
def test_load_env_bad_port_rejected_naming_variable(bad_port):
    with pytest.raises(ValueError, match="V2X_ECU_PORT"):
        load_env({"V2X_ECU_PORT": bad_port})


# --- committed scenario variants (11.1.6.2, SP HLD D3) -------------------------------------------


SCENARIOS_DIR = Path(__file__).resolve().parents[1] / "scenarios"


class TestCommittedScenarioVariants:
    """The two R11-acceptance scenario files load and are observably different by construction.

    This is the **one** place the committed geometry is written as literals: it pins what the demo
    is meant to be, so a retune has to change a test that states the intent rather than one that
    restates the arithmetic. ``test_streams_differ.py`` derives everything from these same files
    and checks the wiring from them to the wire.
    """

    #: R13 admission gate (m) the R22 crossing instant is written against. Frozen with the Phase 2
    #: gate constants; re-checked there per SP HLD section 10 item 3.
    GATE_ENTER_M = 30.0

    def test_default_is_the_r22_approach_geometry(self):
        """default.yaml is the R22 demo cycle (SP D7): C starts at 70,0 m and closes at 5,0 m/s
        over a 10,0 s cycle, so d_BC crosses the 30 m admission gate 8,0 s in - inside R22's open
        interval (7,0 s, 10,0 s) and leaving the run's remaining time for the warning."""
        scenario = load_scenario(SCENARIOS_DIR / "default.yaml")
        assert scenario.object.initial_distance_m == 70.0
        assert scenario.object.closing_speed_mps == 5.0
        assert scenario.duration_s == 10.0

        gate_crossing_s = (
            scenario.object.initial_distance_m - self.GATE_ENTER_M
        ) / scenario.object.closing_speed_mps
        assert gate_crossing_s == pytest.approx(8.0)
        assert 7.0 < gate_crossing_s < scenario.duration_s

    def test_c_out_of_range_is_static_beyond_35m_exit_gate(self):
        scenario = load_scenario(SCENARIOS_DIR / "c-out-of-range.yaml")
        assert scenario.object.closing_speed_mps == 0.0
        # gate_exit pairing re-checked when Phase 2 freezes gate values (SP HLD §10 item 3)
        assert scenario.object.initial_distance_m > 35.0

    def test_variants_differ_in_d3_kinematic_fields(self):
        default = load_scenario(SCENARIOS_DIR / "default.yaml")
        out_of_range = load_scenario(SCENARIOS_DIR / "c-out-of-range.yaml")
        assert (
            default.object.initial_distance_m,
            default.object.closing_speed_mps,
        ) != (
            out_of_range.object.initial_distance_m,
            out_of_range.object.closing_speed_mps,
        )

    def test_r19_demo_cycle_period_is_a_multiple_of_the_10s_ada_clip(self):
        """SP HLD D8: the cycle length must be an integer multiple of the 10.0 s ADA video clip
        (ego-b-occluding-c.mp4), or the clip's independent loop drifts out of phase with the bench
        and wraps mid-warning (D7/D11's `b_unknown` regression)."""
        scenario = load_scenario(SCENARIOS_DIR / "r19-demo-cycle.yaml")
        ada_clip_length_s = 10.0
        assert scenario.phases.cycle_length_s % ada_clip_length_s == 0

    def test_r19_demo_cycle_two_vehicle_object_stays_beyond_the_gate(self):
        scenario = load_scenario(SCENARIOS_DIR / "r19-demo-cycle.yaml")
        assert scenario.two_vehicle_object.closing_speed_mps == 0.0
        assert scenario.two_vehicle_object.initial_distance_m > self.GATE_ENTER_M

    def test_r19_demo_cycle_three_vehicle_object_stays_inside_the_gate_throughout(self):
        scenario = load_scenario(SCENARIOS_DIR / "r19-demo-cycle.yaml")
        obj = scenario.three_vehicle_object
        farthest_m = obj.initial_distance_m  # closing speed only decreases distance from here
        assert farthest_m < self.GATE_ENTER_M
