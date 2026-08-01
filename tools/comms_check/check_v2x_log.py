#!/usr/bin/env python3
"""``[EVT]``-stream assertion -- the receive-evidence half of the D7 comms check.

Asserts the D7 chain on a V2X ECU event stream: per message ``rx_datagram``
(datagram received) -> ``decode_ok`` carrying the decoded ``CpmContent`` as JSON
(event raised, deserialization shown) -> ``r2_forwarded`` carrying the R2 JSON
body. Exits non-zero naming the first missing link.

Test equipment only (V2X ECU comms HLD, decision D7). It is never deployed: it
reads a log the node already produced -- CI-captured stdout, or a saved CarSky
View Log export (the smoke-test View-Log-as-retrieval model). Any non-``[EVT]``
line is ignored, so ``[CAP]`` capture text, ``[BOOT]``/``[SEND]`` lines, tcpdump
output and blank lines may be interleaved freely.

Usage::

    check_v2x_log.py [logfile] [--expect-vectors DIR] [--repeat N]
    check_v2x_log.py [logfile] [--min-messages N]

Configuration -- CLI wins over env:

=======================  =================  ==========================================
CLI                      Env                Meaning / default
=======================  =================  ==========================================
``logfile`` (pos. 1)     --                 log to read; omitted or ``-`` reads stdin
``--expect-vectors DIR`` --                 expected-vector mode over that golden
                                            corpus (``*.json`` cases); no default --
                                            omitting it selects stream mode
``--repeat N``           --                 corpus repeats in the run, default 1;
                                            expected count = cases x N. Mirrors
                                            ``send_cpm.py --repeat``
``--min-messages N``     ``MIN_MESSAGES``   stream mode: minimum complete chains
                                            required, default 1
=======================  =================  ==========================================

The two modes are mutually exclusive (giving both is a bad invocation); no env
var selects a mode.

**Expected-vector mode** (CI, corpus known): exactly one complete chain per
golden case is expected (x ``--repeat``). Every ``decode_ok`` payload must equal
one golden ``.json`` content, every golden case must be matched at least once,
each matched ``decode_ok`` must be followed later in the stream by an
``r2_forwarded`` for the same ``stationId``, and the final counters must show at
least the expected number of forwards with zero decode rejects.

**Stream mode** (on-platform, corpus not known to the checker): at least
``--min-messages`` ``rx_datagram`` events, and as many ``decode_ok`` (non-empty
``cpm``) and ``r2_forwarded`` (non-empty ``r2``) events -- i.e. every received
message completed the chain -- with the chain order verified per triple.

Chain-pairing heuristic: the i-th occurrence of each stage event is paired with
the i-th occurrence of the others, and the stream position must increase
``rx_datagram`` -> ``decode_ok`` -> ``r2_forwarded``. The frozen ``counters``
make that pairing exact rather than heuristic when nothing was dropped: each
stage event increments its own counter by 1 as it is emitted, so ``decode_ok``
#k carries ``counters.decode_ok == k``; the checker verifies that continuity
(delta 1 between consecutive occurrences, which also detects a lost log line
without assuming the stream starts at 1) and, when the final counters show no
reject/drop at all, additionally requires the paired triples to carry equal
counter values.

Frozen ``[EVT]`` fields this script depends on (``V2X_ECU/src/log/event_log.hpp``
-- renaming any of them breaks this consumer): line prefix ``[EVT] `` followed by
one JSON object with ``event``, ``mono_ms``, ``epoch_ms`` and ``counters`` (keys
``rx_datagram``, ``decode_ok``, ``decode_reject``, ``validate_reject``,
``dedupe_drop``, ``r2_forwarded``); ``cpm`` on ``decode_ok``; ``r2`` on
``r2_forwarded``. The 9-name ``event`` vocabulary is frozen too, so an unknown
name is a failure, not a tolerated extra. ``detail`` and ``bytes`` are free
diagnostics and are not parsed.

Output: one ``[CHK] <group>: PASS|FAIL`` line per assertion group, then a final
``[CHK] PASS ...`` or ``[CHK] FAIL: <first missing link>``. Exit 0 when every
group passes, 1 on any assertion failure, 2 on a bad invocation (unreadable
logfile, both modes given, unusable corpus directory, bad option/value).
"""

import json
import os
import sys
from dataclasses import dataclass, field
from pathlib import Path

