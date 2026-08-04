#!/usr/bin/env python3
"""``[EVT]``-stream assertion for the ADA ECU -- the R18 evidence checker.

The ADA counterpart of ``tools/comms_check/check_v2x_log.py``: turns a saved
ADA ``[EVT]`` stream (CI-captured stdout, an ``EVENT_LOG_PATH`` file, or a
CarSky View Log export) into a pass/fail. Test equipment only, never deployed.

Any non-``[EVT]`` line is ignored, so ``[CAP]`` capture text, boot banners,
tcpdump output and blank lines may be interleaved freely. A line that claims
to be ``[EVT]`` but does not carry a JSON object is a failure, not noise.

Usage::

    check_evt_log.py --admission [logfile] [--confirm-hits N]
                     [--gate-enter-m M] [--gate-exit-m M] [--min-transitions N]
    check_evt_log.py --expect-no-tracks [logfile]
    check_evt_log.py [--admission] --fusion --both-tracks
                     [--r4-schema PATH] [logfile]

``logfile`` omitted or ``-`` reads stdin. At least one check flag is
required; ``--admission`` and ``--expect-no-tracks`` stay mutually
exclusive, while the Phase 4 flags ``--fusion``, ``--both-tracks`` and
``--r4-schema`` are additive and compose freely with ``--admission`` (the
e2e lane passes them together).

**Mode --admission** asserts the observed ``track_transition`` sequence per
track id is a legal path through the R13 admission state machine
(``ADA_ECU/doc/phase2-4-ada-ecu-admission.puml``):

* per-id chain continuity: each transition's ``from`` equals the id's current
  state, starting from ``not_tracked``;
* no ``not_tracked -> tracked`` jump (legal only when ``--confirm-hits`` is 1,
  where the tentative range 1..CONFIRM_HITS-1 is empty);
* the intermediate in-gate hits 2..CONFIRM_HITS-1 are the machine's silent
  ``counted`` self-loop (``track_store.cpp``: state-changing edges emit, the
  self-loops do not), so the hit count is NOT stream-observable and a
  ``tentative -> tracked`` promotion is checked for chain continuity and
  in-gate distance only -- with ``--confirm-hits`` >= 2 it legally follows the
  admission directly in the stream;
* no drop inside the hysteresis band: a distance-based
  ``tracked -> not_tracked`` requires distance > ``--gate-exit-m``; admissions
  and tentative holds require distance <= ``--gate-enter-m``; tracked
  refreshes require distance <= ``--gate-exit-m``;
* a timeout drop (``distance`` absent/null, or ``reason`` naming
  expiry/timeout) is legal from tentative and tracked;
* at least one full ``not_tracked -> tentative -> tracked -> not_tracked``
  cycle observed per named source (each distinct ``source`` value seen);
* at least ``--min-transitions`` transitions in total (default ``1``), so an
  empty stream can never pass vacuously.

The first illegal edge, in stream order, is named in the failure output.

**Mode --expect-no-tracks** exits non-zero if any ``track_transition``
appears -- the "mock off yields no tracks" arm.

**Mode --fusion** asserts the Phase 4 relay chain. Per relayed track id
(id starting ``v2x:``): at least one ``r2_ingest`` in the stream (global),
a ``track_transition`` for that id, and >= 1 ``assessment`` whose
``trackId`` matches. Globally, the edge-triggered risk/emit pairing (D5):
every ``risk_transition`` has exactly one matching ``r4_tx`` -- the next
``r4_tx`` after it and before the next ``risk_transition`` -- and no
``r4_tx`` appears without a preceding unconsumed ``risk_transition``. The
b_unknown arm: when any ``assess_skipped_b_unknown`` appears and no
``risk_transition`` ever does, the ``r4_tx`` count must be 0. The first
violation is named with its line number.

**Mode --both-tracks** exits 0 only when all four hold, else non-zero
naming which is missing: (1) some own-sensor id has a ``track_transition``
to ``tracked`` (source ``own_sensor``) AND an ``own_sensor_ingest`` whose
``payload.object`` carries all nine R3 fields -- the tracked-state +
full-fields evidence is a JOIN of those two lines on the track id; by
design no single ``[EVT]`` line carries both facts; (2) some ``v2x:`` id
has a ``track_transition`` to ``tracked`` with source ``v2x_relayed``;
(3) >= 1 ``r4_tx`` whose embedded ``body.object`` is a full nine-field R3
TrackedObject with source ``v2x_relayed``; (4) that same ``r4_tx`` body
carries non-null numeric ``geometry.vehicleB`` (x, y).

**Flag --r4-schema PATH** validates every ``r4_tx`` embedded ``body``
against the JSON schema at PATH (the synced ``r4-ada-ivi.schema.json``);
``jsonschema`` is imported lazily and only when the flag is passed, so the
plain modes stay standard-library only.

**Every mode:** zero ``[EVT]`` lines is a failure, never a pass.

Frozen ``[EVT]`` fields this script depends on
(``ADA_ECU/src/log/event_log.hpp`` -- renaming any breaks this consumer):
line prefix ``[EVT] `` followed by one JSON object with ``event``,
``mono_ms``, ``epoch_ms``, ``counters`` and ``payload``; the 12-name event
vocabulary plus ``detector_disabled`` -- the detector reader's disabled-arm
line, pending D8 ratification (an unknown name is a failure, not a
tolerated extra); on
``track_transition`` a payload with ``id``, ``source``, ``from``, ``to``,
``distance`` and ``reason``. ``reason`` is parsed only to recognise
timeout/expiry drops; its full vocabulary stays free diagnostics.

Output: one ``[CHK] <group>: PASS|FAIL`` line per assertion group, then a
final ``[CHK] PASS ...`` or ``[CHK] FAIL: <first failure>``. Exit 0 when
every group passes, 1 on any assertion failure, 2 on a bad invocation.
"""

