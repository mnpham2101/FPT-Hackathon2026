#!/usr/bin/env python3
"""Mock IVI receiver - Phase 4 test equipment, never part of the image.

The CI-side R4 loopback sink: binds a UDP socket, receives the R4 datagrams the
ADA ECU emits (``src/output/ivi_sender``), and makes them checkable. Standard
library only on the receive path; ``jsonschema`` is imported lazily and only
when ``--validate`` is passed.

Distinct from ``tools/ada-bench/mock_ivi.py`` (the deployed sink) - neither
imports the other.

The bind address comes from ``--host``/``--port`` or the ``MOCK_IVI_HOST``/
``MOCK_IVI_PORT`` env vars - there is no hardcoded peer. ``--port 0`` binds an
ephemeral port; the ``[RX] listening`` line reports the one chosen.

Per datagram, one stdout line: its receive sequence, byte length, and the body's
``type``, ``warningType`` and ``riskState`` (absent or unparseable -> ``-``).

Options:

- ``--out FILE``:     append every datagram's raw bytes as one JSONL line
                      (byte-identical to the wire body).
- ``--validate``:     validate each body against the synced
                      ``ADA_ECU/contracts/r4-ada-ivi.schema.json``; its ``$ref``
                      to ``r3-tracked-object.schema.json`` resolves through a
                      registry rooted at that contracts directory.
- ``--expect-min N``: on termination, exit non-zero when fewer than N warning
                      events (``type == "warning"``; schema-valid ones when
                      ``--validate``) arrived.
- ``--timeout-s S``:  exit after S seconds with no datagram, so CI runs end
                      deterministically. Without it the receiver runs until
                      SIGINT/SIGTERM.
"""

from __future__ import annotations

import argparse
import json
import os
import signal
import socket
import sys
import time
from pathlib import Path
from typing import Any, Dict, List, Optional

TOOLS_DIR = Path(__file__).resolve().parent
CONTRACTS_DIR = TOOLS_DIR.parent / "contracts"
R4_SCHEMA_PATH = CONTRACTS_DIR / "r4-ada-ivi.schema.json"

ENV_HOST = "MOCK_IVI_HOST"
ENV_PORT = "MOCK_IVI_PORT"
POLL_INTERVAL_S = 0.5  # recv poll so signals and the idle timeout get serviced
MAX_DATAGRAM_BYTES = 65535

_stop = False


def _request_stop(signum: int, frame: Any) -> None:  # noqa: ARG001 (signal API)
    global _stop
    _stop = True


def build_validator(schema_path: Path) -> Any:
    """A Draft 2020-12 validator whose relative $refs resolve in schema_path's
    directory. jsonschema is imported here, not at module level, so the plain
    receive path stays standard-library only."""
    try:
        import jsonschema
    except ImportError:
        raise SystemExit(
            "--validate requires the 'jsonschema' package, which is not "
            "installed; install it with: pip install jsonschema"
        )
    schema = json.loads(schema_path.read_text(encoding="utf-8"))
    contracts_dir = schema_path.parent
    try:
        # jsonschema >= 4.18: the referencing registry replaces RefResolver.
        from referencing import Registry, Resource

        resources = []
        for path in sorted(contracts_dir.glob("*.schema.json")):
            contents = json.loads(path.read_text(encoding="utf-8"))
            resources.append((path.name, Resource.from_contents(contents)))
        registry = Registry().with_resources(resources)
        return jsonschema.Draft202012Validator(schema, registry=registry)
    except ImportError:
        # Older jsonschema: RefResolver rooted at the contracts directory.
        resolver = jsonschema.RefResolver(
            base_uri=contracts_dir.as_uri() + "/", referrer=schema
        )
        return jsonschema.Draft202012Validator(schema, resolver=resolver)


def body_fields(payload: bytes) -> Dict[str, Any]:
    """The parsed body, or {} when the datagram is not a JSON object."""
    try:
        body = json.loads(payload.decode("utf-8"))
    except (UnicodeDecodeError, ValueError):
        return {}
    return body if isinstance(body, dict) else {}


