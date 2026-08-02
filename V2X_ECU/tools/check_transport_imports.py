#!/usr/bin/env python3
"""R7 transport-import gate (subtask 7.1.3.5, HLD D1).

Enforces the R7 seam permanently: ``src/net/`` is the sole socket-API holder,
so no C++ file outside it may include a transport header. Runs both locally
(``python V2X_ECU/tools/check_transport_imports.py`` from anywhere) and as a
CI ``contracts-gate`` step alongside ``contracts/check_sync.py``. Read-only
and diagnostic: it only reports.

Two checks:

1. Transport includes -- every ``.hpp/.h/.cpp/.cc`` under ``V2X_ECU/src/``,
   excluding the ``src/net/`` subtree, must not ``#include`` (``<...>`` or
   ``"..."`` form) ``sys/socket.h``, anything under ``netinet/`` or ``arpa/``,
   or asio in any spelling (``asio...`` / ``boost/asio...``).
2. Convention F2, re-asserted -- the banned release-1 token must not appear
   anywhere under ``V2X_ECU/src/`` (``src/net/`` included). Token and scope
   are read from ``contracts/sync-manifest.json`` -- the same source of truth
   ``check_sync.py`` uses -- so the two gates can never disagree. As there, a
   plain substring search suffices: the release-2 spelling
   ``vanetza::asn1::r2::Cpm`` contains ``asn1::r2::Cpm``, never the bare
   banned token.

Exits 0 only when both checks pass; otherwise prints every collected failure
and exits 1.

Stdlib only -- CI runs this on a bare actions/setup-python@v5 interpreter with
nothing installed.
"""

import json
import re
import sys
from pathlib import Path

# Resolved from this file's location (V2X_ECU/tools/), not the cwd, so the
# script behaves the same from the repo root, V2X_ECU/, or anywhere else.
REPO_ROOT = Path(__file__).resolve().parents[2]
SRC_ROOT = REPO_ROOT / "V2X_ECU" / "src"
NET_DIR = SRC_ROOT / "net"
MANIFEST_PATH = REPO_ROOT / "contracts" / "sync-manifest.json"

CPP_SUFFIXES = {".hpp", ".h", ".cpp", ".cc"}

# Matches both #include <...> and #include "..." forms.
INCLUDE_RE = re.compile(r'^\s*#\s*include\s*[<"]([^">]+)[">]')

# Banned per HLD D1: the socket API surface and asio in any spelling. Prefix
# matches implement the plan's <netinet/...>, <arpa/...>, <asio...>,
# <boost/asio...> wording.
BANNED_EXACT = ("sys/socket.h",)
BANNED_PREFIXES = ("netinet/", "arpa/", "asio", "boost/asio")


def is_banned_include(target):
    return target in BANNED_EXACT or target.startswith(BANNED_PREFIXES)


def check_includes(failures):
    """Scan C++ files under src/ (excluding src/net/) for transport includes.

    Returns the number of files scanned. Collects one failure per offending
    include line, with file and line number, so a single run reports the full
    picture.
    """
    if not SRC_ROOT.is_dir():
        failures.append(f"missing scan root: {SRC_ROOT.relative_to(REPO_ROOT).as_posix()}")
        return 0

    scanned = 0
    for path in sorted(SRC_ROOT.rglob("*")):
        if not path.is_file() or path.suffix not in CPP_SUFFIXES:
            continue
        if NET_DIR in path.parents:
            # src/net/ is the sole sanctioned socket-API holder (HLD D1).
            continue
        scanned += 1
        # Decode bytes leniently: a stray non-UTF-8 byte must not crash the
        # gate, and #include directives are ASCII anyway.
        text = path.read_bytes().decode("utf-8", errors="replace")
        relative = path.relative_to(REPO_ROOT).as_posix()
        for lineno, line in enumerate(text.splitlines(), start=1):
            match = INCLUDE_RE.match(line)
            if match and is_banned_include(match.group(1).strip()):
                failures.append(
                    f"transport include violation: {relative}:{lineno} includes "
                    f"'{match.group(1)}' (only src/net/ may include transport "
                    f"headers; HLD D1)"
                )
    return scanned


def check_f2(manifest, failures):
    """Re-assert convention F2 -- same semantics as contracts/check_sync.py.

    Files are read as bytes and the token is encoded before searching, so a
    non-UTF-8 file in the tree cannot crash the gate with a decode error.
    """
    token = manifest["f2BannedToken"]
    scope_rel = manifest["f2Scope"]
    scope_path = REPO_ROOT / scope_rel
    if not scope_path.is_dir():
        failures.append(f"missing F2 scope directory: {scope_rel}")
        return 0

    needle = token.encode("utf-8")
    scanned = 0
    for path in sorted(scope_path.rglob("*")):
        if not path.is_file():
            continue
        scanned += 1
        if needle in path.read_bytes():
            relative = path.relative_to(REPO_ROOT).as_posix()
            failures.append(
                f"F2 violation: banned token '{token}' found in {relative} "
                f"(use the release-2 spelling vanetza::asn1::r2::Cpm)"
            )
    return scanned


def main():
    if not MANIFEST_PATH.is_file():
        print(
            f"check_transport_imports: FAIL - manifest not found at {MANIFEST_PATH}",
            file=sys.stderr,
        )
        return 1

    manifest = json.loads(MANIFEST_PATH.read_text(encoding="utf-8"))

    # Collect every failure from both checks before exiting.
    failures = []
    cpp_count = check_includes(failures)
    check_f2(manifest, failures)

    if failures:
        print(
            f"check_transport_imports: FAIL - {len(failures)} problem(s) found",
            file=sys.stderr,
        )
        for failure in failures:
            print(f"  - {failure}", file=sys.stderr)
        return 1

    print(
        f"check_transport_imports: OK - {cpp_count} C++ file(s) outside src/net/ "
        f"free of transport includes, F2 grep clean over {manifest['f2Scope']}"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