from __future__ import annotations

import argparse
import json
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import Callable, Optional

EVT_PREFIX = "[EVT] "

# Emitted once by detector_reader.cpp's disabled arm (DETECTOR_ENABLED=false,
# no spawn); a 13th name pending D8 ratification alongside the frozen twelve.
EVENT_DETECTOR_DISABLED = "detector_disabled"

# Frozen D8 event vocabulary (event_log.hpp events::kAll) -- an unknown name
# is a failure. Phase 2 emits the first eight; the last four are Phase 4.
EVENT_NAMES = frozenset({
    "detector_spawn", "detector_eof", "detector_restart", "own_sensor_ingest",
    "r2_ingest", "parse_reject", "track_transition", "track_expire",
    "assessment", "assess_skipped_b_unknown", "risk_transition", "r4_tx",
    EVENT_DETECTOR_DISABLED,
})
MANDATORY_FIELDS = ("event", "mono_ms", "epoch_ms", "counters", "payload")

# The three R13 states of the admission diagram.
STATE_NAMES = frozenset({"not_tracked", "tentative", "tracked"})

# Defaults mirror the R13 diagram's noted values (CONFIRM_HITS, GATE_ENTER_M
# 30 m, GATE_EXIT_M 35 m). They are CLI flags, never literals in the checks --
# a run against a differently configured node passes the node's values here.
DEFAULT_CONFIRM_HITS = 3
DEFAULT_GATE_ENTER_M = 30.0
DEFAULT_GATE_EXIT_M = 35.0
DEFAULT_MIN_TRANSITIONS = 1

# A drop whose reason names one of these is the diagram's timeout edge, legal
# regardless of any distance carried (step() evaluates the timeout first).
TIMEOUT_REASON_MARKERS = ("expire", "timeout")

MAX_REPORTED_PER_GROUP = 5   # keep a broken log readable; the count is stated

# The nine frozen R3 TrackedObject fields (contracts/r3-tracked-object.schema.json)
# -- what --both-tracks means by "a full R3 object".
R3_FIELDS = ("id", "class", "source", "position", "distance", "speed",
             "confidence", "state", "timestamps")

# Relayed track ids carry this prefix (r2 adapter naming); --fusion keys on it.
V2X_ID_PREFIX = "v2x:"


def log(msg: str) -> None:
    print(f"[CHK] {msg}", flush=True)


def die(msg: str, code: int) -> None:
    print(f"[ERR] {msg}", file=sys.stderr, flush=True)
    sys.exit(code)


@dataclass(frozen=True)
class Event:
    """One parsed ``[EVT]`` line. ``index`` orders events among themselves;
    ``lineno`` points back into the input file for human diagnosis."""
    index: int
    lineno: int
    name: str
    counters: dict
    obj: dict

    def counter(self) -> Optional[int]:
        """This event's own cumulative counter, when present and integral."""
        value = self.counters.get(self.name)
        return value if isinstance(value, int) and not isinstance(value, bool) else None


@dataclass(frozen=True)
class Transition:
    """One ``track_transition`` payload, already field-checked."""
    event: Event
    track_id: str
    source: str
    from_state: str
    to_state: str
    distance: Optional[float]   # None = absent/null (an expiry tick)
    reason: str

    @property
    def lineno(self) -> int:
        return self.event.lineno

    def edge(self) -> str:
        return f"{self.from_state} -> {self.to_state}"

    def is_timeout(self) -> bool:
        if self.distance is None:
            return True
        return any(marker in self.reason for marker in TIMEOUT_REASON_MARKERS)


@dataclass(frozen=True)
class AdmissionConfig:
    """The thresholds the legality checks run against (all from CLI flags)."""
    confirm_hits: int
    gate_enter_m: float
    gate_exit_m: float
    min_transitions: int


