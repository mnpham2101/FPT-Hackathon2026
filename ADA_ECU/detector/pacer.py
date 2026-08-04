"""D10 deadline scheduler: hold each sampled frame until its wall-clock instant
(ADA_ECU/doc/ada-ecu-design-decisions.md D10 — deadlines are computed against t0, never
accumulated, so inference cost does not fold into scenario time)."""

import time


def pace(frames, *, fps: float, stride: int, start_delay_s: float, enabled: bool,
         monotonic=time.monotonic, sleep=time.sleep):
    """Wrap an Iterator[Frame]; yield the same frames on schedule. Adds, drops and reorders nothing."""
    if not enabled:
        yield from frames
        return
    t0 = monotonic()  # taken once at iteration start; all deadlines are absolute against it
    for n, frame in enumerate(frames):  # n = sampled-frame ordinal; frames are opaque
        deadline = t0 + start_delay_s + n * stride / fps
        remaining = deadline - monotonic()
        if remaining > 0:
            # The only detector module permitted to call time.sleep (layer rule).
            sleep(remaining)
        # A late frame releases immediately; later deadlines do not shift.
        yield frame
