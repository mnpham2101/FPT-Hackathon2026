"""1.0.5.1 - bench R1 CpmContent binding round-trip + schema validation over all golden vectors.

Runs via ``python -m pytest Scenario_Player/tests`` from the repo root (the CI invocation).
Reads only node-local paths (``Scenario_Player/contracts``, ``Scenario_Player/tests/fixtures/golden``)
so the node folder stays a self-contained Docker build context. The ``.uper`` side of the golden
vectors is deliberately not consumed here: no UPER/ASN.1 codec exists on the bench (open item F3).
"""

import json
import sys
from pathlib import Path

import jsonschema
import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from player.contracts.cpm_content import (  # noqa: E402
    CpmContent,
    from_dict,
    from_json,
    to_dict,
    to_json,
)

PLAYER = Path(__file__).resolve().parents[1]
GOLDEN = PLAYER / "tests" / "fixtures" / "golden"
SCHEMA_PATH = PLAYER / "contracts" / "r1-cpm-content.schema.json"

CASES = ["nominal", "mdt-max", "mdt-min", "conf-unavailable", "gate-boundary", "coord-large"]


def _load(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


@pytest.mark.parametrize("case", CASES)
def test_golden_dict_roundtrip(case):
    """from_dict -> to_dict is key-for-key identity against the frozen golden vector."""
    original = _load(GOLDEN / f"{case}.json")

    content = from_dict(original)
    assert isinstance(content, CpmContent)
    assert to_dict(content) == original


@pytest.mark.parametrize("case", CASES)
def test_golden_json_text_roundtrip(case):
    """from_json -> to_json re-parses to the same dict, and re-decodes to an equal dataclass."""
    path = GOLDEN / f"{case}.json"
    original = _load(path)

    content = from_json(path.read_text(encoding="utf-8"))
    text = to_json(content)

    assert json.loads(text) == original
    assert from_json(text) == content


def test_schema_itself_is_valid():
    jsonschema.Draft202012Validator.check_schema(_load(SCHEMA_PATH))


@pytest.mark.parametrize("case", CASES)
def test_golden_validates_against_schema(case):
    schema = _load(SCHEMA_PATH)
    instance = _load(GOLDEN / f"{case}.json")
    jsonschema.Draft202012Validator(schema).validate(instance)


def test_wire_native_edge_values():
    """Spot-check that the edge cases really carry wire-native integers, unconverted."""
    assert from_dict(_load(GOLDEN / "nominal.json")).object.measurementDeltaTime == -50
    assert from_dict(_load(GOLDEN / "mdt-max.json")).object.measurementDeltaTime == 2047
    assert from_dict(_load(GOLDEN / "mdt-min.json")).object.measurementDeltaTime == -2047
    assert from_dict(_load(GOLDEN / "conf-unavailable.json")).object.classConfidence == 101

    gate = from_dict(_load(GOLDEN / "gate-boundary.json"))
    assert (gate.object.position.x, gate.object.position.y) == (3000, 0)

    large = from_dict(_load(GOLDEN / "coord-large.json"))
    assert (large.object.position.x, large.object.position.y) == (131071, -131072)


def test_unknown_keys_are_ignored():
    """Additive evolution: extra wire keys decode without error and do not survive re-encoding."""
    original = _load(GOLDEN / "nominal.json")
    extended = dict(original, futureField=42)
    extended["object"] = dict(original["object"], futureObjectField="x")

    assert to_dict(from_dict(extended)) == original
