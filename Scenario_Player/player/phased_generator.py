"""Three-phase demo-cycle generator for the Scenario Player (SP HLD D8).

Cycles a repeating waiting / two-vehicle / three-vehicle choreography: ``phases.waiting_s`` of
silence (no CPM sent - the IVI waiting state), then ``phases.two_vehicle_s`` of CPM sent from B
with C held beyond the R13 admission gate (only A and B are perceptually present), then
``phases.three_vehicle_s`` of CPM sent with C inside the gate and closing (the R19 warning
window). The cycle then restarts.

Reuses ``Scenario``/``player/scenario.py`` unchanged (D3, D8): this component only selects which
of two pre-built ``Scenario`` instances to sample, and whether to send at all - kinematics stays
one model, never a branch inside it. It is a separate controller component from
``player/generator.py``, not a branch inside ``Generator``, so the existing continuous single-phase
path and its tests are untouched. Stdlib only; Python 3.11-compatible.
"""

import json
import time
from collections.abc import Callable

from player.config import PhaseConfig, ScenarioConfig
from player.contracts.cpm_content import CpmContent
from player.encoder_client import EncodeError
from player.scenario import Scenario

_WAITING = "waiting"
_TWO_VEHICLE = "two_vehicle"
_THREE_VEHICLE = "three_vehicle"


def _print_flushed(message: str) -> None:
    """Default log sink: stdout with an immediate flush (container View Log)."""
    print(message, flush=True)


def _wall_clock_ms() -> int:
    """Default ``now_ms``: wall clock in milliseconds (the sample's ``reference_time_ms``)."""
    return int(time.time() * 1000)


class PhasedGenerator:
    """The three-phase demo-cycle loop: waiting -> two_vehicle -> three_vehicle -> repeat (D8).

    Duck-types on the injected ``encode``/``send`` callables, exactly like ``Generator``.
    ``now_ms``, ``sleep`` and ``log`` are injectable with wall-clock/``time.sleep``/flushed-print
    defaults.
    """

    def __init__(
        self,
        two_vehicle_scenario: Scenario,
        three_vehicle_scenario: Scenario,
        phases: PhaseConfig,
        scenario_cfg: ScenarioConfig,
        encode: Callable[[CpmContent], bytes],
        send: Callable[[bytes], int],
        *,
        now_ms: Callable[[], int] | None = None,
        sleep: Callable[[float], None] | None = None,
        log: Callable[[str], None] | None = None,
    ) -> None:
        self._two_vehicle = two_vehicle_scenario
        self._three_vehicle = three_vehicle_scenario
        self._phases = phases
        self._cfg = scenario_cfg
        self._encode = encode
        self._send = send
        self._now_ms: Callable[[], int] = _wall_clock_ms if now_ms is None else now_ms
        self._sleep: Callable[[float], None] = time.sleep if sleep is None else sleep
        self._log: Callable[[str], None] = _print_flushed if log is None else log

    def _phase_at(self, t_cycle: float) -> tuple[str, float]:
        """Resolve the phase and its local (phase-relative) time at cycle time ``t_cycle``."""
        if t_cycle < self._phases.waiting_s:
            return _WAITING, t_cycle
        t_cycle -= self._phases.waiting_s
        if t_cycle < self._phases.two_vehicle_s:
            return _TWO_VEHICLE, t_cycle
        return _THREE_VEHICLE, t_cycle - self._phases.two_vehicle_s

    def run(self, max_ticks: int | None = None) -> None:
        """Run the phased cycle; ``max_ticks`` is a test hook bounding the total tick count.

        Every tick advances the cycle clock and sleeps for one period regardless of phase, so
        cadence is identical to ``Generator``. Only the ``waiting`` phase skips encode/send - the
        loop still ticks and sleeps through it, silence rather than a pause.
        """
        period = 1.0 / self._cfg.cpm_rate_hz
        cycle_length = self._phases.cycle_length_s
        seq = 0  # global tick counter - never reset by a cycle restart
        cycle_tick = 0  # within-cycle tick counter - reset when the cycle restarts
        last_phase: str | None = None

        while max_ticks is None or seq < max_ticks:
            t_cycle = cycle_tick * period
            phase, phase_t = self._phase_at(t_cycle)

            if phase != last_phase:
                self._log(
                    "[PHASE] " + json.dumps({"phase": phase, "seq": seq}, separators=(",", ":"))
                )
                last_phase = phase

            if phase != _WAITING:
                scenario = self._two_vehicle if phase == _TWO_VEHICLE else self._three_vehicle
                try:
                    content = scenario.sample(phase_t, self._now_ms())
                    payload = self._encode(content)
                except EncodeError as exc:
                    self._log(
                        "[ENC-SKIP] "
                        + json.dumps({"seq": seq, "reason": str(exc)}, separators=(",", ":"))
                    )
                else:
                    sent_bytes = self._send(payload)
                    self._log(
                        "[TX] "
                        + json.dumps(
                            {"seq": seq, "scenario_time_s": phase_t, "bytes": sent_bytes},
                            separators=(",", ":"),
                        )
                    )

            # Fixed cadence through every phase, waiting included (HLD D8/D5).
            self._sleep(period)

            seq += 1
            cycle_tick += 1
            if cycle_tick * period >= cycle_length:  # next tick would exceed the cycle length
                if not self._cfg.loop:
                    return
                cycle_tick = 0  # loop: true - cycle restarts at 0, seq keeps counting
                last_phase = None  # forces a fresh [PHASE] line at the new cycle's start