EVT_PREFIX = "[EVT] "

# Frozen D4 event vocabulary (event_log.hpp) -- an unknown name is a failure.
EVENT_NAMES = frozenset({
    "rx_datagram", "decode_ok", "decode_reject", "validate_reject",
    "dedupe_drop", "r2_forwarded", "stub_transition", "fault_injected",
    "recovery",
})
# Stage events own a counter of the same name; the three stub events increment none.
STAGE_EVENTS = ("rx_datagram", "decode_ok", "decode_reject", "validate_reject",
                "dedupe_drop", "r2_forwarded")
COUNTER_KEYS = STAGE_EVENTS
MANDATORY_FIELDS = ("event", "mono_ms", "epoch_ms", "counters")

ENV_MIN_MESSAGES = os.environ.get("MIN_MESSAGES", "1")

USAGE = ("usage: check_v2x_log.py [logfile] [--expect-vectors DIR] [--repeat N]\n"
         "       check_v2x_log.py [logfile] [--min-messages N]")

MAX_REPORTED_PER_GROUP = 5     # keep a broken log readable; the count is always stated


def log(msg):
    print(f"[CHK] {msg}", flush=True)


def die(msg, code):
    print(f"[ERR] {msg}", file=sys.stderr, flush=True)
    sys.exit(code)


@dataclass
class Event:
    """One parsed ``[EVT]`` line. ``index`` orders events among themselves
    (stream position, used for the chain-order assertion); ``lineno`` points
    back into the input file for human diagnosis."""
    index: int
    lineno: int
    name: str
    counters: dict
    obj: dict

    def counter(self):
        """This event's own stage counter, or None for the stub events."""
        return self.counters.get(self.name) if self.name in STAGE_EVENTS else None


@dataclass
class Checker:
    """Collects per-group verdicts; the first failure recorded is the reported
    'first missing link'."""
    failures: list = field(default_factory=list)

    def group(self, name, failures, detail):
        if failures:
            log(f"{name}: FAIL ({len(failures)}) - {failures[0]}")
            for extra in failures[1:MAX_REPORTED_PER_GROUP]:
                log(f"{name}:   also - {extra}")
            if len(failures) > MAX_REPORTED_PER_GROUP:
                log(f"{name}:   ... {len(failures) - MAX_REPORTED_PER_GROUP} more")
            self.failures.extend(failures)
        else:
            log(f"{name}: PASS - {detail}")


def parse_args(argv):
    logfile, corpus, repeat, minimum = None, None, "1", None
    positional = []
    i = 0
    while i < len(argv):
        arg = argv[i]
        if arg in ("-h", "--help"):
            print(__doc__)
            sys.exit(0)
        if arg in ("--expect-vectors", "--repeat", "--min-messages"):
            if i + 1 >= len(argv):
                print(USAGE, file=sys.stderr, flush=True)
                die(f"{arg} needs a value", 2)
            value = argv[i + 1]
            if arg == "--expect-vectors":
                corpus = value
            elif arg == "--repeat":
                repeat = value
            else:
                minimum = value
            i += 2
            continue
        if arg.startswith("-") and arg != "-":      # bare "-" is the stdin positional
            print(USAGE, file=sys.stderr, flush=True)
            die(f"unknown option {arg}", 2)
        positional.append(arg)
        i += 1

    if len(positional) > 1:
        print(USAGE, file=sys.stderr, flush=True)
        die(f"expected at most 1 positional arg (logfile), got {len(positional)}", 2)
    if positional:
        logfile = positional[0]
    if corpus and minimum is not None:
        print(USAGE, file=sys.stderr, flush=True)
        die("--expect-vectors and --min-messages are mutually exclusive modes", 2)

    def positive_int(name, raw):
        try:
            value = int(raw)
        except ValueError:
            print(USAGE, file=sys.stderr, flush=True)
            die(f"{name} must be an integer, got {raw!r}", 2)
        if value < 1:
            print(USAGE, file=sys.stderr, flush=True)
            die(f"{name} must be >= 1, got {value}", 2)
        return value

    repeat_n = positive_int("--repeat", repeat)
    min_n = positive_int("--min-messages/MIN_MESSAGES",
                         minimum if minimum is not None else ENV_MIN_MESSAGES)
    corpus_dir = Path(corpus).expanduser() if corpus else None
    return logfile, corpus_dir, repeat_n, min_n


