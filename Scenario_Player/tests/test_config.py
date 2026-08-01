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
