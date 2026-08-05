#!/usr/bin/env python3
"""Collision-risk event list -- the D8 demo-table artifact (R18).

Renders a saved ADA ``[EVT]`` stream as the chronological collision-risk
event list a human reads: the ``track_transition`` + ``risk_transition`` +
``r4_tx`` subset, one table row per event, oldest first. A report, not a
checker -- it asserts nothing about legality (that is ``check_evt_log.py``)
and works on a saved log alone, with no live process.

Usage::

    event_report.py [logfile] [--summary]

``logfile`` omitted or ``-`` reads stdin. Any non-``[EVT]`` line ([CAP]
capture text, boot banners, blanks) is ignored; a line that claims to be
``[EVT]`` but does not carry a JSON object is a named error on stderr and a
non-zero exit -- the same stance as ``check_evt_log.py``, a corrupt log is
never silently reported over. An empty selection is NOT an error (exit 0):
the tool prints ``no events selected`` -- it is a report, not a checker.

Columns (fixed):

    time | track id | source | state change | d_AC · ttc | risk level | R4 emitted

- **time**: the event's ``epoch_ms`` rendered ISO-8601 UTC with millisecond
  precision. When ``epoch_ms`` is absent, the monotonic ``mono_ms`` is shown
  as ``mono+<ms>`` and the row is flagged in a footnote -- the two domains
  are never mixed silently.
- **track id / source**: from the ``track_transition`` / ``risk_transition``
  payload, or from the embedded R4 body's ``object`` on ``r4_tx``.
- **state change**: ``from->to`` on ``track_transition`` rows, ``-`` otherwise.
- **d_AC · ttc**: the ``risk_transition`` payload's ``d_ac``/``ttc``
  (``-`` for either when absent or null); ``-`` on other rows.
- **risk level**: ``risk_transition``'s ``to``, or the ``r4_tx`` body's
  ``riskState``.
- **R4 emitted**: whether an ``r4_tx`` accompanied the transition. Pairing
  rule (designated): an ``r4_tx`` row itself shows ``yes``; a
  ``risk_transition`` row shows ``yes`` iff the next ``r4_tx`` in stream
  order precedes the next ``risk_transition``. A ``track_transition`` row
  shows ``-`` -- the pairing rule is defined only for the risk pair.

``--summary`` appends a footer counting: tracks admitted (distinct ids that
reached state ``tracked``), risk transitions per ``to`` level, and R4 events
sent (every ``r4_tx`` line, whether or not ``send_ok``).

Payload fields consumed (planner-designated, matching the C++ emitters):
``track_transition`` {id, source, from, to}; ``risk_transition`` {id, source,
from, to, d_ac, ttc}; ``r4_tx`` {body: <R4 warning event object>}. The
``track_expire`` payload ({id, source, distance}) is implementation-chosen
and NOT yet ratified -- this tool tolerates the event by name only and never
parses its payload fields.
"""

from __future__ import annotations

import argparse
import json
import sys
from collections import Counter
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Optional

EVT_PREFIX = "[EVT] "
SELECTED = ("track_transition", "risk_transition", "r4_tx")
HEADERS = ("time", "track id", "source", "state change", "d_AC · ttc",
           "risk level", "R4 emitted")
ABSENT = "-"
MONO_FLAG = "*"   # appended to a mono_ms-fallback time cell


@dataclass(frozen=True)
class Row:
    lineno: int
    name: str
    cells: tuple
    mono_fallback: bool


def err(msg: str) -> None:
    print(f"[ERR] {msg}", file=sys.stderr, flush=True)


def read_lines(logfile: Optional[str]) -> tuple:
    if logfile in (None, "-"):
        return "<stdin>", sys.stdin.read().splitlines()
    path = Path(logfile).expanduser()
    try:
        text = path.read_text(encoding="utf-8", errors="replace")
    except OSError as e:
        err(f"cannot read logfile {path}: {e}")
        sys.exit(2)
    return str(path), text.splitlines()


def extract_events(lines: list) -> tuple:
    """(events, malformed): events are (lineno, obj) for every well-formed
    ``[EVT] {json}`` line; anything else non-[EVT] is ignored as noise."""
    events: list = []
    malformed: list = []
    for lineno, raw in enumerate(lines, start=1):
        stripped = raw.strip()
        if not stripped.startswith(EVT_PREFIX):
            continue
        try:
            obj = json.loads(stripped[len(EVT_PREFIX):])
        except ValueError as e:
            malformed.append(f"line {lineno}: malformed [EVT] JSON - {e}")
            continue
        if not isinstance(obj, dict):
            malformed.append(f"line {lineno}: [EVT] payload is "
                             f"{type(obj).__name__}, not a JSON object")
            continue
        events.append((lineno, obj))
    return events, malformed


def fmt_time(obj: dict) -> tuple:
    """(cell, mono_fallback). epoch_ms -> ISO-8601 UTC with ms; an absent
    epoch falls back to mono_ms, flagged."""
    epoch = obj.get("epoch_ms")
    if isinstance(epoch, (int, float)) and not isinstance(epoch, bool):
        ms = int(epoch)
        stamp = datetime.fromtimestamp(ms / 1000.0, tz=timezone.utc)
        return (stamp.strftime("%Y-%m-%dT%H:%M:%S") + f".{ms % 1000:03d}Z",
                False)
    mono = obj.get("mono_ms")
    if isinstance(mono, (int, float)) and not isinstance(mono, bool):
        return f"mono+{int(mono)}ms{MONO_FLAG}", True
    return ABSENT, False


