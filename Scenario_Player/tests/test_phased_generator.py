"""11.1.6.9 - PhasedGenerator: the D8 waiting/two-vehicle/three-vehicle cycle.

Runs via ``python -m pytest Scenario_Player/tests`` from the repo root (the CI invocation). The
loop is driven entirely by fakes, mirroring ``test_generator.py``'s harness style: fixed ``now_ms``,
recorded ``sleep``, captured ``log``, predictable encode bytes, len-returning send. Covers: the
waiting phase sends nothing, the two-vehicle and three-vehicle phases sample their own ``Scenario``
with phase-local time starting at 0, one ``[PHASE]`` line per transition (and one per cycle
restart), the ``[TX]``/``[ENC-SKIP]`` shapes are unchanged from ``Generator``, and ``loop: false``
stops cleanly at the cycle's end.
"""

import json
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from player.config import ObjectConfig, PhaseConfig, ScenarioConfig, SenderConfig  # noqa: E402
from player.contracts.cpm_content import CpmContent  # noqa: E402
from player.encoder_client import EncodeError  # noqa: E402
from player.phased_generator import PhasedGenerator  # noqa: E402
from player.scenario import Scenario  # noqa: E402

FIXED_NOW_MS = 716084805123
FAKE_PAYLOAD = b"\x0e\x00\x9cUPER"

_SENDER = SenderConfig(station_id=1201, lat=21.028511, lon=105.804817, heading_deg=90.0)