def read_lines(logfile):
    if logfile in (None, "-"):
        return "<stdin>", sys.stdin.read().splitlines()
    path = Path(logfile).expanduser()
    try:
        text = path.read_text(encoding="utf-8", errors="replace")
    except OSError as e:
        die(f"cannot read logfile {path}: {e}", 2)
    return str(path), text.splitlines()


def load_corpus(corpus_dir):
    if not corpus_dir.is_dir():
        die(f"corpus directory not found: {corpus_dir}", 2)
    cases = {}
    for path in sorted(corpus_dir.glob("*.json")):     # lexicographic = send_cpm.py order
        try:
            cases[path.stem] = json.loads(path.read_text(encoding="utf-8"))
        except (OSError, ValueError) as e:
            die(f"cannot read golden case {path}: {e}", 2)
    if not cases:
        die(f"no .json golden cases in corpus directory: {corpus_dir}", 2)
    return cases


def extract_events(lines):
    """Pull the `[EVT] {json}` lines out of an arbitrarily noisy log. Returns
    (events, malformed) -- a line that claims to be `[EVT]` but does not carry a
    JSON object is a failure, not noise."""
    events, malformed = [], []
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
            malformed.append(f"line {lineno}: [EVT] payload is {type(obj).__name__}, not a JSON object")
            continue
        name = obj.get("event")
        counters = obj.get("counters")
        events.append(Event(index=len(events), lineno=lineno,
                            name=name if isinstance(name, str) else "",
                            counters=counters if isinstance(counters, dict) else {},
                            obj=obj))
    return events, malformed


def check_fields(chk, events):
    """Mandatory frozen fields + a well-formed counters object on every event."""
    failures = []
    for ev in events:
        missing = [f for f in MANDATORY_FIELDS if f not in ev.obj]
        if missing:
            failures.append(f"line {ev.lineno}: [EVT] missing mandatory field(s) {', '.join(missing)}")
            continue
        if not isinstance(ev.obj["counters"], dict):
            failures.append(f"line {ev.lineno}: 'counters' is not a JSON object")
            continue
        absent = [k for k in COUNTER_KEYS if k not in ev.counters]
        if absent:
            failures.append(f"line {ev.lineno}: 'counters' missing key(s) {', '.join(absent)}")
            continue
        bad = [k for k in COUNTER_KEYS if not isinstance(ev.counters[k], int)
               or isinstance(ev.counters[k], bool)]
        if bad:
            failures.append(f"line {ev.lineno}: non-integer counter(s) {', '.join(bad)}")
    chk.group("fields", failures,
              f"all {len(events)} event(s) carry {'/'.join(MANDATORY_FIELDS)} "
              f"with the {len(COUNTER_KEYS)} frozen counter keys")


def check_vocabulary(chk, events):
    failures = [f"line {ev.lineno}: unknown event name {ev.obj.get('event')!r} "
                f"(frozen vocabulary: {', '.join(sorted(EVENT_NAMES))})"
                for ev in events if ev.name not in EVENT_NAMES]
    chk.group("vocabulary", failures,
              f"all event names within the frozen {len(EVENT_NAMES)}-name vocabulary")


def check_counters(chk, events):
    """Monotonicity across the stream, plus per-stage continuity: a stage event
    bumps its own counter by exactly 1, so a delta > 1 means log lines were lost."""
    failures = []
    previous = None
    for ev in events:
        if previous is not None:
            for key in COUNTER_KEYS:
                now, before = ev.counters.get(key), previous.counters.get(key)
                if isinstance(now, int) and isinstance(before, int) and now < before:
                    failures.append(f"line {ev.lineno}: counters.{key} went backwards "
                                    f"{before} -> {now} (non-monotonic stream)")
        previous = ev
    for stage in STAGE_EVENTS:
        seen = [ev for ev in events if ev.name == stage]
        for prev_ev, ev in zip(seen, seen[1:]):
            before, now = prev_ev.counter(), ev.counter()
            if not isinstance(before, int) or not isinstance(now, int):
                continue
            if now != before + 1:
                failures.append(f"line {ev.lineno}: counters.{stage} jumped {before} -> {now} "
                                f"- {now - before - 1} {stage} line(s) missing from the stream")
    chk.group("counters", failures,
              "counters monotonically non-decreasing; every stage event increments "
              "its own counter by exactly 1 (no lost lines)")


