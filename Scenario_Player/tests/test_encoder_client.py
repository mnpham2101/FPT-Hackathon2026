"""11.1.6.5 - ``EncoderClient`` against the committed fake helper (SP HLD D1/D4).

Runs via ``python -m pytest Scenario_Player/tests`` from the repo root (the CI invocation), on
Windows and Linux alike: the fake ``fake_cpm_encode.py`` is always launched through
``sys.executable`` via the client's full-argv ``command`` override - no real ``cpm_encode`` binary
needed. Covered: echo-success persistence (two encodes, one process), an ``{"error": ...}`` reply
surfacing as ``EncodeError`` while the stream survives (D1), helper death -> logged restart with
exactly one injected backoff and only the in-flight message lost (D4), and ``close()`` /
context-manager teardown leaving no child behind.
"""

import dataclasses
import sys
from pathlib import Path
from typing import List

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from player.contracts.cpm_content import CpmContent, from_json, to_json  # noqa: E402
from player.encoder_client import EncodeError, EncoderClient  # noqa: E402

TESTS_DIR = Path(__file__).resolve().parent
FAKE_HELPER = TESTS_DIR / "fake_cpm_encode.py"
GOLDEN_NOMINAL = TESTS_DIR / "fixtures" / "golden" / "nominal.json"

#: fake_cpm_encode.ERROR_MARKER_STATION_ID - the stationId that provokes the error reply.
ERROR_MARKER_STATION_ID = 999

#: Arbitrary backoff handed to the client; asserted against the recorded sleeper, never slept.
BACKOFF_S = 0.25


@pytest.fixture
def nominal_content() -> CpmContent:
    return from_json(GOLDEN_NOMINAL.read_text(encoding="utf-8"))


@pytest.fixture
def recorded_sleeps() -> List[float]:
    return []


@pytest.fixture
def log_lines() -> List[str]:
    return []


@pytest.fixture
def make_client(recorded_sleeps: List[float], log_lines: List[str]):
    """Factory for clients driving the fake helper in ``mode``; closes them all on teardown."""
    clients: List[EncoderClient] = []

    def factory(mode: str) -> EncoderClient:
        client = EncoderClient(
            "unused-encoder-path",
            command=[sys.executable, str(FAKE_HELPER), mode],
            restart_backoff_s=BACKOFF_S,
            sleep=recorded_sleeps.append,
            log=log_lines.append,
        )
        clients.append(client)
        return client

    yield factory
    for client in clients:
        client.close()


def _expected_payload(content: CpmContent) -> bytes:
    """Echo mode replies base64(JSON line), so the decoded payload is the JSON line's UTF-8."""
    return to_json(content).encode("utf-8")


# --- (a) echo-success + persistence ---------------------------------------------------------------


def test_echo_success_two_encodes_same_process(make_client, nominal_content):
    client = make_client("echo")
    assert client.encode(nominal_content) == _expected_payload(nominal_content)
    pid = client.pid
    assert pid is not None
    assert client.encode(nominal_content) == _expected_payload(nominal_content)
    assert client.pid == pid  # persistent subprocess, not one spawn per encode


def test_default_command_is_encoder_path_stream():
    """Without an override, the real helper argv shape is ``[encoder_path, "--stream"]`` (D1)."""
    client = EncoderClient("/app/cpm_encode")
    assert client._command == ["/app/cpm_encode", "--stream"]  # never spawned - no cleanup needed


# --- (b) error-line: typed failure, stream survives ------------------------------------------------


def test_error_reply_raises_but_stream_survives(make_client, nominal_content, log_lines):
    client = make_client("error-on-marker")
    assert client.encode(nominal_content) == _expected_payload(nominal_content)
    pid = client.pid

    poisoned = dataclasses.replace(nominal_content, stationId=ERROR_MARKER_STATION_ID)
    with pytest.raises(EncodeError, match="boom"):
        client.encode(poisoned)
    assert any("boom" in line for line in log_lines)

    # HLD D1: the error reply never kills the stream - same process serves the next encode.
    assert client.encode(nominal_content) == _expected_payload(nominal_content)
    assert client.pid == pid


# --- (c) death-restart with backoff ----------------------------------------------------------------


def test_death_restarts_with_one_backoff_and_new_process(
    make_client, nominal_content, recorded_sleeps, log_lines
):
    client = make_client("die-after:1")
    assert client.encode(nominal_content) == _expected_payload(nominal_content)
    first_pid = client.pid

    # Helper exited after the first echo: this encode fails, is logged, and backs off once.
    with pytest.raises(EncodeError):
        client.encode(nominal_content)
    assert recorded_sleeps == [BACKOFF_S]
    assert any("restarting after" in line for line in log_lines)

    # HLD D4: only the in-flight message is lost - the respawned helper serves the next encode.
    assert client.encode(nominal_content) == _expected_payload(nominal_content)
    assert client.pid != first_pid


# --- (d) close() + context manager -----------------------------------------------------------------


def test_close_terminates_child(make_client, nominal_content):
    client = make_client("echo")
    client.encode(nominal_content)
    process = client._process
    assert process is not None
    client.close()
    assert process.poll() is not None  # child reaped, no zombie
    assert client.pid is None


def test_context_manager_closes_child(nominal_content, recorded_sleeps, log_lines):
    with EncoderClient(
        "unused-encoder-path",
        command=[sys.executable, str(FAKE_HELPER), "echo"],
        restart_backoff_s=BACKOFF_S,
        sleep=recorded_sleeps.append,
        log=log_lines.append,
    ) as client:
        assert client.encode(nominal_content) == _expected_payload(nominal_content)
        process = client._process
        assert process is not None
    assert process.poll() is not None
    assert client.pid is None
