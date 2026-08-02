"""Rate-loop generator for the Scenario Player (SP HLD D4).

The bench's controller loop: at ``cpm_rate_hz``, sample the scenario at deterministic scenario
time ``t``, encode via the injected encode callable, ship via the injected send callable, and emit
one ``[TX]`` JSONL line per datagram (seq, scenario time, byte length) flushed to stdout for View
Log observability. ``loop: true`` restarts scenario time at ``duration_s`` (the global seq keeps
counting); ``loop: false`` exits cleanly. An ``EncodeError`` tick is logged as ``[ENC-SKIP]`` and
skipped - the loop survives (HLD D4: the bench is test equipment and must stay alive).

Clock and sleep are injectable callables (the pattern used across ``player/``) so tests run the
loop deterministically against fakes. Stdlib only; Python 3.11-compatible.
"""

import json
import time
from collections.abc import Callable

from player.config import ScenarioConfig
from player.contracts.cpm_content import CpmContent
from player.encoder_client import EncodeError
from player.scenario import Scenario


def _print_flushed(message: str) -> None:
    """Default log sink: stdout with an immediate flush (container View Log)."""
    print(message, flush=True)


def _wall_clock_ms() -> int:
    """Default ``now_ms``: wall clock in milliseconds (the sample's ``reference_time_ms``)."""
    return int(time.time() * 1000)


class Generator:
    """The ``cpm_rate_hz`` loop over one scenario: sample → encode → send → ``[TX]`` (HLD D4).

    Duck-types on the injected ``encode``/``send`` callables - production wires
    ``EncoderClient.encode`` and ``UdpSender.send``, tests pass fakes. ``now_ms``, ``sleep`` and
    ``log`` are injectable with wall-clock/``time.sleep``/flushed-print defaults.
    """

    def __init__(
        self,
        scenario: Scenario,
        scenario_cfg: ScenarioConfig,
        encode: Callable[[CpmContent], bytes],
        send: Callable[[bytes], int],
        *,
        now_ms: Callable[[], int] | None = None,
        sleep: Callable[[float], None] | None = None,
        log: Callable[[str], None] | None = None,
    ) -> None:
        self._scenario = scenario
        self._cfg = scenario_cfg
        self._encode = encode
        self._send = send
        self._now_ms: Callable[[], int] = _wall_clock_ms if now_ms is None else now_ms
        self._sleep: Callable[[float], None] = time.sleep if sleep is None else sleep
        self._log: Callable[[str], None] = _print_flushed if log is None else log

    def run(self, max_ticks: int | None = None) -> None:
        """Run the rate loop; ``max_ticks`` is a test hook bounding the total tick count.

        ``max_ticks=None`` runs per the scenario's ``duration_s``/``loop`` semantics: with
        ``loop: false`` the loop returns cleanly once a cycle completes, with ``loop: true`` it
        runs until externally stopped. Scenario time is pure arithmetic from the within-cycle tick
        counter (``t = tick_index * period``) - the wall clock never drifts into ``t``.
        """
        period = 1.0 / self._cfg.cpm_rate_hz
        seq = 0  # global datagram sequence - never reset by a loop restart
        cycle_tick = 0  # within-cycle tick counter - reset when scenario time restarts

        while max_ticks is None or seq < max_ticks:
            t = cycle_tick * period
            try:
                content = self._scenario.sample(t, self._now_ms())
                payload = self._encode(content)
            except EncodeError as exc:
                # HLD D4: one lost message, the loop survives - log, skip the send, continue.
                self._log(
                    "[ENC-SKIP] "
                    + json.dumps({"seq": seq, "reason": str(exc)}, separators=(",", ":"))
                )
            else:
                sent_bytes = self._send(payload)
                self._log(
                    "[TX] "
                    + json.dumps(
                        {"seq": seq, "scenario_time_s": t, "bytes": sent_bytes},
                        separators=(",", ":"),
                    )
                )

            # Fixed cadence: simple sleep-per-tick is the sanctioned M1 shape - drift correction
            # (measuring elapsed work time and shortening the sleep) is out of scope here.
            self._sleep(period)

            seq += 1
            cycle_tick += 1
            if cycle_tick * period >= self._cfg.duration_s:  # next tick would exceed duration_s
                if not self._cfg.loop:
                    return
                cycle_tick = 0  # loop: true - scenario time restarts at 0, seq keeps counting
