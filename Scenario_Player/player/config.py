"""Env + scenario-YAML loading/validation for the Scenario Player (bench).

This module is the bench's **only env reader** (SP HLD D3/S5): every runtime tunable enters either
through ``load_env`` (deployment wiring - blueprint-injected env vars, with the blueprint values as
code defaults) or through ``load_scenario`` (scenario content - the declarative YAML shape of HLD
D3). No other module reads ``os.environ``, and no scenario tunable lives as a code literal.

Validation rule - fail loud, fail named: every missing key, mistyped value (bool-vs-number pitfalls
included), non-positive ``cpm_rate_hz``/``duration_s``, or unreadable file raises ``ValueError``
whose message names the offending key (dotted path for nested keys) or file path. No clamping, no
silent defaults beyond the ones designated in HLD S5/F8.

PyYAML (``yaml.safe_load``) + stdlib; Python 3.11-compatible.
"""

import os
from collections.abc import Mapping
from dataclasses import dataclass
from pathlib import Path
from typing import Any

import yaml

#: F8 - profiled CPM generation rate used when the scenario YAML omits ``cpm_rate_hz``.
DEFAULT_CPM_RATE_HZ: float = 10.0


@dataclass(frozen=True)
class EnvConfig:
    """Deployment wiring from env (HLD S5); defaults are the blueprint node-config values."""

    scenario_config: str = "/app/scenarios/default.yaml"
    v2x_ecu_host: str = "10.99.0.11"
    v2x_ecu_port: int = 47100
    encoder_path: str = "/app/cpm_encode"


@dataclass(frozen=True)
class SenderConfig:
    """Sender B's static WGS84 pose (scenario YAML ``sender``), SI/degree units."""

    station_id: int
    lat: float
    lon: float
    heading_deg: float


@dataclass(frozen=True)
class ObjectConfig:
    """Object C's relative state and class (scenario YAML ``object``), SI units.

    ``classification``/``confidence`` mirror the exact types of ``PerceivedObject.classification``/
    ``classConfidence`` in ``player/contracts/cpm_content.py`` so kinematics maps them 1:1.
    """

    object_id: int
    initial_distance_m: float
    closing_speed_mps: float
    lateral_offset_m: float
    classification: int
    confidence: int


@dataclass(frozen=True)
class ScenarioConfig:
    """One validated scenario (HLD D3): name, timing, and the sender/object tunables."""

    name: str
    duration_s: float
    loop: bool
    sender: SenderConfig
    object: ObjectConfig
    cpm_rate_hz: float = DEFAULT_CPM_RATE_HZ


def load_env(env: Mapping[str, str] | None = None) -> EnvConfig:
    """Read the HLD S5 env set from ``env`` (default ``os.environ``); injectable for tests.

    ``V2X_ECU_PORT`` is parsed strictly: a base-10 integer in 1-65535, else ``ValueError``
    naming the variable.
    """
    source: Mapping[str, str] = os.environ if env is None else env
    defaults = EnvConfig()

    port_raw = source.get("V2X_ECU_PORT")
    if port_raw is None:
        port = defaults.v2x_ecu_port
    else:
        try:
            port = int(port_raw)
        except ValueError:
            raise ValueError(
                f"env var V2X_ECU_PORT must be an integer, got {port_raw!r}"
            ) from None
        if not 1 <= port <= 65535:
            raise ValueError(f"env var V2X_ECU_PORT must be in 1-65535, got {port}")

    return EnvConfig(
        scenario_config=source.get("SCENARIO_CONFIG", defaults.scenario_config),
        v2x_ecu_host=source.get("V2X_ECU_HOST", defaults.v2x_ecu_host),
        v2x_ecu_port=port,
        encoder_path=source.get("ENCODER_PATH", defaults.encoder_path),
    )