@dataclass
class TrackWalk:
    """Per-track-id replay state while walking the transition stream."""
    state: str = "not_tracked"
    hits: int = 0            # in-gate updates accumulated while tentative
    cycle_stage: int = 0     # 0 idle, 1 tentative seen, 2 tracked via tentative


@dataclass
class Checker:
    """Collects per-group verdicts; the first failure recorded is the
    reported first illegal edge / first missing link."""
    failures: list = field(default_factory=list)

    def group(self, name: str, failures: list, detail: str) -> None:
        if failures:
            log(f"{name}: FAIL ({len(failures)}) - {failures[0]}")
            for extra in failures[1:MAX_REPORTED_PER_GROUP]:
                log(f"{name}:   also - {extra}")
            if len(failures) > MAX_REPORTED_PER_GROUP:
                log(f"{name}:   ... {len(failures) - MAX_REPORTED_PER_GROUP} more")
            self.failures.extend(failures)
        else:
            log(f"{name}: PASS - {detail}")


def read_lines(logfile: Optional[str]) -> tuple[str, list]:
    if logfile in (None, "-"):
        return "<stdin>", sys.stdin.read().splitlines()
    path = Path(logfile).expanduser()
    try:
        text = path.read_text(encoding="utf-8", errors="replace")
    except OSError as e:
        die(f"cannot read logfile {path}: {e}", 2)
    return str(path), text.splitlines()


def extract_events(lines: list) -> tuple[list, list]:
    """Pull the ``[EVT] {json}`` lines out of an arbitrarily noisy log.
    Returns (events, malformed)."""
    events: list = []
    malformed: list = []
    for lineno, raw in enumerate(lines, start=1):
        stripped = raw.strip()
        if not stripped.startswith(EVT_PREFIX):
            continue
        payload = stripped[len(EVT_PREFIX):]
        try:
            obj = json.loads(payload)
        except ValueError as e:
            malformed.append(f"line {lineno}: malformed [EVT] JSON - {e}")
            continue
        if not isinstance(obj, dict):
            malformed.append(
                f"line {lineno}: [EVT] payload is {type(obj).__name__}, not a JSON object")
            continue
        name = obj.get("event")
        counters = obj.get("counters")
        events.append(Event(index=len(events), lineno=lineno,
                            name=name if isinstance(name, str) else "",
                            counters=counters if isinstance(counters, dict) else {},
                            obj=obj))
    return events, malformed


def check_fields(chk: Checker, events: list) -> None:
    """Mandatory frozen fields + a well-formed counters object on every event."""
    failures = []
    for ev in events:
        missing = [f for f in MANDATORY_FIELDS if f not in ev.obj]
        if missing:
            failures.append(
                f"line {ev.lineno}: [EVT] missing mandatory field(s) {', '.join(missing)}")
            continue
        if not isinstance(ev.obj["counters"], dict):
            failures.append(f"line {ev.lineno}: 'counters' is not a JSON object")
            continue
        if not isinstance(ev.obj["payload"], dict):
            failures.append(f"line {ev.lineno}: 'payload' is not a JSON object")
    chk.group("fields", failures,
              f"all {len(events)} event(s) carry {'/'.join(MANDATORY_FIELDS)}")


def check_vocabulary(chk: Checker, events: list) -> None:
    failures = [f"line {ev.lineno}: unknown event name {ev.obj.get('event')!r} "
                f"(frozen vocabulary: {', '.join(sorted(EVENT_NAMES))})"
                for ev in events if ev.name not in EVENT_NAMES]
    chk.group("vocabulary", failures,
              f"all event names within the frozen {len(EVENT_NAMES)}-name vocabulary")


def check_counters(chk: Checker, events: list) -> None:
    """Each event name's own counter advances by exactly 1 between consecutive
    occurrences -- a bigger jump means log lines were lost, which would
    otherwise surface as a phantom illegal edge."""
    failures = []
    names = sorted({ev.name for ev in events if ev.name in EVENT_NAMES})
    for name in names:
        seen = [ev for ev in events if ev.name == name]
        for prev_ev, ev in zip(seen, seen[1:]):
            before, now = prev_ev.counter(), ev.counter()
            if before is None or now is None:
                continue
            if now != before + 1:
                failures.append(
                    f"line {ev.lineno}: counters.{name} jumped {before} -> {now} "
                    f"- {now - before - 1} {name} line(s) missing from the stream")
    chk.group("counters", failures,
              "every event's own counter advances by exactly 1 (no lost lines)")


def number_or_none(value: object) -> tuple:
    """(ok, parsed) -- None/absent stays None; a bool or non-number is not ok."""
    if value is None:
        return True, None
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        return False, None
    return True, float(value)


