"""3.0.4.5 — detector R3 JSONL binding round-trip + Python-side validation of all shared samples.

Runs via ``python -m pytest ADA_ECU/detector/tests`` from the repo root (the CI invocation).
Schema validation here covers every node-local sample against every node-local schema — the
one-language-validates-for-all rule (HLD D2), so the C++/Kotlin bindings need no validator of their own.
"""

import json
import sys
from pathlib import Path

import jsonschema
import pytest
import referencing

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from contracts.tracked_object import from_dict, from_jsonl, to_jsonl  # noqa: E402

ADA = Path(__file__).resolve().parents[2]
SAMPLES = ADA / "tests" / "fixtures" / "samples"
SCHEMAS = ADA / "contracts"


def _load(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def test_r3_jsonl_roundtrip():
    original = _load(SAMPLES / "r3-tracked-object.json")

    obj = from_dict(original)
    assert obj.object_class == "vehicle"
    assert obj.source == "v2x_relayed"

    line = to_jsonl(obj)
    assert "\n" not in line

    assert from_jsonl(line) == obj
    assert from_jsonl(line + "\n") == obj
    assert json.loads(line) == original


# (sample file, schema file) — all five node-local samples vs the three node-local schemas.
VALIDATION_CASES = [
    ("r2-object.json", "r2-v2x-object.schema.json"),
    ("r3-tracked-object.json", "r3-tracked-object.schema.json"),
    ("r4-warning.json", "r4-ada-ivi.schema.json"),
    ("r4-state.json", "r4-ada-ivi.schema.json"),
    ("r4-unknown-warning.json", "r4-ada-ivi.schema.json"),
]


def _registry() -> referencing.Registry:
    """Registry resolving the r4 schema's ``$ref`` to the r3 schema by its ``$id``."""
    r3_schema = _load(SCHEMAS / "r3-tracked-object.schema.json")
    return referencing.Registry().with_resource(
        "r3-tracked-object.schema.json",
        referencing.Resource.from_contents(r3_schema),
    )


@pytest.mark.parametrize(("sample_name", "schema_name"), VALIDATION_CASES)
def test_sample_validates_against_schema(sample_name, schema_name):
    schema = _load(SCHEMAS / schema_name)
    instance = _load(SAMPLES / sample_name)
    validator = jsonschema.Draft202012Validator(schema, registry=_registry())
    validator.validate(instance)