def fmt_number(value: object) -> str:
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        return ABSENT
    return f"{value:g}"


def str_or_absent(value: object) -> str:
    return value if isinstance(value, str) and value else ABSENT


def r4_accompanies(index: int, names: list) -> bool:
    """The designated pairing rule for a risk_transition at ``index``: yes iff
    the next r4_tx in stream order precedes the next risk_transition."""
    for name in names[index + 1:]:
        if name == "r4_tx":
            return True
        if name == "risk_transition":
            return False
    return False


def build_rows(selected: list) -> list:
    """One Row per selected event, in stream (chronological) order."""
    names = [obj.get("event") for _, obj in selected]
    rows: list = []
    for i, (lineno, obj) in enumerate(selected):
        name = obj.get("event")
        payload = obj.get("payload")
        payload = payload if isinstance(payload, dict) else {}
        time_cell, mono = fmt_time(obj)
        if name == "track_transition":
            frm, to = payload.get("from"), payload.get("to")
            change = (f"{frm}->{to}"
                      if isinstance(frm, str) and isinstance(to, str) else ABSENT)
            cells = (time_cell, str_or_absent(payload.get("id")),
                     str_or_absent(payload.get("source")), change,
                     ABSENT, ABSENT, ABSENT)
        elif name == "risk_transition":
            d_ac = fmt_number(payload.get("d_ac"))
            ttc = fmt_number(payload.get("ttc"))
            cells = (time_cell, str_or_absent(payload.get("id")),
                     str_or_absent(payload.get("source")), ABSENT,
                     f"{d_ac} · {ttc}", str_or_absent(payload.get("to")),
                     "yes" if r4_accompanies(i, names) else "no")
        else:  # r4_tx
            body = payload.get("body")
            body = body if isinstance(body, dict) else {}
            r3 = body.get("object")
            r3 = r3 if isinstance(r3, dict) else {}
            cells = (time_cell, str_or_absent(r3.get("id")),
                     str_or_absent(r3.get("source")), ABSENT, ABSENT,
                     str_or_absent(body.get("riskState")), "yes")
        rows.append(Row(lineno=lineno, name=name, cells=cells,
                        mono_fallback=mono))
    return rows


def render_table(rows: list) -> None:
    widths = [len(h) for h in HEADERS]
    for row in rows:
        widths = [max(w, len(c)) for w, c in zip(widths, row.cells)]
    line = " | ".join(h.ljust(w) for h, w in zip(HEADERS, widths))
    print(line)
    print("-+-".join("-" * w for w in widths))
    for row in rows:
        print(" | ".join(c.ljust(w) for c, w in zip(row.cells, widths)))
    flagged = sum(1 for row in rows if row.mono_fallback)
    if flagged:
        print(f"({MONO_FLAG}) {flagged} row(s) show monotonic mono_ms - "
              f"epoch_ms absent on those events")


def render_summary(selected: list) -> None:
    admitted = set()
    per_level: Counter = Counter()
    r4_sent = 0
    for _, obj in selected:
        name = obj.get("event")
        payload = obj.get("payload")
        payload = payload if isinstance(payload, dict) else {}
        if name == "track_transition" and payload.get("to") == "tracked":
            track_id = payload.get("id")
            if isinstance(track_id, str):
                admitted.add(track_id)
        elif name == "risk_transition":
            level = payload.get("to")
            per_level[level if isinstance(level, str) else "?"] += 1
        elif name == "r4_tx":
            r4_sent += 1
    print()
    print("summary:")
    print(f"  tracks admitted: {len(admitted)}"
          + (f" ({', '.join(sorted(admitted))})" if admitted else ""))
    levels = (", ".join(f"{lvl}={n}" for lvl, n in sorted(per_level.items()))
              or "none")
    print(f"  risk transitions per level: {levels}")
    print(f"  R4 events sent: {r4_sent}")


def main(argv: list) -> int:
    parser = argparse.ArgumentParser(
        prog="event_report.py",
        description="Render an ADA [EVT] log as the collision-risk event "
                    "list (R18); see the module docstring for the columns.")
    parser.add_argument("logfile", nargs="?", default=None,
                        help="log to read; omitted or '-' reads stdin")
    parser.add_argument("--summary", action="store_true",
                        help="append the tracks/levels/R4 count footer")
    args = parser.parse_args(argv)

    source, lines = read_lines(args.logfile)
    events, malformed = extract_events(lines)
    for failure in malformed:
        err(failure)

    selected = [(lineno, obj) for lineno, obj in events
                if obj.get("event") in SELECTED]
    if not selected:
        print("no events selected")
    else:
        rows = build_rows(selected)
        render_table(rows)
    if args.summary:
        render_summary(selected)

    if malformed:
        err(f"{len(malformed)} malformed [EVT] line(s) in {source}")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
