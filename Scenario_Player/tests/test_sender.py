"""Tests for ``player/sender.py`` (11.1.6.6): loopback delivery + never-raises error path."""

import socket
from collections.abc import Iterator

import pytest

from player.sender import UdpSender

#: Receive timeout — generous for CI, tiny next to the suite's budget; loopback is instant.
_RECV_TIMEOUT_S: float = 2.0


@pytest.fixture()
def receiver() -> Iterator[socket.socket]:
    """A loopback UDP listener on an ephemeral port with a bounded recv timeout."""
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind(("127.0.0.1", 0))
    sock.settimeout(_RECV_TIMEOUT_S)
    yield sock
    sock.close()


def test_send_delivers_exact_bytes_and_returns_length(receiver: socket.socket) -> None:
    port = receiver.getsockname()[1]
    payload = b"\x00\x01\x02UPER-ish payload\xff\xfe"

    sender = UdpSender("127.0.0.1", port)
    try:
        assert sender.send(payload) == len(payload)
        received, _ = receiver.recvfrom(65535)
        assert received == payload
    finally:
        sender.close()


def test_two_sends_arrive_as_two_datagrams(receiver: socket.socket) -> None:
    port = receiver.getsockname()[1]
    first = b"datagram-one"
    second = b"datagram-two-longer"

    with UdpSender("127.0.0.1", port) as sender:
        assert sender.send(first) == len(first)
        assert sender.send(second) == len(second)

    got_first, _ = receiver.recvfrom(65535)
    got_second, _ = receiver.recvfrom(65535)
    assert got_first == first
    assert got_second == second


def test_send_after_close_logs_and_returns_zero() -> None:
    lines: list[str] = []
    sender = UdpSender("127.0.0.1", 9, log=lines.append)
    sender.close()
    sender.close()  # idempotent

    result = sender.send(b"after-close")  # must not raise (HLD D4: bench stays alive)

    assert result == 0
    assert len(lines) == 1
    assert lines[0].startswith("[SND-ERR]")
    assert "127.0.0.1:9" in lines[0]


def test_context_manager_sends_then_closes(receiver: socket.socket) -> None:
    port = receiver.getsockname()[1]
    lines: list[str] = []

    with UdpSender("127.0.0.1", port, log=lines.append) as sender:
        assert sender.send(b"inside-with") == len(b"inside-with")

    received, _ = receiver.recvfrom(65535)
    assert received == b"inside-with"
    # Exiting the with-block closed the socket: a later send is the logged-failure path.
    assert sender.send(b"after-with") == 0
    assert len(lines) == 1 and lines[0].startswith("[SND-ERR]")