def collect_transitions(chk: Checker, events: list) -> list:
    """Field-checks every ``track_transition`` payload and returns the
    well-formed ones as Transition objects, in stream order."""
    failures: list = []
    transitions: list = []
    for ev in events:
        if ev.name != "track_transition":
            continue
        payload = ev.obj.get("payload")
        if not isinstance(payload, dict):
            continue    # already reported by check_fields
        track_id = payload.get("id")
        source = payload.get("source")
        from_state = payload.get("from")
        to_state = payload.get("to")
        ok_distance, distance = number_or_none(payload.get("distance"))
        reason = payload.get("reason")
        bad = []
        if not isinstance(track_id, str) or not track_id:
            bad.append(f"'id' missing or not a string ({track_id!r})")
        if not isinstance(source, str) or not source:
            bad.append(f"'source' missing or not a string ({source!r})")
        if from_state not in STATE_NAMES:
            bad.append(f"'from' not a state name ({from_state!r})")
        if to_state not in STATE_NAMES:
            bad.append(f"'to' not a state name ({to_state!r})")
        if not ok_distance:
            bad.append(f"'distance' not a number or null ({payload.get('distance')!r})")
        if bad:
            failures.append(f"line {ev.lineno}: track_transition payload invalid - "
                            + "; ".join(bad))
            continue
        transitions.append(Transition(
            event=ev, track_id=track_id, source=source,
            from_state=from_state, to_state=to_state, distance=distance,
            reason=reason if isinstance(reason, str) else ""))
    chk.group("transition-payloads", failures,
              f"{len(transitions)} track_transition payload(s) carry "
              f"id/source/from/to/distance/reason well-formed")
    return transitions


def resync(walk: TrackWalk, t: Transition, cfg: AdmissionConfig) -> None:
    """After a chain break, adopt the transition's claimed target state so one
    defect does not cascade into a failure per subsequent line."""
    walk.state = t.to_state
    walk.cycle_stage = 0
    if t.to_state == "tentative":
        walk.hits = 1
    elif t.to_state == "tracked":
        walk.hits = cfg.confirm_hits
    else:
        walk.hits = 0


def apply_edge(walk: TrackWalk, t: Transition, cfg: AdmissionConfig) -> Optional[str]:
    """Validates one transition against the R13 machine and advances the
    walk. Returns a failure message for an illegal edge, else None."""
    frm, to, d = t.from_state, t.to_state, t.distance
    where = f"line {t.lineno}: track {t.track_id!r} ({t.source})"

    if frm != walk.state:
        expected = walk.state
        resync(walk, t, cfg)
        return (f"{where} edge {t.edge()} breaks the chain - "
                f"the track's observed state is {expected!r}, not {frm!r}")

    if frm == "not_tracked" and to == "tentative":
        if d is None:
            resync(walk, t, cfg)
            return f"{where} admission {t.edge()} carries no distance"
        if d > cfg.gate_enter_m:
            resync(walk, t, cfg)
            return (f"{where} admission {t.edge()} at distance {d} m > "
                    f"GATE_ENTER_M {cfg.gate_enter_m} m - out-of-gate admission")
        walk.state, walk.hits, walk.cycle_stage = "tentative", 1, 1
        return None

    if frm == "not_tracked" and to == "tracked":
        if cfg.confirm_hits != 1:
            resync(walk, t, cfg)
            return (f"{where} illegal edge {t.edge()} - the not_tracked -> tracked "
                    f"jump is legal only with CONFIRM_HITS 1, checker runs with "
                    f"{cfg.confirm_hits}")
        if d is None or d > cfg.gate_enter_m:
            resync(walk, t, cfg)
            return (f"{where} admission {t.edge()} at distance {d} m not within "
                    f"GATE_ENTER_M {cfg.gate_enter_m} m")
        walk.state, walk.hits, walk.cycle_stage = "tracked", 1, 0
        return None

    if frm == "tentative" and to == "tentative":
        if d is None or d > cfg.gate_enter_m:
            resync(walk, t, cfg)
            return (f"{where} tentative hold {t.edge()} at distance {d} m not within "
                    f"GATE_ENTER_M {cfg.gate_enter_m} m")
        walk.hits += 1
        if walk.hits >= cfg.confirm_hits:
            hits = walk.hits
            resync(walk, t, cfg)
            return (f"{where} edge {t.edge()} at {hits} in-gate update(s) - the machine "
                    f"promotes at CONFIRM_HITS {cfg.confirm_hits}, this hold is illegal")
        return None

    if frm == "tentative" and to == "tracked":
        # The hits between admission and promotion are the machine's silent
        # `counted` self-loop (13.2.4.3 ruling: state-changing edges emit,
        # self-loops do not), so the promotion count cannot be asserted from
        # the stream -- only continuity (checked above) and the in-gate
        # distance can.
        if d is None or d > cfg.gate_enter_m:
            resync(walk, t, cfg)
            return (f"{where} promotion {t.edge()} at distance {d} m not within "
                    f"GATE_ENTER_M {cfg.gate_enter_m} m")
        walk.hits = cfg.confirm_hits
        walk.state = "tracked"
        walk.cycle_stage = 2 if walk.cycle_stage == 1 else 0
        return None

    if frm == "tracked" and to == "tracked":
        if d is None or d > cfg.gate_exit_m:
            resync(walk, t, cfg)
            return (f"{where} refresh {t.edge()} at distance {d} m not within "
                    f"GATE_EXIT_M {cfg.gate_exit_m} m")
        return None

    if frm == "tentative" and to == "not_tracked":
        if not t.is_timeout() and t.distance <= cfg.gate_enter_m:
            resync(walk, t, cfg)
            return (f"{where} drop {t.edge()} at distance {t.distance} m <= "
                    f"GATE_ENTER_M {cfg.gate_enter_m} m - in-gate drop")
        walk.state, walk.hits, walk.cycle_stage = "not_tracked", 0, 0
        return None

    if frm == "tracked" and to == "not_tracked":
        completed = walk.cycle_stage == 2
        if not t.is_timeout() and t.distance <= cfg.gate_exit_m:
            band = t.distance > cfg.gate_enter_m
            resync(walk, t, cfg)
            return (f"{where} drop {t.edge()} at distance {t.distance} m <= "
                    f"GATE_EXIT_M {cfg.gate_exit_m} m - "
                    + ("drop inside the hysteresis band "
                       f"({cfg.gate_enter_m}, {cfg.gate_exit_m}] m" if band
                       else "in-gate drop"))
        walk.state, walk.hits = "not_tracked", 0
        walk.cycle_stage = 3 if completed else 0    # 3 = full cycle done
        return None

    resync(walk, t, cfg)
    return f"{where} illegal edge {t.edge()} - no such edge in the R13 machine"


