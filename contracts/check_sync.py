#!/usr/bin/env python3
"""Phase 0 contract-integrity gate (subtask 1.0.7.1).

Runs both locally (``python contracts/check_sync.py`` from anywhere) and as the
CI ``contracts-gate`` job. Read-only and diagnostic: it never copies, fixes, or
rewrites anything -- it only reports.

Why node-local copies exist at all: each node folder (V2X_ECU/, ADA_ECU/,
IVI_ECU/, Scenario_Player/) is a self-contained Docker build context, so no node
build or test may read the repo-root contracts/ folder. Every consumer therefore
checks in a verbatim copy of the schemas/fixtures it consumes (HLD decision D1),
and this gate is what stops those copies from silently drifting.

Two checks, both driven entirely by contracts/sync-manifest.json (no path, token,
or scope is hardcoded here):

1. Byte identity -- every ``source -> target`` pair in the manifest's ``copies``
   map must be byte-for-byte equal.
2. Convention F2 -- the banned release-1 token (``f2BannedToken``) must not
   appear anywhere under the declared search root (``f2Scope``).

Exits 0 only when both checks pass; otherwise prints every collected failure and
exits 1.

Stdlib only -- CI runs this on a bare actions/setup-python@v5 interpreter with
nothing installed.
"""

import json
import sys
from pathlib import Path

# Resolved from this file's location, not the cwd, so the script behaves the
# same whether CI runs it from the repo root or a developer runs it from
# contracts/ (or anywhere else).
SCRIPT_DIR = Path(__file__).resolve().parent
REPO_ROOT = Path(__file__).resolve().parents[1]
MANIFEST_PATH = SCRIPT_DIR / "sync-manifest.json"


def check_copies(manifest, failures):
    """Compare every source -> target pair byte-for-byte. Returns the copy count.

    Files are read as bytes, never in text mode: the .uper golden vectors are
    binary UPER encodings, and text mode would both mangle them and hide
    line-ending drift in the JSON copies.
    """
    copy_count = 0
    for entry in manifest["copies"]:
        source_rel = entry["source"]
        source_path = REPO_ROOT / source_rel
        if source_path.is_file():
            source_bytes = source_path.read_bytes()
        else:
            source_bytes = None
            failures.append(f"missing source: {source_rel}")

        for target_rel in entry["targets"]:
            copy_count += 1
            target_path = REPO_ROOT / target_rel
            if not target_path.is_file():
                failures.append(f"missing target: {target_rel} (copy of {source_rel})")
                continue
            if source_bytes is None:
                # Source already reported missing; nothing to compare against.
                continue
            target_bytes = target_path.read_bytes()
            if target_bytes != source_bytes:
                failures.append(
                    f"byte difference: {target_rel} does not match {source_rel} "
                    f"(source {len(source_bytes)} bytes, target {len(target_bytes)} bytes)"
                )
    return copy_count


def check_f2(manifest, failures):
    """Fail on any file under the manifest's F2 scope containing the banned token.

    Only the ETSI release-2 spelling ``vanetza::asn1::r2::Cpm`` is allowed in V2X
    sources; the bare release-1 token ``asn1::Cpm`` is banned. A plain substring
    search is sufficient and needs no regex: the release-2 spelling contains the
    substring ``asn1::r2::Cpm``, which is NOT ``asn1::Cpm`` (the ``r2::`` segment
    sits between them), so a conforming release-2 file can never match.

    Files are read as bytes and the token is encoded to bytes before searching,
    so a non-UTF-8 file in the tree cannot crash the gate with a decode error.
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
        print(f"check_sync: FAIL - manifest not found at {MANIFEST_PATH}", file=sys.stderr)
        return 1

    manifest = json.loads(MANIFEST_PATH.read_text(encoding="utf-8"))

    # Collect every failure from both checks before exiting, so one run reports
    # the full picture instead of one problem at a time.
    failures = []
    copy_count = check_copies(manifest, failures)
    check_f2(manifest, failures)

    if failures:
        print(f"check_sync: FAIL - {len(failures)} problem(s) found", file=sys.stderr)
        for failure in failures:
            print(f"  - {failure}", file=sys.stderr)
        return 1

    print(
        f"check_sync: OK - {copy_count} copies byte-identical, "
        f"F2 grep clean over {manifest['f2Scope']}"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