def _object(initial_distance_m: float, closing_speed_mps: float) -> ObjectConfig:
    return ObjectConfig(
        object_id=7,
        initial_distance_m=initial_distance_m,
        closing_speed_mps=closing_speed_mps,
        lateral_offset_m=1.2,
        classification=5,
        confidence=95,
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
    """One PhasedGenerator wired to fakes: recorded sends/sleeps, captured log, fixed clock."""

    def __init__(
        self,
        cpm_rate_hz: float,
        waiting_s: float,
        two_vehicle_s: float,
        three_vehicle_s: float,
        loop: bool,
        *,
        encode=None,
    ) -> None:
        cfg = ScenarioConfig(
            name="phased-test",
            loop=loop,
            sender=_SENDER,
            cpm_rate_hz=cpm_rate_hz,
            phases=PhaseConfig(
                waiting_s=waiting_s, two_vehicle_s=two_vehicle_s, three_vehicle_s=three_vehicle_s
            ),
            two_vehicle_object=_object(60.0, 0.0),
            three_vehicle_object=_object(25.0, 3.0),
        )
        self.two_vehicle_scenario = RecordingScenario(
            ScenarioConfig(name=cfg.name, loop=cfg.loop, sender=cfg.sender, object=cfg.two_vehicle_object)
        )
        self.three_vehicle_scenario = RecordingScenario(
            ScenarioConfig(name=cfg.name, loop=cfg.loop, sender=cfg.sender, object=cfg.three_vehicle_object)
        )
        self.sent_payloads: list[bytes] = []
        self.sleeps: list[float] = []
        self.log_lines: list[str] = []

        def fake_send(payload: bytes) -> int:
            self.sent_payloads.append(payload)
            return len(payload)

        self.generator = PhasedGenerator(
            self.two_vehicle_scenario,
            self.three_vehicle_scenario,
            cfg.phases,
            cfg,
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

    def phase_records(self) -> list[dict]:
        return [
            json.loads(line[len("[PHASE] ") :])
            for line in self.log_lines
            if line.startswith("[PHASE] ")
        ]


def test_waiting_phase_sends_nothing():
    """(a) 10 Hz, waiting_s=1.0: 10 ticks, zero sends, zero scenario samples, cadence still held."""
    harness = Harness(
        cpm_rate_hz=10.0, waiting_s=1.0, two_vehicle_s=1.0, three_vehicle_s=1.0, loop=False
    )

    harness.generator.run(max_ticks=10)

    assert harness.sent_payloads == []
    assert harness.two_vehicle_scenario.calls == []
    assert harness.three_vehicle_scenario.calls == []
    assert len(harness.sleeps) == 10


def test_two_vehicle_phase_samples_two_vehicle_scenario_from_zero():
    """(b) two_vehicle phase samples only the two_vehicle Scenario, local time restarting at 0."""
    harness = Harness(
        cpm_rate_hz=10.0, waiting_s=0.2, two_vehicle_s=0.3, three_vehicle_s=0.3, loop=False
    )

    harness.generator.run(max_ticks=5)  # ticks 0,1 waiting; 2,3,4 two_vehicle (t=0.0/0.1/0.2)

    assert harness.three_vehicle_scenario.calls == []
    times = [t for t, _ in harness.two_vehicle_scenario.calls]
    assert times == pytest.approx([0.0, 0.1, 0.2])


def test_three_vehicle_phase_samples_three_vehicle_scenario_from_zero():
    """(c) three_vehicle phase samples only the three_vehicle Scenario, local time restarting at 0."""
    harness = Harness(
        cpm_rate_hz=10.0, waiting_s=0.1, two_vehicle_s=0.1, three_vehicle_s=0.3, loop=False
    )

    harness.generator.run(max_ticks=5)  # tick 0 waiting; tick 1 two_vehicle; ticks 2,3,4 three_vehicle

    assert len(harness.two_vehicle_scenario.calls) == 1
    times = [t for t, _ in harness.three_vehicle_scenario.calls]
    assert times == pytest.approx([0.0, 0.1, 0.2])


def test_phase_transitions_logged_once_each():
    """(d) exactly one [PHASE] line per transition, in waiting -> two_vehicle -> three_vehicle order."""
    harness = Harness(
        cpm_rate_hz=10.0, waiting_s=0.2, two_vehicle_s=0.2, three_vehicle_s=0.2, loop=False
    )

    harness.generator.run()  # full cycle: 2+2+2 = 6 ticks

    phases = [record["phase"] for record in harness.phase_records()]
    assert phases == ["waiting", "two_vehicle", "three_vehicle"]


def test_loop_true_restarts_cycle_and_reemits_phase_lines():
    """(e) loop: true wraps back to waiting and logs a fresh [PHASE] line each cycle restart."""
    harness = Harness(
        cpm_rate_hz=10.0, waiting_s=0.1, two_vehicle_s=0.1, three_vehicle_s=0.1, loop=True
    )

    harness.generator.run(max_ticks=6)  # exactly two full cycles of 3 ticks each

    phases = [record["phase"] for record in harness.phase_records()]
    assert phases == ["waiting", "two_vehicle", "three_vehicle", "waiting", "two_vehicle", "three_vehicle"]
    assert [record["seq"] for record in harness.phase_records()] == [0, 1, 2, 3, 4, 5]


def test_loop_false_stops_after_one_cycle():
    """(f) loop: false returns cleanly once the cycle completes, with no max_ticks bound needed."""
    harness = Harness(
        cpm_rate_hz=10.0, waiting_s=0.1, two_vehicle_s=0.1, three_vehicle_s=0.1, loop=False
    )

    harness.generator.run()  # would hang here if the cycle-end exit were broken

    assert len(harness.sleeps) == 3


def test_tx_line_shape_unchanged_from_generator():
    """(g) every [TX] line during an active phase is JSON with exactly {seq, scenario_time_s, bytes}."""
    harness = Harness(
        cpm_rate_hz=10.0, waiting_s=0.1, two_vehicle_s=0.2, three_vehicle_s=0.0001, loop=False
    )

    harness.generator.run(max_ticks=3)  # tick 0 waiting; ticks 1,2 two_vehicle

    records = harness.tx_records()
    assert len(records) == 2
    for record in records:
        assert set(record) == {"seq", "scenario_time_s", "bytes"}
        assert record["bytes"] == len(FAKE_PAYLOAD)
    assert [record["seq"] for record in records] == [1, 2]


def test_encode_failure_during_active_phase_skipped_loop_continues():
    """(h) EncodeError on an active-phase tick: no send, one [ENC-SKIP], loop survives (D4/D8)."""

    def flaky_encode(content: CpmContent) -> bytes:
        raise EncodeError("helper reported: boom")

    harness = Harness(
        cpm_rate_hz=10.0,
        waiting_s=0.1,
        two_vehicle_s=0.1,
        three_vehicle_s=0.1,
        loop=False,
        encode=flaky_encode,
    )

    harness.generator.run()

    skip_lines = [line for line in harness.log_lines if line.startswith("[ENC-SKIP] ")]
    assert len(skip_lines) == 2  # one two_vehicle tick, one three_vehicle tick
    assert harness.sent_payloads == []
    assert len(harness.sleeps) == 3  # cadence held through both failed ticks