def load_scenario(path: str | Path) -> ScenarioConfig:
    """Load and validate one scenario YAML (HLD D3 shape) into a ``ScenarioConfig``.

    Raises ``ValueError`` naming the offending key for every missing/mistyped/out-of-range value,
    and naming the path when the file is missing, unreadable, or not valid YAML.
    """
    scenario_path = Path(path)
    try:
        text = scenario_path.read_text(encoding="utf-8")
    except OSError as exc:
        raise ValueError(f"cannot read scenario config '{scenario_path}': {exc}") from exc

    try:
        raw = yaml.safe_load(text)
    except yaml.YAMLError as exc:
        raise ValueError(f"invalid YAML in scenario config '{scenario_path}': {exc}") from exc

    if not isinstance(raw, Mapping):
        raise ValueError(
            f"scenario config '{scenario_path}' must be a YAML mapping,"
            f" got {type(raw).__name__}"
        )

    return _parse_scenario(raw)


def _parse_scenario(raw: Mapping[str, Any]) -> ScenarioConfig:
    sender_raw = _require_mapping(raw, "sender")
    object_raw = _require_mapping(raw, "object")

    if "cpm_rate_hz" in raw:
        cpm_rate_hz = _positive_number(raw["cpm_rate_hz"], "cpm_rate_hz")
    else:
        cpm_rate_hz = DEFAULT_CPM_RATE_HZ

    return ScenarioConfig(
        name=_string(_require(raw, "name"), "name"),
        cpm_rate_hz=cpm_rate_hz,
        duration_s=_positive_number(_require(raw, "duration_s"), "duration_s"),
        loop=_boolean(_require(raw, "loop"), "loop"),
        sender=SenderConfig(
            station_id=_integer(_require(sender_raw, "station_id", "sender."), "sender.station_id"),
            lat=_number(_require(sender_raw, "lat", "sender."), "sender.lat"),
            lon=_number(_require(sender_raw, "lon", "sender."), "sender.lon"),
            heading_deg=_number(_require(sender_raw, "heading_deg", "sender."), "sender.heading_deg"),
        ),
        object=ObjectConfig(
            object_id=_integer(_require(object_raw, "object_id", "object."), "object.object_id"),
            initial_distance_m=_number(
                _require(object_raw, "initial_distance_m", "object."), "object.initial_distance_m"
            ),
            closing_speed_mps=_number(
                _require(object_raw, "closing_speed_mps", "object."), "object.closing_speed_mps"
            ),
            lateral_offset_m=_number(
                _require(object_raw, "lateral_offset_m", "object."), "object.lateral_offset_m"
            ),
            classification=_integer(
                _require(object_raw, "classification", "object."), "object.classification"
            ),
            confidence=_integer(_require(object_raw, "confidence", "object."), "object.confidence"),
        ),
    )


def _require(mapping: Mapping[str, Any], key: str, prefix: str = "") -> Any:
    if key not in mapping:
        raise ValueError(f"scenario config: missing required key '{prefix}{key}'")
    return mapping[key]


def _require_mapping(raw: Mapping[str, Any], key: str) -> Mapping[str, Any]:
    value = _require(raw, key)
    if not isinstance(value, Mapping):
        raise ValueError(
            f"scenario config: key '{key}' must be a mapping, got {type(value).__name__}"
        )
    return value


def _string(value: Any, key: str) -> str:
    if not isinstance(value, str):
        raise ValueError(
            f"scenario config: key '{key}' must be a string, got {type(value).__name__}"
        )
    return value


def _boolean(value: Any, key: str) -> bool:
    if not isinstance(value, bool):
        raise ValueError(
            f"scenario config: key '{key}' must be a boolean, got {type(value).__name__}"
        )
    return value


def _integer(value: Any, key: str) -> int:
    # bool is an int subclass in Python - reject it explicitly (YAML `true` is not an id/count).
    if isinstance(value, bool) or not isinstance(value, int):
        raise ValueError(
            f"scenario config: key '{key}' must be an integer, got {type(value).__name__}"
        )
    return value


def _number(value: Any, key: str) -> float:
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise ValueError(
            f"scenario config: key '{key}' must be a number, got {type(value).__name__}"
        )
    return float(value)


def _positive_number(value: Any, key: str) -> float:
    number = _number(value, key)
    if number <= 0:
        raise ValueError(f"scenario config: key '{key}' must be > 0, got {value}")
    return number
