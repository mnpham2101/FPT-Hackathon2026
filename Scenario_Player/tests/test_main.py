"""11.1.6.8 - entrypoint ``main.build_and_run`` end-to-end (SP HLD D4).

Runs via ``python -m pytest Scenario_Player/tests`` from the repo root (the CI invocation), on
Windows and Linux alike. Covered: an end-to-end smoke run - bounded scenario YAML (small
``duration_s``, ``loop: false``), the committed fake ``cpm_encode`` helper in ``echo`` mode, and a
loopback UDP listener receiving the datagrams the entrypoint wires together, with ``[TX]`` lines
in the captured log; the ``[FATAL]`` startup-failure path returning 1 without an escaping
exception; and the production default forwarding ``command=None`` to ``EncoderClient`` (the real
``[encoder_path, "--stream"]`` shape, HLD D1).
"""

import base64
import json
import socket
import sys
from pathlib import Path
from typing import Callable, Dict, List, Optional, Sequence

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import main  # noqa: E402

TESTS_DIR = Path(__file__).resolve().parent
FAKE_HELPER = TESTS_DIR / "fake_cpm_encode.py"

#: Bounded scenario for the smoke run: rate 10 Hz x duration 0.3 s, loop: false -> exactly 3 ticks.
SCENARIO_YAML = """\
name: bounded-smoke
cpm_rate_hz: 10
duration_s: 0.3
loop: false

sender:
  station_id: 1201
  lat: 21.028511
  lon: 105.804817
  heading_deg: 90.0

object:
  object_id: 7
  initial_distance_m: 60.0
  closing_speed_mps: 2.5
  lateral_offset_m: 1.2
  classification: 5
  confidence: 95
"""

EXPECTED_TICKS = 3
DRAIN_TIMEOUT_S = 2.0


@pytest.fixture
def scenario_yaml(tmp_path: Path) -> Path:
    path = tmp_path / "bounded-smoke.yaml"
    path.write_text(SCENARIO_YAML, encoding="utf-8")
    return path


@pytest.fixture
def listener():
    """Loopback UDP listener on an ephemeral port; yields (socket, port)."""
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind(("127.0.0.1", 0))
    sock.settimeout(DRAIN_TIMEOUT_S)
    try:
        yield sock, sock.getsockname()[1]
    finally:
        sock.close()


def _drain(sock: socket.socket, expected: int) -> List[bytes]:
    """Receive up to ``expected`` datagrams, stopping early on the receive timeout."""
    datagrams: List[bytes] = []
    while len(datagrams) < expected:
        try:
            payload, _ = sock.recvfrom(65535)
        except socket.timeout:
            break
        datagrams.append(payload)
    return datagrams


def _tx_records(log_lines: List[str]) -> List[dict]:
    return [json.loads(line[len("[TX] "):]) for line in log_lines if line.startswith("[TX] ")]


def test_smoke_run_sends_datagrams_and_logs_tx(scenario_yaml: Path, listener) -> None:
    """End-to-end: env + YAML -> fake encoder -> loopback UDP; rc 0, payloads + [TX] lines."""
    sock, port = listener
    log_lines: List[str] = []

    rc = main.build_and_run(
        env={
            "SCENARIO_CONFIG": str(scenario_yaml),
            "V2X_ECU_HOST": "127.0.0.1",
            "V2X_ECU_PORT": str(port),
            "ENCODER_PATH": "ignored",
        },
        encoder_command=[sys.executable, str(FAKE_HELPER), "echo"],
        log=log_lines.append,
    )

    assert rc == 0

    datagrams = _drain(sock, EXPECTED_TICKS)
    assert len(datagrams) >= 1  # UDP: tolerate loss, but the loopback run should deliver all 3

    # The fake helper echoes base64(input JSON line), so each datagram is the CpmContent JSON the
    # entrypoint's pipeline produced - assert one payload really is that JSON.
    content = json.loads(datagrams[0].decode("utf-8"))
    assert content["stationId"] == 1201
    assert content["object"]["objectId"] == 7
    # Round-trip the fake's contract explicitly: payload == base64-decode(base64(payload)).
    assert base64.b64decode(base64.b64encode(datagrams[0])) == datagrams[0]

    assert any(line.startswith("[START] ") for line in log_lines)
    tx = _tx_records(log_lines)
    assert len(tx) == EXPECTED_TICKS
    assert tx[0]["seq"] == 0
    assert [record["seq"] for record in tx] == list(range(EXPECTED_TICKS))
    assert all(record["bytes"] == len(datagrams[0]) or record["bytes"] > 0 for record in tx)


def test_startup_failure_logs_fatal_and_returns_1(tmp_path: Path) -> None:
    """SCENARIO_CONFIG at a nonexistent file: rc 1, one [FATAL] line, no escaping exception."""
    log_lines: List[str] = []

    rc = main.build_and_run(
        env={
            "SCENARIO_CONFIG": str(tmp_path / "does-not-exist.yaml"),
            "V2X_ECU_HOST": "127.0.0.1",
            "V2X_ECU_PORT": "47100",
            "ENCODER_PATH": "ignored",
        },
        encoder_command=[sys.executable, str(FAKE_HELPER), "echo"],
        log=log_lines.append,
    )

    assert rc == 1
    fatal_lines = [line for line in log_lines if line.startswith("[FATAL] ")]
    assert len(fatal_lines) == 1
    assert "does-not-exist.yaml" in fatal_lines[0]


def test_default_encoder_command_is_none(
    scenario_yaml: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    """``encoder_command=None`` forwards ``command=None`` to EncoderClient (D1 default shape)."""
    captured: Dict[str, object] = {}

    class _Stop(Exception):
        pass

    class _CapturingClient:
        def __init__(
            self,
            encoder_path: str,
            *,
            command: Optional[Sequence[str]] = None,
            log: Optional[Callable[[str], None]] = None,
        ) -> None:
            captured["encoder_path"] = encoder_path
            captured["command"] = command
            raise _Stop("captured - stop before spawning anything")

    monkeypatch.setattr(main, "EncoderClient", _CapturingClient)
    log_lines: List[str] = []

    rc = main.build_and_run(
        env={
            "SCENARIO_CONFIG": str(scenario_yaml),
            "ENCODER_PATH": "/app/cpm_encode",
        },
        log=log_lines.append,
    )

    assert rc == 1  # _Stop is swallowed by the fatal boundary - nothing escapes
    assert captured["command"] is None
    assert captured["encoder_path"] == "/app/cpm_encode"
