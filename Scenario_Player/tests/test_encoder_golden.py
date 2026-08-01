"""11.1.7.2 - wire truth of SP HLD D2: ``cpm_encode(golden .json) == golden .uper``, byte-for-byte.

D2 catches codec drift twice. ``contracts/check_sync.py`` proves the *copies* are byte-identical to
their masters; this module proves the *encoder* still produces those exact bytes. A synced copy that
matches its master while the codec has moved underneath it would pass the first gate and fail here -
that is the whole point of the second check.

Two kinds of test live here, deliberately gated differently:

* **fixture identity** (``test_golden_uper_matches_master``) - pure Python, no binary, so it runs on
  every local and CI invocation. It duplicates, node-locally, the ``.uper`` rows the sync manifest
  gains in 11.1.1.2; the redundancy is cheap and keeps the fixture honest even if the manifest row
  is ever dropped. This is the one test here that reads the repo-root ``contracts/`` masters, which
  is safe because ``tests/`` never enters the image (SP HLD §8) and the sanctioned invocation is
  ``python -m pytest Scenario_Player/tests`` from the repo root.
* **encoder equality** (``test_encoder_matches_golden_uper``, ``test_encoder_rejects_invalid_json``)
  - needs the compiled ``cpm_encode`` (SP HLD D1). It is skipped per-test, not per-module, so the
  absence of the binary never hides the fixture-identity check. Locally the binary is normally
  absent and these skip; the CI ``sp-codec-helper`` lane (11.1.8.1) builds the helper, exports
  ``ENCODER_PATH``, and runs them for real.

The helper is exercised through its one-shot ``--encode <file>`` mode (``codec_helper/src/main.cpp``)
rather than through ``player/encoder_client.py``: this is a statement about the *binary's* output,
so nothing of the Python client sits between the codec and the assertion.
"""

import base64
import os
import subprocess
from pathlib import Path
from typing import Optional

import pytest

PLAYER = Path(__file__).resolve().parents[1]
REPO_ROOT = PLAYER.parent
GOLDEN = PLAYER / "tests" / "fixtures" / "golden"
MASTER_GOLDEN = REPO_ROOT / "contracts" / "golden-vectors"

#: The six frozen Phase 0 vectors, sorted so parametrized ids are deterministic across runs.
CASES = sorted(
    ["nominal", "mdt-max", "mdt-min", "conf-unavailable", "gate-boundary", "coord-large"]
)

#: Bound on one one-shot encode. Generous (a cold start of the Vanetza-linked binary on a loaded CI
#: runner is still far below this) but finite, so a hung helper fails the run instead of the job.
ENCODE_TIMEOUT_S = 30


def _existing_binary(candidate: Path) -> Optional[Path]:
    """``candidate``, or its ``.exe`` sibling, if either is a file; otherwise ``None``."""
    for path in (candidate, candidate.parent / (candidate.name + ".exe")):
        if path.is_file():
            return path
    return None


def _locate_encoder() -> Optional[Path]:
    """Find the ``cpm_encode`` binary, mirroring the ``ENCODER_PATH`` convention of SP HLD §5.

    Resolution order:

    1. ``ENCODER_PATH`` - the same override the runtime client uses (``player/config.py``); this is
       what the CI lane sets to the freshly built binary.
    2. ``codec_helper/build/cpm_encode`` - the local build path pinned by
       ``codec_helper/CMakeLists.txt`` (``RUNTIME_OUTPUT_DIRECTORY``), so a developer who has built
       the helper gets these tests unskipped with no environment setup.

    A ``.exe`` sibling is accepted at both steps: harmless on Linux, and it keeps the tests usable
    if the helper is ever built on a Windows host.
    """
    override = os.environ.get("ENCODER_PATH")
    if override:
        found = _existing_binary(Path(override))
        if found is not None:
            return found
    return _existing_binary(PLAYER / "codec_helper" / "build" / "cpm_encode")


#: Resolved once at import: every test in the module agrees on one binary (or on its absence).
ENCODER = _locate_encoder()

requires_encoder = pytest.mark.skipif(
    ENCODER is None,
    reason=(
        "cpm_encode binary not built (see codec_helper/CMakeLists.txt; "
        "CI lane sp-codec-helper builds it and sets ENCODER_PATH)"
    ),
)