def payload(ev, key):
    """The event's embedded contract payload, or None when absent/empty/not an object."""
    value = ev.obj.get(key)
    return value if isinstance(value, dict) and value else None


def check_expected_vectors(chk, events, cases, repeat):
    expected = len(cases) * repeat
    rx = [ev for ev in events if ev.name == "rx_datagram"]
    decodes = [ev for ev in events if ev.name == "decode_ok"]
    forwards = [ev for ev in events if ev.name == "r2_forwarded"]

    chk.group("rx", [] if len(rx) >= expected else
              [f"rx_datagram events: {len(rx)} < {expected} expected "
               f"({len(cases)} golden case(s) x repeat {repeat}) - "
               f"{expected - len(rx)} datagram(s) never received"],
              f"{len(rx)} rx_datagram event(s) >= {expected} expected")

    # Set-match: each decode_ok payload must equal one golden case content, and
    # every golden case must show up at least once. Order-insensitive by design.
    failures, matched, matches = [], {name: 0 for name in cases}, {}
    for ev in decodes:
        cpm = payload(ev, "cpm")
        if cpm is None:
            failures.append(f"line {ev.lineno}: decode_ok #{ev.counter()} carries no "
                            f"decoded CpmContent ('cpm' absent or empty)")
            continue
        hit = next((name for name, content in cases.items() if content == cpm), None)
        if hit is None:
            failures.append(f"line {ev.lineno}: decode_ok #{ev.counter()} cpm matches no golden "
                            f"case (stationId={cpm.get('stationId')!r}, "
                            f"objectId={(cpm.get('object') or {}).get('objectId')!r})")
            continue
        matched[hit] += 1
        matches[ev.index] = hit
    for name, count in matched.items():
        if count == 0:
            failures.append(f"golden case '{name}' never appeared in a decode_ok cpm "
                            f"- decode_ok link missing for it")
    chk.group("decode-content", failures,
              f"{len(matches)} decode_ok payload(s) matched golden cases: "
              + ", ".join(f"{name}x{count}" for name, count in matched.items()))

    # Forward link: each matched decode_ok needs a later r2_forwarded for the same
    # stationId. Greedy first-unconsumed pairing keeps 1 decode_ok : 1 forward.
    failures, consumed = [], set()
    for ev in forwards:
        if payload(ev, "r2") is None:
            failures.append(f"line {ev.lineno}: r2_forwarded #{ev.counter()} carries no R2 JSON "
                            f"body ('r2' absent or empty)")
    for ev in decodes:
        if ev.index not in matches:
            continue
        station = (payload(ev, "cpm") or {}).get("stationId")
        hit = next((f for f in forwards
                    if f.index > ev.index and f.index not in consumed
                    and payload(f, "r2") is not None
                    and payload(f, "r2").get("stationId") == station), None)
        if hit is None:
            failures.append(f"line {ev.lineno}: no r2_forwarded after decode_ok #{ev.counter()} "
                            f"(case '{matches[ev.index]}', stationId={station!r}) "
                            f"- forward link missing")
            continue
        consumed.add(hit.index)
    chk.group("forward", failures,
              f"{len(consumed)} decode_ok event(s) each followed by an r2_forwarded "
              f"carrying the same stationId")

    failures = []
    if events:
        final = events[-1].counters
        forwarded = final.get("r2_forwarded")
        rejected = final.get("decode_reject")
        if not isinstance(forwarded, int) or forwarded < expected:
            failures.append(f"final counters.r2_forwarded={forwarded} < {expected} expected "
                            f"- the R2 forward link did not complete for every case")
        if isinstance(rejected, int) and rejected != 0:
            failures.append(f"final counters.decode_reject={rejected} (expected 0) "
                            f"- golden vectors were rejected by the decoder")
        detail = (f"final counters r2_forwarded={forwarded} >= {expected}, decode_reject=0 "
                  f"(validate_reject={final.get('validate_reject')}, "
                  f"dedupe_drop={final.get('dedupe_drop')})")
    else:
        detail = "no events"
    chk.group("counters-final", failures, detail)


