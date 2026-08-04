"""12.3.2.8 — pacer deadlines: spacing, start delay, absolute schedule under a slow consumer,
disabled passthrough, and stream identity."""

import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from pacer import pace  # noqa: E402


class FakeClock:
    """Injected monotonic clock; sleep() records the interval and advances time by it."""

    def __init__(self, start: float = 100.0):
        self.now = start
        self.sleeps: list[float] = []

    def monotonic(self) -> float:
        return self.now

    def sleep(self, interval: float) -> None:
        assert interval > 0, "pacer must never sleep a non-positive interval"
        self.sleeps.append(interval)
        self.now += interval


def _paced(frames, clock, *, fps, stride, start_delay_s, enabled=True):
    return pace(frames, fps=fps, stride=stride, start_delay_s=start_delay_s,
                enabled=enabled, monotonic=clock.monotonic, sleep=clock.sleep)


def test_deadlines_are_stride_over_fps_apart():
    clock = FakeClock()
    release_times = []
    for _ in _paced(iter(range(4)), clock, fps=30.0, stride=5, start_delay_s=0.0):
        release_times.append(clock.now)
    period = 5 / 30.0
    for i in (1, 2, 3):
        assert release_times[i] - release_times[i - 1] == pytest.approx(period)
    # frame 0 is due at t0 itself: three sleeps of one period each
    assert clock.sleeps == pytest.approx([period, period, period])


def test_start_delay_precedes_first_frame():
    clock = FakeClock()
    gen = _paced(iter([object()]), clock, fps=30.0, stride=1, start_delay_s=0.5)
    next(gen)
    assert clock.sleeps == pytest.approx([0.5])
    assert clock.now == pytest.approx(100.5)


def test_slow_consumer_does_not_shift_later_deadlines():
    clock = FakeClock()
    gen = _paced(iter(range(5)), clock, fps=10.0, stride=1, start_delay_s=0.0)
    t0 = clock.now
    next(gen)  # frame 0 due at t0: no sleep
    assert clock.sleeps == []
    clock.now += 0.35  # consumer stalls past the frame-1..3 deadlines (0.1, 0.2, 0.3)
    next(gen)  # frame 1: late, released immediately
    next(gen)  # frame 2: late
    next(gen)  # frame 3: late
    assert clock.sleeps == []
    next(gen)  # frame 4 due at t0 + 0.4, absolute — not shifted by the three late frames
    assert clock.sleeps == pytest.approx([0.05])
    assert clock.now == pytest.approx(t0 + 0.4)


def test_disabled_performs_zero_sleeps():
    clock = FakeClock()
    out = list(_paced(iter(range(6)), clock, fps=30.0, stride=5, start_delay_s=2.0,
                      enabled=False))
    assert out == list(range(6))
    assert clock.sleeps == []
    assert clock.now == 100.0  # clock never consulted for scheduling


def test_yielded_sequence_identical_in_both_modes():
    frames = [object() for _ in range(5)]
    for enabled in (True, False):
        clock = FakeClock()
        out = list(_paced(iter(frames), clock, fps=30.0, stride=5, start_delay_s=0.1,
                          enabled=enabled))
        assert out == frames  # same objects, same order, nothing added or dropped