def _first_difference(actual: bytes, expected: bytes) -> Optional[int]:
    """Offset of the first differing byte, or of the truncation point; ``None`` if equal."""
    for index in range(min(len(actual), len(expected))):
        if actual[index] != expected[index]:
            return index
    if len(actual) == len(expected):
        return None
    return min(len(actual), len(expected))


def _mismatch_report(case: str, actual: bytes, expected: bytes) -> str:
    """Actionable diff for a codec regression: lengths, first differing offset, both hex dumps.

    A bare "bytes differ" is useless when debugging an ASN.1 encoder - the offset localizes the
    change to a field, and the vectors are 58 bytes, so dumping both in full costs nothing.
    """
    offset = _first_difference(actual, expected)
    lines = [
        f"{case}: cpm_encode output does not match the golden .uper",
        f"  expected {len(expected)} bytes, got {len(actual)} bytes",
    ]
    if offset is None:
        lines.append("  no differing byte found (length mismatch only)")
    elif offset >= len(actual) or offset >= len(expected):
        lines.append(f"  identical up to byte {offset}, then one side ends (truncated output)")
    else:
        lines.append(
            f"  first difference at offset {offset}: "
            f"expected 0x{expected[offset]:02x}, got 0x{actual[offset]:02x}"
        )
    lines.append(f"  expected hex: {expected.hex()}")
    lines.append(f"  actual   hex: {actual.hex()}")
    return "\n".join(lines)


def _run_encode(json_path: Path) -> "subprocess.CompletedProcess[bytes]":
    """One ``cpm_encode --encode <file>`` invocation; bytes captured, never decoded as text."""
    assert ENCODER is not None  # guarded by requires_encoder
    return subprocess.run(
        [str(ENCODER), "--encode", str(json_path)],
        capture_output=True,
        timeout=ENCODE_TIMEOUT_S,
    )


def _process_report(label: str, completed: "subprocess.CompletedProcess[bytes]") -> str:
    """Both channels in the failure message: stdout carries the {"error":...} line, stderr the text."""
    return (
        f"{label}: exit {completed.returncode}\n"
        f"  stdout: {completed.stdout!r}\n"
        f"  stderr: {completed.stderr!r}"
    )


# --- fixture identity (no binary needed) -----------------------------------------------------------


@pytest.mark.parametrize("case", CASES)
def test_golden_uper_matches_master(case):
    """The synced ``.uper`` fixture is byte-identical to its ``contracts/golden-vectors/`` master."""
    target = GOLDEN / f"{case}.uper"
    master = MASTER_GOLDEN / f"{case}.uper"

    assert master.is_file(), f"master vector missing: {master} (run pytest from the repo root)"
    assert target.is_file(), f"synced vector missing: {target}"

    master_bytes = master.read_bytes()
    target_bytes = target.read_bytes()
    assert target_bytes == master_bytes, _mismatch_report(
        f"{case}.uper sync", target_bytes, master_bytes
    )


# --- encoder equality (needs the built helper) -----------------------------------------------------


@requires_encoder
@pytest.mark.parametrize("case", CASES)
def test_encoder_matches_golden_uper(case):
    """``cpm_encode --encode <case>.json`` decodes to exactly the bytes of ``<case>.uper`` (D2)."""
    json_path = GOLDEN / f"{case}.json"
    expected = (GOLDEN / f"{case}.uper").read_bytes()

    completed = _run_encode(json_path)
    assert completed.returncode == 0, _process_report(f"{case}: encode failed", completed)

    # The D1 reply is one bare base64 line; strip only the framing whitespace, then let
    # validate=True reject any stray character rather than silently discarding it.
    payload = base64.b64decode(completed.stdout.strip(), validate=True)
    assert payload == expected, _mismatch_report(case, payload, expected)


@requires_encoder
def test_encoder_rejects_invalid_json(tmp_path):
    """The failure contract the CI lane relies on: non-zero exit plus an ``{"error":...}`` line."""
    invalid = tmp_path / "invalid-cpm-content.json"
    invalid.write_text('{"stationId": ', encoding="utf-8")

    completed = _run_encode(invalid)
    assert completed.returncode != 0, _process_report(
        "invalid JSON was accepted", completed
    )
    assert b'{"error"' in completed.stdout, _process_report(
        "no error line on stdout", completed
    )