def check_legal_paths(chk: Checker, transitions: list, cfg: AdmissionConfig) -> dict:
    """Walks every transition in stream order; returns per-source cycle
    completion for the cycle check."""
    failures: list = []
    walks: dict = {}
    cycled: dict = {}   # source -> bool: a full NT->TE->TR->NT cycle seen
    for t in transitions:
        walk = walks.setdefault(t.track_id, TrackWalk())
        cycled.setdefault(t.source, False)
        failure = apply_edge(walk, t, cfg)
        if failure is not None:
            failures.append(failure)
            continue
        if walk.cycle_stage == 3:
            cycled[t.source] = True
            walk.cycle_stage = 0
    chk.group("legal-path", failures,
              f"{len(transitions)} transition(s) across {len(walks)} track id(s) all "
              f"legal R13 edges (CONFIRM_HITS {cfg.confirm_hits}, gates "
              f"{cfg.gate_enter_m}/{cfg.gate_exit_m} m)")
    return cycled


def check_full_cycles(chk: Checker, cycled: dict) -> None:
    failures = [f"source {source!r} never completed a full "
                f"not_tracked -> tentative -> tracked -> not_tracked cycle"
                for source, done in sorted(cycled.items()) if not done]
    if not cycled:
        failures.append("no track_transition events at all - no source observed, "
                        "nothing admitted")
    chk.group("full-cycle", failures,
              "full not_tracked -> tentative -> tracked -> not_tracked cycle seen "
              f"for every named source ({', '.join(sorted(cycled))})")


def run_admission(chk: Checker, events: list, cfg: AdmissionConfig) -> None:
    transitions = collect_transitions(chk, events)
    chk.group("volume",
              [] if len(transitions) >= cfg.min_transitions else
              [f"track_transition events: {len(transitions)} < {cfg.min_transitions} "
               f"required by --min-transitions - a vacuous pass is forbidden"],
              f"{len(transitions)} track_transition event(s) >= "
              f"{cfg.min_transitions} required")
    cycled = check_legal_paths(chk, transitions, cfg)
    check_full_cycles(chk, cycled)


def run_expect_no_tracks(chk: Checker, events: list, _cfg: AdmissionConfig) -> None:
    offenders = [ev for ev in events if ev.name == "track_transition"]
    failures = []
    for ev in offenders:
        payload = ev.obj.get("payload")
        track_id = payload.get("id") if isinstance(payload, dict) else None
        failures.append(f"line {ev.lineno}: track_transition present "
                        f"(id={track_id!r}) - expected none")
    chk.group("no-tracks", failures,
              f"0 track_transition events across {len(events)} [EVT] line(s)")


