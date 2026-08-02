"""UDP transmitter for encoded CPM payload bytes (SP HLD §3/§4).

One ``UdpSender`` owns one datagram socket for its lifetime and ships each payload as exactly one
datagram to the V2X ECU peer. The host/port are constructor parameters — reading ``V2X_ECU_HOST``/
``V2X_ECU_PORT`` from env is ``player/config.py``'s job (HLD S5), never this module's.

Failure policy (HLD D4 — the bench is test equipment and must stay alive and observable in View
Log): a send that fails logs one ``[SND-ERR]`` line naming the peer and the error, returns 0, and
never raises. Sending after ``close()`` follows the same logged-failure path. Stdlib only; Python
3.11-compatible.
"""

import socket
from collections.abc import Callable


def _print_flushed(message: str) -> None:
    """Default log sink: stdout with an immediate flush (container View Log)."""
    print(message, flush=True)


class UdpSender:
    """One-datagram-per-payload UDP sender to a fixed ``host:port`` peer.

    Usable as a context manager; ``close()`` is idempotent, and a ``send`` after close takes the
    logged-failure path (returns 0) instead of raising — the bench loop keeps running.
    """

    def __init__(
        self,
        host: str,
        port: int,
        *,
        log: Callable[[str], None] | None = None,
    ) -> None:
        self._host = host
        self._port = port
        self._log: Callable[[str], None] = _print_flushed if log is None else log
        self._socket: socket.socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

    def send(self, payload: bytes) -> int:
        """Transmit ``payload`` as one datagram; return the byte count sent (for the TX log).

        On any ``OSError`` (peer unreachable, socket closed, oversized datagram, ...) logs one
        ``[SND-ERR]`` line and returns 0 — never raises (HLD D4: bench stays alive).
        """
        try:
            return self._socket.sendto(payload, (self._host, self._port))
        except OSError as exc:
            self._log(f"[SND-ERR] send to {self._host}:{self._port} failed: {exc}")
            return 0

    def close(self) -> None:
        """Close the owned socket; safe to call repeatedly."""
        self._socket.close()

    def __enter__(self) -> "UdpSender":
        return self

    def __exit__(self, *exc_info: object) -> None:
        self.close()