def check_stream(chk, events, minimum):
    rx = [ev for ev in events if ev.name == "rx_datagram"]
    decodes = [ev for ev in events if ev.name == "decode_ok"]
    forwards = [ev for ev in events if ev.name == "r2_forwarded"]

    chk.group("rx", [] if len(rx) >= minimum else
              [f"rx_datagram events: {len(rx)} < {minimum} required "
               f"- fewer messages received than --min-messages"],
              f"{len(rx)} rx_datagram event(s) >= {minimum} required")

    def with_payload(stage, evs, key):
        failures = [f"line {ev.lineno}: {stage} #{ev.counter()} carries no {key} payload "
                    f"('{key}' absent or empty)" for ev in evs if payload(ev, key) is None]
        good = [ev for ev in evs if payload(ev, key) is not None]
        if len(good) < minimum:
            failures.insert(0, f"{stage} events carrying {key}: {len(good)} < {minimum} required "
                               f"- {minimum - len(good)} received message(s) did not complete the "
                               f"chain at the {stage} link")
        return failures, good

    failures, good_decodes = with_payload("decode_ok", decodes, "cpm")
    chk.group("decode", failures,
              f"{len(good_decodes)} decode_ok event(s) with a decoded CpmContent >= {minimum} required")

    failures, good_forwards = with_payload("r2_forwarded", forwards, "r2")
    chk.group("forward", failures,
              f"{len(good_forwards)} r2_forwarded event(s) with an R2 body >= {minimum} required")

    # Chain order over the first `minimum` triples, paired i-th to i-th (see the
    # module docstring's pairing heuristic). Exact counter pairing is additionally
    # demanded when the stream shows no reject/drop, where the three stage
    # counters must advance in lockstep.
    failures = []
    triples = min(minimum, len(rx), len(good_decodes), len(good_forwards))
    lockstep = bool(events) and all(events[-1].counters.get(k) == 0
                                   for k in ("decode_reject", "validate_reject", "dedupe_drop"))
    for i in range(triples):
        a, b, c = rx[i], good_decodes[i], good_forwards[i]
        if not (a.index < b.index < c.index):
            failures.append(f"chain #{i + 1} out of order: rx_datagram (line {a.lineno}), "
                            f"decode_ok (line {b.lineno}), r2_forwarded (line {c.lineno}) "
                            f"- expected rx_datagram -> decode_ok -> r2_forwarded")
            continue
        if lockstep and not (a.counter() == b.counter() == c.counter()):
            failures.append(f"chain #{i + 1} counters do not pair: rx_datagram #{a.counter()}, "
                            f"decode_ok #{b.counter()}, r2_forwarded #{c.counter()} "
                            f"with no reject/drop in the stream")
    chk.group("order", failures,
              f"{triples} chain(s) ordered rx_datagram -> decode_ok -> r2_forwarded"
              + (" with counters paired in lockstep" if lockstep else
                 " (reject/drop present: counter lockstep not asserted)"))


def main(argv):
    logfile, corpus_dir, repeat, minimum = parse_args(argv)
    source, lines = read_lines(logfile)
    cases = load_corpus(corpus_dir) if corpus_dir else None
    mode = "expected-vector" if cases else "stream"
    log(f"mode={mode} input={source} lines={len(lines)}"
        + (f" corpus={corpus_dir} cases={len(cases)} repeat={repeat}" if cases
           else f" min_messages={minimum}"))

    events, malformed = extract_events(lines)
    chk = Checker()
    parse_failures = list(malformed)
    if not events:
        parse_failures.append(f"no [EVT] lines found in {source} - no receive evidence at all")
    chk.group("parse", parse_failures,
              f"{len(events)} [EVT] line(s) parsed from {len(lines)} input line(s), 0 malformed")

    check_fields(chk, events)
    check_vocabulary(chk, events)
    check_counters(chk, events)
    if cases:
        check_expected_vectors(chk, events, cases, repeat)
    else:
        check_stream(chk, events, minimum)

    if chk.failures:
        log(f"FAIL: {chk.failures[0]}")
        if len(chk.failures) > 1:
            log(f"FAIL: {len(chk.failures)} assertion failure(s) total")
        return 1
    counted = {stage: sum(1 for ev in events if ev.name == stage) for stage in STAGE_EVENTS}
    log(f"PASS mode={mode} evt_lines={len(events)} "
        + " ".join(f"{stage}={count}" for stage, count in counted.items())
        + " - receive -> decode_ok(CpmContent JSON) -> r2_forwarded(R2 JSON) chain complete")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