# --- Phase 4 additive checks -------------------------------------------------
# NOTE: track_expire's payload fields ({"id", "source", "distance"}) are
# implementation-chosen and NOT yet ratified. Every check below tolerates the
# event by name only (it is in EVENT_NAMES) and never parses its payload.


def event_payload(ev: Event) -> dict:
    payload = ev.obj.get("payload")
    return payload if isinstance(payload, dict) else {}


def has_all_r3_fields(obj: object) -> bool:
    return isinstance(obj, dict) and all(f in obj for f in R3_FIELDS)


def is_finite_number(value: object) -> bool:
    return isinstance(value, (int, float)) and not isinstance(value, bool)


def r4_body(ev: Event) -> Optional[dict]:
    """The embedded R4 warning-event object of an r4_tx line, or None."""
    body = event_payload(ev).get("body")
    return body if isinstance(body, dict) else None


def check_fusion_chain(chk: Checker, events: list) -> None:
    """Per relayed (v2x:) track id: r2_ingest present globally, a
    track_transition for the id, >= 1 assessment whose trackId matches."""
    failures: list = []
    relayed_first: dict = {}   # id -> first track_transition Event
    for ev in events:
        if ev.name != "track_transition":
            continue
        track_id = event_payload(ev).get("id")
        if isinstance(track_id, str) and track_id.startswith(V2X_ID_PREFIX):
            relayed_first.setdefault(track_id, ev)
    r2_count = sum(1 for ev in events if ev.name == "r2_ingest")
    assessed_ids = {event_payload(ev).get("trackId")
                    for ev in events if ev.name == "assessment"}
    for track_id, first in sorted(relayed_first.items(),
                                  key=lambda item: item[1].lineno):
        if r2_count == 0:
            failures.append(
                f"line {first.lineno}: relayed track {track_id!r} transitions "
                f"with zero r2_ingest events in the stream - no R2 ever ingested")
        if track_id not in assessed_ids:
            failures.append(
                f"line {first.lineno}: relayed track {track_id!r} has no "
                f"assessment whose trackId matches")
    chk.group("fusion-chain", failures,
              f"{len(relayed_first)} relayed (v2x:) track id(s) each covered by "
              f"r2_ingest ({r2_count}) and a matching assessment")


def check_fusion_pairing(chk: Checker, events: list) -> None:
    """Edge-triggered emit pairing (D5): every risk_transition is consumed by
    exactly one r4_tx before the next risk_transition; an r4_tx never appears
    without a preceding unconsumed risk_transition."""
    failures: list = []
    pending: Optional[Event] = None   # the unconsumed risk_transition
    for ev in events:
        if ev.name == "risk_transition":
            if pending is not None:
                failures.append(
                    f"line {pending.lineno}: risk_transition has no matching "
                    f"r4_tx before the next risk_transition (line {ev.lineno})")
            pending = ev
        elif ev.name == "r4_tx":
            if pending is None:
                failures.append(
                    f"line {ev.lineno}: r4_tx without a preceding unconsumed "
                    f"risk_transition - the emit is edge-triggered (D5)")
            else:
                pending = None
    if pending is not None:
        failures.append(f"line {pending.lineno}: risk_transition has no "
                        f"matching r4_tx before the end of the stream")
    chk.group("fusion-pairing", failures,
              "every risk_transition consumed by exactly one r4_tx, "
              "no unpaired r4_tx (edge-triggered, D5)")


def check_fusion_b_unknown(chk: Checker, events: list) -> None:
    """The b_unknown-run arm: skips observed and no risk_transition ever ->
    the run must have emitted zero r4_tx."""
    skips = [ev for ev in events if ev.name == "assess_skipped_b_unknown"]
    transitions = sum(1 for ev in events if ev.name == "risk_transition")
    r4s = [ev for ev in events if ev.name == "r4_tx"]
    failures = []
    if skips and transitions == 0 and r4s:
        failures.append(
            f"line {r4s[0].lineno}: {len(r4s)} r4_tx event(s) in a b_unknown "
            f"run (assess_skipped_b_unknown present, zero risk_transition) - "
            f"expected none")
    chk.group("fusion-b-unknown", failures,
              f"{len(skips)} assess_skipped_b_unknown / {transitions} "
              f"risk_transition / {len(r4s)} r4_tx consistent")


def run_fusion(chk: Checker, events: list, _cfg: AdmissionConfig) -> None:
    check_fusion_chain(chk, events)
    check_fusion_pairing(chk, events)
    check_fusion_b_unknown(chk, events)


