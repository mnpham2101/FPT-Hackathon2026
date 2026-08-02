"""Persistent ``cpm_encode`` subprocess client for the Scenario Player (SP HLD D1/D4).

Speaks the D1 stream protocol with the R1 codec helper: the helper is spawned once as
``[encoder_path, "--stream"]`` and kept alive across calls; each ``encode`` writes one compact
``CpmContent`` JSON line to its stdin and reads exactly one reply line from its stdout - either a
base64 UPER payload (returned as decoded bytes) or an ``{"error": ...}`` object, which is logged
and surfaced as ``EncodeError`` while the helper process stays up for the next call (HLD D1: an
encode failure never kills the stream). A reply line that is neither an error object nor valid
base64 also raises ``EncodeError`` without restarting the helper - the process is alive, only that
one reply was garbage.

Helper death (write failure or EOF on stdout) follows HLD D4: log the restart, back off via the
injectable sleeper, respawn, and raise ``EncodeError`` for the in-flight message only - the caller
skips that tick, the bench stays alive and observable, and restarts are unbounded across calls
(one failed encode per death, never a crash). ``restart_backoff_s`` is a client-internal
resilience knob (constructor parameter), deliberately not an env tunable; reading ``ENCODER_PATH``
is ``player/config.py``'s job - this module only takes parameters.

The helper's stderr is inherited (neither piped nor devnulled) so its diagnostics land in the
container log next to the bench's own lines (View Log observability). Stdlib only; Python
3.11-compatible.
"""

import base64
import binascii
import json
import subprocess
import time
from collections.abc import Callable, Sequence
from typing import NoReturn

from player.contracts.cpm_content import CpmContent, to_json

#: Grace period in ``close()`` for the helper to exit after stdin closes, before it is killed.
#: Client-internal shutdown detail (like ``restart_backoff_s``), not an env tunable.
_CLOSE_WAIT_S: float = 2.0


def _print_flushed(message: str) -> None:
    """Default log sink: stdout with an immediate flush (container View Log)."""
    print(message, flush=True)


class EncodeError(Exception):
    """One encode failed: helper-reported ``{"error": ...}``, garbage reply, or helper death.

    The client stays usable after raising - the next ``encode`` runs against the same (or, after
    a death, the respawned) helper process.
    """


class EncoderClient:
    """Line-oriented client around one persistent ``cpm_encode --stream`` subprocess (HLD D1).

    ``command`` overrides the full argv (tests run the committed fake helper via
    ``[sys.executable, fake_script, mode]``); by default the real helper binary is invoked as
    ``[encoder_path, "--stream"]``. The subprocess is spawned lazily on first use, or explicitly
    via ``start()``. Usable as a context manager; ``close()`` leaves no child behind.
    """

    def __init__(
        self,
        encoder_path: str,
        *,
        command: Sequence[str] | None = None,
        restart_backoff_s: float = 0.5,
        sleep: Callable[[float], None] | None = None,
        log: Callable[[str], None] | None = None,
    ) -> None:
        self._command: list[str] = (
            list(command) if command is not None else [encoder_path, "--stream"]
        )
        self._restart_backoff_s = restart_backoff_s
        self._sleep: Callable[[float], None] = time.sleep if sleep is None else sleep
        self._log: Callable[[str], None] = _print_flushed if log is None else log
        self._process: subprocess.Popen[str] | None = None

    @property
    def pid(self) -> int | None:
        """PID of the live helper process, or ``None`` before start / after close."""
        return None if self._process is None else self._process.pid

    def start(self) -> None:
        """Spawn the helper if not already running; idempotent while a process is attached.

        A process that died mid-stream is *not* silently respawned here - death is detected and
        handled (log, backoff, respawn, ``EncodeError``) inside ``encode`` per HLD D4.
        """
        if self._process is None:
            self._process = self._spawn()

    def encode(self, content: CpmContent) -> bytes:
        """Encode one ``CpmContent``: one JSON line out, one reply line in, per HLD D1.

        Returns the base64-decoded UPER payload bytes. Raises ``EncodeError`` on a helper
        ``{"error": ...}`` reply or a non-base64 garbage reply (helper stays up), and on helper
        death (helper is respawned after backoff; only this message is lost).
        """
        self.start()
        process = self._process
        assert process is not None and process.stdin is not None and process.stdout is not None

        line = to_json(content)
        # to_json emits compact separators and escaped strings - defend the one-line framing
        # invariant anyway, since an embedded newline would desynchronize the whole stream.
        if "\n" in line or "\r" in line:
            raise ValueError("CpmContent JSON serialized with an embedded newline")

        try:
            process.stdin.write(line + "\n")
            process.stdin.flush()
        except OSError as exc:  # BrokenPipeError included - helper is gone
            self._restart_and_fail(f"write failed: {exc}")

        reply = process.stdout.readline()
        if reply == "":  # EOF - helper exited mid-stream
            self._restart_and_fail("EOF on stdout")
        reply = reply.strip()

        parsed: object = None
        try:
            parsed = json.loads(reply)
        except ValueError:
            pass
        if isinstance(parsed, dict) and "error" in parsed:
            reason = str(parsed["error"])
            self._log(f"[ENC] helper reported encode error: {reason}")
            raise EncodeError(reason)

        try:
            return base64.b64decode(reply, validate=True)
        except binascii.Error as exc:
            self._log(f"[ENC] helper returned a non-base64 reply: {reply!r}")
            raise EncodeError(f"non-base64 reply from helper: {reply!r}") from exc

    def close(self) -> None:
        """Detach and terminate the helper: close stdin, wait briefly, kill on timeout.

        Leaves no zombie children; safe to call repeatedly. A later ``encode`` respawns.
        """
        process = self._process
        self._process = None
        if process is not None:
            self._reap(process)

    def __enter__(self) -> "EncoderClient":
        self.start()
        return self

    def __exit__(self, *exc_info: object) -> None:
        self.close()

    # --- internals --------------------------------------------------------------------------

    def _spawn(self) -> "subprocess.Popen[str]":
        # Line-buffered text pipes both ways; stderr inherited (module docstring rationale).
        return subprocess.Popen(
            self._command,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            text=True,
            bufsize=1,
        )

    def _restart_and_fail(self, cause: str) -> NoReturn:
        """HLD D4 death path: log, reap, back off (injectable), respawn, fail this message."""
        backoff = self._restart_backoff_s
        self._log(f"[ENC] helper died — restarting after {backoff}s ({cause})")
        dead = self._process
        self._process = None
        if dead is not None:
            self._reap(dead)
        self._sleep(backoff)
        self._process = self._spawn()
        raise EncodeError(f"helper died mid-encode ({cause}); message dropped, helper restarted")

    @staticmethod
    def _reap(process: "subprocess.Popen[str]") -> None:
        for stream in (process.stdin, process.stdout):
            if stream is not None:
                try:
                    stream.close()
                except OSError:
                    pass
        try:
            process.wait(timeout=_CLOSE_WAIT_S)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait()