def field_str(body: Dict[str, Any], name: str) -> str:
    value = body.get(name)
    return str(value) if isinstance(value, (str, int, float)) else "-"


def run_receive(args: argparse.Namespace) -> int:
    validator = build_validator(R4_SCHEMA_PATH) if args.validate else None
    out_file = open(args.out, "ab") if args.out else None

    seq = 0
    warnings = 0        # datagrams whose body has type == "warning"
    valid_warnings = 0  # of those, the schema-valid ones (--validate only)

    signal.signal(signal.SIGINT, _request_stop)
    signal.signal(signal.SIGTERM, _request_stop)

    try:
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as rx:
            rx.bind((args.host, args.port))
            rx.settimeout(POLL_INTERVAL_S)
            host, port = rx.getsockname()
            print(f"[RX] listening on {host}:{port}", flush=True)
            last_rx = time.monotonic()
            while not _stop:
                try:
                    payload, _ = rx.recvfrom(MAX_DATAGRAM_BYTES)
                except socket.timeout:
                    idle = time.monotonic() - last_rx
                    if args.timeout_s is not None and idle >= args.timeout_s:
                        print(f"[RX] idle for {args.timeout_s}s, exiting",
                              flush=True)
                        break
                    continue
                except OSError:
                    if _stop:
                        break
                    raise
                last_rx = time.monotonic()

                body = body_fields(payload)
                print(f"[RX] seq={seq} bytes={len(payload)} "
                      f"type={field_str(body, 'type')} "
                      f"warningType={field_str(body, 'warningType')} "
                      f"riskState={field_str(body, 'riskState')}", flush=True)

                if out_file is not None:
                    out_file.write(payload + b"\n")
                    out_file.flush()

                is_warning = body.get("type") == "warning"
                warnings += 1 if is_warning else 0
                if validator is not None and is_warning:
                    errors: List[Any] = list(validator.iter_errors(body))
                    for err in errors:
                        loc = "/".join(str(p) for p in err.absolute_path) or "$"
                        print(f"[VAL] seq={seq} {loc}: {err.message}",
                              flush=True)
                    valid_warnings += 0 if errors else 1
                seq += 1
    finally:
        if out_file is not None:
            out_file.close()

    counted = valid_warnings if args.validate else warnings
    ok = args.expect_min is None or counted >= args.expect_min
    print(f"[RX] result: datagrams={seq} warnings={warnings}"
          + (f" schema-valid={valid_warnings}" if args.validate else "")
          + (f" expect-min={args.expect_min}" if args.expect_min is not None
             else "")
          + f" -> {'PASS' if ok else 'FAIL'}", flush=True)
    return 0 if ok else 1


def main(argv: Optional[List[str]] = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--host", default=os.environ.get(ENV_HOST),
                        help=f"bind host (default: env {ENV_HOST})")
    parser.add_argument("--port", type=int,
                        default=int(os.environ[ENV_PORT])
                        if os.environ.get(ENV_PORT) else None,
                        help=f"bind UDP port, 0 for ephemeral "
                             f"(default: env {ENV_PORT})")
    parser.add_argument("--out", type=Path, default=None,
                        help="append each datagram's raw bytes as a JSONL line")
    parser.add_argument("--validate", action="store_true",
                        help="validate each body against the synced R4 schema "
                             "(requires the jsonschema package)")
    parser.add_argument("--expect-min", type=int, default=None, metavar="N",
                        help="exit non-zero when fewer than N warning events "
                             "(schema-valid ones under --validate) arrived")
    parser.add_argument("--timeout-s", type=float, default=None, metavar="S",
                        help="exit after S seconds with no datagram "
                             "(default: run until SIGINT/SIGTERM)")
    args = parser.parse_args(argv)

    if args.host is None or args.port is None:
        parser.error(f"bind address required: --host/--port or env "
                     f"{ENV_HOST}/{ENV_PORT} (no hardcoded peer)")
    if args.expect_min is not None and args.expect_min < 0:
        parser.error("--expect-min must be >= 0")
    if args.timeout_s is not None and args.timeout_s <= 0:
        parser.error("--timeout-s must be > 0")
    return run_receive(args)


if __name__ == "__main__":
    sys.exit(main())