def run_both_tracks(chk: Checker, events: list, _cfg: AdmissionConfig) -> None:
    """The four both-tracks facts; the missing one(s) are named. Fact (1) is a
    JOIN on the track id of two lines -- a track_transition to 'tracked' and an
    own_sensor_ingest with the full object -- because no single [EVT] line
    carries both the tracked state and all nine R3 fields (see docstring)."""
    tracked_own_ids = set()
    tracked_v2x = False
    ingest_full_ids = set()
    r4_full_relayed: list = []   # r4_tx Events satisfying fact (3)
    for ev in events:
        payload = event_payload(ev)
        if ev.name == "track_transition":
            track_id = payload.get("id")
            if payload.get("to") != "tracked" or not isinstance(track_id, str):
                continue
            if payload.get("source") == "own_sensor":
                tracked_own_ids.add(track_id)
            elif (payload.get("source") == "v2x_relayed"
                  and track_id.startswith(V2X_ID_PREFIX)):
                tracked_v2x = True
        elif ev.name == "own_sensor_ingest":
            obj = payload.get("object")
            if has_all_r3_fields(obj):
                ingest_full_ids.add(obj["id"])
        elif ev.name == "r4_tx":
            body = r4_body(ev)
            obj = body.get("object") if body else None
            if (has_all_r3_fields(obj)
                    and obj.get("source") == "v2x_relayed"):
                r4_full_relayed.append(ev)

    def vehicle_b_ok(ev: Event) -> bool:
        geometry = (r4_body(ev) or {}).get("geometry")
        vb = geometry.get("vehicleB") if isinstance(geometry, dict) else None
        return (isinstance(vb, dict) and is_finite_number(vb.get("x"))
                and is_finite_number(vb.get("y")))

    failures = []
    joined = tracked_own_ids & ingest_full_ids
    if not joined:
        failures.append(
            "(1) no own_sensor id both reaches state 'tracked' "
            f"(track_transition; ids: {sorted(tracked_own_ids) or 'none'}) and "
            f"appears in an own_sensor_ingest carrying all nine R3 fields "
            f"(ids: {sorted(ingest_full_ids) or 'none'}) - the JOIN is empty")
    if not tracked_v2x:
        failures.append("(2) no v2x: id reaches state 'tracked' with source "
                        "v2x_relayed")
    if not r4_full_relayed:
        failures.append("(3) no r4_tx whose body.object is a full nine-field "
                        "R3 TrackedObject with source v2x_relayed")
    elif not any(vehicle_b_ok(ev) for ev in r4_full_relayed):
        failures.append(
            f"(4) no such r4_tx carries non-null numeric geometry.vehicleB "
            f"(x, y) - first candidate at line {r4_full_relayed[0].lineno}")
    chk.group("both-tracks", failures,
              f"own_sensor tracked+full join {sorted(joined)}, v2x_relayed "
              f"tracked, {len(r4_full_relayed)} full relayed r4_tx with "
              f"geometry.vehicleB")


def build_r4_validator(schema_path: Path):
    """A validator whose relative $refs (the r3 schema) resolve in
    schema_path's directory -- the same registry/RefResolver approach as
    mock_ivi_receiver.py. jsonschema is imported here, not at module level,
    so every other mode stays standard-library only."""
    try:
        import jsonschema
    except ImportError:
        die("--r4-schema requires the 'jsonschema' package, which is not "
            "installed; install it with: pip install jsonschema", 2)
    try:
        schema = json.loads(schema_path.read_text(encoding="utf-8"))
    except (OSError, ValueError) as e:
        die(f"cannot load schema {schema_path}: {e}", 2)
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
        # Older jsonschema: RefResolver rooted at the schema's directory.
        resolver = jsonschema.RefResolver(
            base_uri=contracts_dir.as_uri() + "/", referrer=schema)
        return jsonschema.Draft202012Validator(schema, resolver=resolver)


def run_r4_schema(chk: Checker, events: list, schema_path: Path) -> None:
    validator = build_r4_validator(schema_path)
    failures: list = []
    checked = 0
    for ev in events:
        if ev.name != "r4_tx":
            continue
        body = r4_body(ev)
        if body is None:
            failures.append(f"line {ev.lineno}: r4_tx payload.body is not a "
                            f"JSON object - nothing to validate")
            continue
        checked += 1
        for error in validator.iter_errors(body):
            loc = "/".join(str(p) for p in error.absolute_path) or "$"
            failures.append(f"line {ev.lineno}: r4_tx body invalid at {loc}: "
                            f"{error.message}")
    chk.group("r4-schema", failures,
              f"{checked} r4_tx body(ies) valid against {schema_path.name}")


def positive_int(raw: str) -> int:
    value = int(raw)
    if value < 1:
        raise argparse.ArgumentTypeError(f"must be >= 1, got {value}")
    return value


