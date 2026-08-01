"""11.1.6.7 - rate-loop generator: cadence, loop/duration semantics, ``[TX]`` line shape.

Runs via ``python -m pytest Scenario_Player/tests`` from the repo root (the CI invocation).
The loop is driven entirely by fakes (fixed ``now_ms``, recorded ``sleep``, captured ``log``,
predictable encode bytes, len-returning send) so every assertion is deterministic: tick count and
scenario times against the ``cpm_rate_hz``/``duration_s`` arithmetic, ``loop: true`` restarting
scenario time while the global seq keeps counting, ``loop: false`` exiting on its own, the
``[TX]`` JSONL shape (exactly ``{seq, scenario_time_s, bytes}``), and the ``EncodeError`` skip
path (one ``[ENC-SKIP]``, no send, loop survives - SP HLD D4).
"""

import json
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from player.config import ObjectConfig, ScenarioConfig, SenderConfig  # noqa: E402
from player.contracts.cpm_content import CpmContent  # noqa: E402
from player.encoder_client import EncodeError  # noqa: E402
from player.generator import Generator  # noqa: E402
from player.scenario import Scenario  # noqa: E402

#: Arbitrary valid TimestampIts, injected as the fixed ``now_ms`` - never advances in tests.
FIXED_NOW_MS = 716084805123

#: Predictable fake encoder output; ``[TX] bytes`` must equal its length (send returns len).
FAKE_PAYLOAD = b"\x0e\x00\x9cUPER"


def _config(cpm_rate_hz: float, duration_s: float, loop: bool) -> ScenarioConfig:
    """A scenario config with the loop tunables under test; kinematics from default.yaml."""
    return ScenarioConfig(
        name="generator-test",
        cpm_rate_hz=cpm_rate_hz,
        duration_s=duration_s,
        loop=loop,
        sender=SenderConfig(station_id=1201, lat=21.028511, lon=105.804817, heading_deg=90.0),
        object=ObjectConfig(
            object_id=7,
            initial_distance_m=60.0,
            closing_speed_mps=2.5,
            lateral_offset_m=1.2,
            classification=5,
            confidence=95,
        ),
    )


class RecordingScenario(Scenario):
    """Real kinematics, plus a record of every ``(t, reference_time_ms)`` sample call."""

    def __init__(self, config: ScenarioConfig) -> None:
        super().__init__(config)
        self.calls: list[tuple[float, int]] = []

    def sample(
        self, t: float, reference_time_ms: int, measurement_delta_ms: int = 0
    ) -> CpmContent:
        self.calls.append((t, reference_time_ms))
        return super().sample(t, reference_time_ms, measurement_delta_ms)


class Harness:
    """One generator wired to fakes: recorded sends/sleeps, captured log, fixed clock."""

    def __init__(
        self,
        cpm_rate_hz: float,
        duration_s: float,
        loop: bool,
        *,
        encode=None,
    ) -> None:
        config = _config(cpm_rate_hz, duration_s, loop)
        self.scenario = RecordingScenario(config)
        self.sent_payloads: list[bytes] = []
        self.sleeps: list[float] = []
        self.log_lines: list[str] = []

        def fake_send(payload: bytes) -> int:
            self.sent_payloads.append(payload)
            return len(payload)

        self.generator = Generator(
            self.scenario,
            config,
            encode if encode is not None else lambda content: FAKE_PAYLOAD,
            fake_send,
            now_ms=lambda: FIXED_NOW_MS,
            sleep=self.sleeps.append,
            log=self.log_lines.append,
        )

    def tx_records(self) -> list[dict]:
        return [
            json.loads(line[len("[TX] ") :])
            for line in self.log_lines
            if line.startswith("[TX] ")
        ]


def test_cadence_and_tick_count_no_loop():
    """(a) 10 Hz over 1,0 s, loop off: exactly 10 ticks at t = 0,0..0,9, 10 sends, 10 sleeps."""
    harness = Harness(cpm_rate_hz=10.0, duration_s=1.0, loop=False)

    harness.generator.run()  # returns cleanly on its own

    assert len(harness.scenario.calls) == 10
    for index, (t, reference_time_ms) in enumerate(harness.scenario.calls):
        assert t == pytest.approx(index * 0.1)
        assert reference_time_ms == FIXED_NOW_MS
    assert harness.sent_payloads == [FAKE_PAYLOAD] * 10
    assert harness.sleeps == pytest.approx([0.1] * 10)


def test_loop_true_restarts_scenario_time_seq_keeps_counting():
    """(b) 2 Hz over 1,0 s, loop on, 6 ticks: t cycles 0,0/0,5 while seq runs 0..5."""
    harness = Harness(cpm_rate_hz=2.0, duration_s=1.0, loop=True)

    harness.generator.run(max_ticks=6)

    times = [t for t, _ in harness.scenario.calls]
    assert times == pytest.approx([0.0, 0.5, 0.0, 0.5, 0.0, 0.5])
    assert [record["seq"] for record in harness.tx_records()] == [0, 1, 2, 3, 4, 5]


def test_loop_false_exits_on_duration_without_max_ticks():
    """(c) loop off terminates by itself at duration_s - no max_ticks bound needed."""
    harness = Harness(cpm_rate_hz=5.0, duration_s=0.6, loop=False)

    harness.generator.run()  # would hang here if duration exit were broken

    assert len(harness.scenario.calls) == 3  # t = 0,0 / 0,2 / 0,4; next (0,6) >= duration_s


def test_tx_line_shape():
    """(d) every [TX] line is JSON with exactly {seq, scenario_time_s, bytes}, correct values."""
    harness = Harness(cpm_rate_hz=10.0, duration_s=0.5, loop=False)

    harness.generator.run()

    records = harness.tx_records()
    assert len(records) == 5
    for index, record in enumerate(records):
        assert set(record) == {"seq", "scenario_time_s", "bytes"}
        assert record["seq"] == index
        assert record["scenario_time_s"] == pytest.approx(index * 0.1)
        assert record["bytes"] == len(FAKE_PAYLOAD)


def test_encode_failure_tick_skipped_loop_continues():
    """(e) EncodeError on the 2nd tick: no send for that seq, one [ENC-SKIP], loop survives."""
    calls = {"count": 0}

    def flaky_encode(content: CpmContent) -> bytes:
        calls["count"] += 1
        if calls["count"] == 2:
            raise EncodeError("helper reported: boom")
        return FAKE_PAYLOAD

    harness = Harness(cpm_rate_hz=10.0, duration_s=0.5, loop=False, encode=flaky_encode)

    harness.generator.run()

    skip_lines = [line for line in harness.log_lines if line.startswith("[ENC-SKIP] ")]
    assert len(skip_lines) == 1
    skip_record = json.loads(skip_lines[0][len("[ENC-SKIP] ") :])
    assert skip_record["seq"] == 1
    assert "boom" in skip_record["reason"]

    # 5 ticks total, one skipped: 4 datagrams, and seq 1 is absent from the TX stream.
    assert len(harness.sent_payloads) == 4
    assert [record["seq"] for record in harness.tx_records()] == [0, 2, 3, 4]
    assert len(harness.sleeps) == 5  # cadence held through the failed tick
