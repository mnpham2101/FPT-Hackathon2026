#!/usr/bin/env python3
"""Golden-vector UDP sender -- the bench-side send stand-in for the D7 comms check.

Sends every golden-vector ``.uper`` payload as **one UDP datagram each** to a
target ``host:port``: raw UPER ``CollectivePerceptionMessage`` bytes with no
framing (convention F5), which is exactly what the V2X ECU listener expects.

Test equipment only (V2X ECU comms HLD, decision D7). It runs locally and in the
CI ``v2x-comms-check`` lane; it is never deployed -- on the platform the live
sender is the Scenario Player.

Usage::

    send_cpm.py [host] [port] [--corpus DIR] [--delay-ms MS] [--repeat N]

Configuration -- CLI wins over env, and there is no default peer (governing
principle 5: no hardcoded peers):

===================  ====================  ==========================================
CLI                  Env                   Meaning / default
===================  ====================  ==========================================
``host`` (pos. 1)    ``TARGET_HOST``       destination host; **no default** (exit 2)
``port`` (pos. 2)    ``TARGET_PORT``       destination UDP port; **no default** (exit 2)
``--corpus DIR``     ``CORPUS_DIR``        vector corpus; default ``contracts/golden-vectors/``
                                           resolved relative to *this file*, so any cwd works
``--delay-ms MS``    ``SEND_DELAY_MS``     inter-send pause, default 100 -- pacing for log
                                           readability only, not a protocol rate
``--repeat N``       --                    send the whole corpus N times, default 1
                                           (feeds the duplicate-drop path)
===================  ====================  ==========================================

Send order is the ``.uper`` filenames sorted lexicographically, every repeat --
deterministic so ``check_v2x_log.py`` can correlate its expectations positionally.

Output: one line per datagram, ``[SEND] <case> len=<n>``, then a summary
``[SEND] done cases=<n> bytes=<total>`` (``cases`` counts datagrams sent, so it is
corpus size x repeat). Exit 0 on success; 2 on a missing/invalid target; 1 on a
missing or empty corpus, an unreadable vector, or a socket error -- this is test
equipment, so it fails loudly rather than sending a partial corpus silently.
"""

import os
import socket
import sys
import time
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
REPO_ROOT = SCRIPT_DIR.parents[1]                          # tools/comms_check -> repo root
DEFAULT_CORPUS = REPO_ROOT / "contracts" / "golden-vectors"

ENV_HOST = os.environ.get("TARGET_HOST")                   # no default: refuse to guess a peer
ENV_PORT = os.environ.get("TARGET_PORT")
ENV_CORPUS = os.environ.get("CORPUS_DIR")                  # default resolved from this file, not cwd
ENV_DELAY_MS = os.environ.get("SEND_DELAY_MS", "100")      # log-readability pacing, not a rate spec

USAGE = "usage: send_cpm.py [host] [port] [--corpus DIR] [--delay-ms MS] [--repeat N]"


def log(tag, msg):
    print(f"[{tag}] {msg}", flush=True)


def die(msg, code):
    print(f"[ERR] {msg}", file=sys.stderr, flush=True)
    sys.exit(code)


def parse_args(argv):
    """Hand-rolled so the two positionals stay optional (env fallback) and the
    usage/exit codes match the acceptance: 2 = bad invocation, 1 = bad corpus."""
    host, port = ENV_HOST, ENV_PORT
    corpus, delay_ms, repeat = ENV_CORPUS, ENV_DELAY_MS, "1"
    positional = []
    i = 0
    while i < len(argv):
        arg = argv[i]
        if arg in ("-h", "--help"):
            print(__doc__)
            sys.exit(0)
        if arg in ("--corpus", "--delay-ms", "--repeat"):
            if i + 1 >= len(argv):
                print(USAGE, file=sys.stderr, flush=True)
                die(f"{arg} needs a value", 2)
            value = argv[i + 1]
            if arg == "--corpus":
                corpus = value
            elif arg == "--delay-ms":
                delay_ms = value
            else:
                repeat = value
            i += 2
            continue
        if arg.startswith("-"):
            print(USAGE, file=sys.stderr, flush=True)
            die(f"unknown option {arg}", 2)
        positional.append(arg)
        i += 1

    if len(positional) > 2:
        print(USAGE, file=sys.stderr, flush=True)
        die(f"expected at most 2 positional args, got {len(positional)}", 2)
    if len(positional) >= 1:
        host = positional[0]
    if len(positional) >= 2:
        port = positional[1]

    if not host or not port:
        print(USAGE, file=sys.stderr, flush=True)
        die("no target: give host+port as args or set TARGET_HOST/TARGET_PORT", 2)

    def positive_number(name, raw, cast, minimum):
        try:
            value = cast(raw)
        except ValueError:
            print(USAGE, file=sys.stderr, flush=True)
            die(f"{name} must be a number, got {raw!r}", 2)
        if value < minimum:
            print(USAGE, file=sys.stderr, flush=True)
            die(f"{name} must be >= {minimum}, got {value}", 2)
        return value

    port_n = positive_number("port", port, int, 1)
    if port_n > 65535:
        print(USAGE, file=sys.stderr, flush=True)
        die(f"port must be <= 65535, got {port_n}", 2)
    delay_s = positive_number("--delay-ms/SEND_DELAY_MS", delay_ms, float, 0) / 1000.0
    repeat_n = positive_number("--repeat", repeat, int, 1)
    corpus_dir = Path(corpus).expanduser() if corpus else DEFAULT_CORPUS
    return host, port_n, corpus_dir, delay_s, repeat_n


def collect_vectors(corpus_dir):
    if not corpus_dir.is_dir():
        die(f"corpus directory not found: {corpus_dir}", 1)
    vectors = sorted(corpus_dir.glob("*.uper"))            # lexicographic = the documented send order
    if not vectors:
        die(f"no .uper vectors in corpus directory: {corpus_dir}", 1)
    return vectors


def main(argv):
    host, port, corpus_dir, delay_s, repeat = parse_args(argv)
    vectors = collect_vectors(corpus_dir)
    log("BOOT", f"send_cpm.py -> {host}:{port} corpus={corpus_dir} "
                f"vectors={len(vectors)} repeat={repeat} delay_ms={delay_s * 1000:g}")

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sent, total = 0, 0
    try:
        for _ in range(repeat):
            for path in vectors:
                try:
                    payload = path.read_bytes()
                except OSError as e:
                    die(f"cannot read vector {path}: {e}", 1)
                try:
                    sock.sendto(payload, (host, port))     # one datagram per PDU, raw UPER (F5)
                except OSError as e:
                    die(f"send failed for {path.stem} to {host}:{port} - {e}", 1)
                sent += 1
                total += len(payload)
                log("SEND", f"{path.stem} len={len(payload)}")
                if delay_s:
                    time.sleep(delay_s)
    finally:
        sock.close()

    log("SEND", f"done cases={sent} bytes={total}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