def positive_float(raw: str) -> float:
    value = float(raw)
    if value <= 0:
        raise argparse.ArgumentTypeError(f"must be > 0, got {value}")
    return value


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog="check_evt_log.py",
        description="ADA [EVT]-stream assertion (R18); see the module "
                    "docstring for the full contract.")
    parser.add_argument("logfile", nargs="?", default=None,
                        help="log to read; omitted or '-' reads stdin")
    modes = parser.add_mutually_exclusive_group(required=False)
    modes.add_argument("--admission", action="store_true",
                       help="assert legal R13 admission paths and full cycles")
    modes.add_argument("--expect-no-tracks", action="store_true",
                       help="assert zero track_transition events")
    parser.add_argument("--fusion", action="store_true",
                        help="assert the Phase 4 relay chain and the "
                             "edge-triggered risk/emit pairing (D5)")
    parser.add_argument("--both-tracks", action="store_true",
                        help="assert both a tracked own_sensor and a tracked "
                             "v2x_relayed R3 object plus a full relayed r4_tx")
    parser.add_argument("--r4-schema", type=Path, default=None, metavar="PATH",
                        help="validate every r4_tx embedded body against the "
                             "schema at PATH (requires the jsonschema package)")
    parser.add_argument("--confirm-hits", type=positive_int,
                        default=DEFAULT_CONFIRM_HITS,
                        help=f"CONFIRM_HITS the node ran with "
                             f"(default {DEFAULT_CONFIRM_HITS})")
    parser.add_argument("--gate-enter-m", type=positive_float,
                        default=DEFAULT_GATE_ENTER_M,
                        help=f"GATE_ENTER_M the node ran with "
                             f"(default {DEFAULT_GATE_ENTER_M})")
    parser.add_argument("--gate-exit-m", type=positive_float,
                        default=DEFAULT_GATE_EXIT_M,
                        help=f"GATE_EXIT_M the node ran with "
                             f"(default {DEFAULT_GATE_EXIT_M})")
    parser.add_argument("--min-transitions", type=positive_int,
                        default=DEFAULT_MIN_TRANSITIONS,
                        help=f"minimum track_transition count for --admission "
                             f"(default {DEFAULT_MIN_TRANSITIONS})")
    return parser


def select_checks(args: argparse.Namespace) -> list:
    """[(name, handler)] for every selected check, run in registry order.
    Additive by design: a later phase registers its flag in build_parser()
    and its row here; nothing else changes. Every handler takes
    (chk, events, cfg)."""
    def r4_schema_handler(chk: Checker, events: list,
                          _cfg: AdmissionConfig) -> None:
        run_r4_schema(chk, events, args.r4_schema)

    registry: tuple = (
        ("admission", args.admission, run_admission),
        ("expect-no-tracks", args.expect_no_tracks, run_expect_no_tracks),
        ("fusion", args.fusion, run_fusion),
        ("both-tracks", args.both_tracks, run_both_tracks),
        ("r4-schema", args.r4_schema is not None, r4_schema_handler),
    )
    return [(name, handler) for name, selected, handler in registry if selected]


def main(argv: list) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)
    if args.gate_enter_m >= args.gate_exit_m:
        parser.error(f"--gate-enter-m {args.gate_enter_m} must be < "
                     f"--gate-exit-m {args.gate_exit_m} (the hysteresis band)")
    cfg = AdmissionConfig(confirm_hits=args.confirm_hits,
                          gate_enter_m=args.gate_enter_m,
                          gate_exit_m=args.gate_exit_m,
                          min_transitions=args.min_transitions)
    checks = select_checks(args)
    if not checks:
        parser.error("at least one check flag is required: --admission, "
                     "--expect-no-tracks, --fusion, --both-tracks or "
                     "--r4-schema")
    mode = "+".join(name for name, _ in checks)

    source, lines = read_lines(args.logfile)
    log(f"mode={mode} input={source} lines={len(lines)}"
        + (f" confirm_hits={cfg.confirm_hits} gate_enter_m={cfg.gate_enter_m}"
           f" gate_exit_m={cfg.gate_exit_m} min_transitions={cfg.min_transitions}"
           if args.admission else ""))

    events, malformed = extract_events(lines)
    chk = Checker()
    parse_failures = list(malformed)
    if not events:
        parse_failures.append(f"no [EVT] lines found in {source} - an empty "
                              f"input is never a pass")
    chk.group("parse", parse_failures,
              f"{len(events)} [EVT] line(s) parsed from {len(lines)} input line(s), "
              f"0 malformed")

    check_fields(chk, events)
    check_vocabulary(chk, events)
    check_counters(chk, events)
    for _, handler in checks:
        handler(chk, events, cfg)

    if chk.failures:
        log(f"FAIL: {chk.failures[0]}")
        if len(chk.failures) > 1:
            log(f"FAIL: {len(chk.failures)} assertion failure(s) total")
        return 1
    transitions = sum(1 for ev in events if ev.name == "track_transition")
    log(f"PASS mode={mode} evt_lines={len(events)} track_transitions={transitions}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
