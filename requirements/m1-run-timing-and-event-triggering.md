# Run Timing, Startup Flow, and Demo Event Triggering Across the Four M1 Nodes

Requirement analysis & technical solution report per [research-report-format.md](../.claude/rules/research-report-format.md). Answers the three questions in [2026-08-02-time-sync-and-demo-event-triggering.md](../.claude/prompts/2026-08-02-time-sync-and-demo-event-triggering.md): is a startup call flow / time-sync module needed, is a demo trigger application needed, and how.

It defines **two new requirement numbers, R20 and R21** (§7), continuing the project-global numbering after R19. R1–R19 in [m1-cooperative-awareness.md](m1-cooperative-awareness.md) are untouched, and **no frozen contract changes** (§6.5).

Diagrams: [m1-run-timing-startup-flow.puml](m1-run-timing-startup-flow.puml) (startup + triggered run) · [m1-run-timing-alignment.puml](m1-run-timing-alignment.puml) (one bench cycle on the scenario timeline).

## 1. The three answers, up front

| # | Question | Answer |
|---|---|---|
| 1 | Startup call flow + time-sync module between ECUs | **No clock synchronisation, and no node-to-node handshake.** The measured offset between the two container nodes is already **under 55 ms** (§2), no node performs arithmetic on another node's timestamp, and the AAOS guest reads no timestamp at all. Readiness is a separate question with a separate answer: it is already covered by R5's Deployment-Viewer check plus a configured bench `start_delay_s` — building a ping-everyone barrier would cost an ego-software reverse channel and buy nothing R5 does not already give. |
| 2 | Demo/test trigger application issuing "read video at A, send CPM at B" | **No orchestrator process, no trigger message.** Each stimulus source self-schedules from its own config against its own process start; the operator's *restart the bench node* action is the GO. The only tool worth writing is a **post-run verification script**, not a trigger. |
| 3 | How | Pace both stimulus sources to real time on `CLOCK_MONOTONIC` deadlines, put every offset in config (§6.1), fix which clock stamps which R3 field (§6.2), and verify with `ADA_ECU/tools/check_run_alignment.py` against ADA's own clock alone (§6.4). Total new code: roughly one day, most of it inside Phase 3 work that has to happen anyway. |

## 2. What contradicts the premise — read this first

The problem statement assumes clock divergence between ECUs. Four findings say the problem is elsewhere.

**(a) The container-node clocks already agree to within 55 ms — measured, not assumed.** The Phase 1 run record ([phase1-comms-run.md](../plans/doc/phase1-comms-run.md)) contains a usable cross-node measurement that nobody took it for. The V2X ECU stamps `rxTime` from its own `system_clock`; the ADA-side sink prints its own `time.strftime('%H:%M:%S')` ([netcheck.py:13](../tools/netcheck/netcheck.py)) on receipt. Five consecutive messages:

| Msg | `rxTime` (V2X clock) | as UTC | ADA sink printed |
|---|---|---|---|
| #5899 | 1785591216646 | 13:33:36.646 | 13:33:36 |
| #5900 | 1785591216745 | 13:33:36.745 | 13:33:36 |
| #5901 | 1785591216846 | 13:33:36.846 | 13:33:36 |
| #5902 | 1785591216946 | 13:33:36.946 | **13:33:36** |
| #5903 | 1785591217046 | 13:33:37.046 | **13:33:37** |

Let δ = (ADA print instant, ADA clock) − (`rxTime` instant, V2X clock) = transit + handling + clock offset. #5902 printing in second 36 requires `0.946 + δ < 1.000` → **δ < 54 ms**. #5903 printing in second 37 requires `0.046 + δ ≥ 0` → **δ > −46 ms**. Transit and handling are non-negative, so the clock offset is under 54 ms in one direction and, with per-datagram handling under 10 ms, under about 56 ms in the other. **|offset| < ~55 ms, and the whole V2X→ADA hop including transit is inside the same bound.**

Caveat, stated rather than glossed: this bounds two *container* nodes in one deployment. CarSky documents no co-scheduling — a Room is "a Kubernetes namespace where the Nydus Operator builds pods/services for every node", the Device only fixes "namespace, resource pool", and there is no `nodeSelector`/affinity statement anywhere ([Car-Sky-Platform.html](development-platform-doc/Car-Sky-Platform.html)). BTC's advisory even contemplates the ADA ECU as an external node on a separate server. So this is a per-deployment property to re-check, not a guarantee — which is exactly why the recommended design does not depend on it.

**(b) Nothing in M1 compares one node's clock against another's.** Every consumer of a foreign timestamp uses it as an opaque record value, not as an operand:

| Foreign timestamp | Consumer | Used for arithmetic against a local clock? |
|---|---|---|
| CPM `referenceTime` (bench clock) | V2X `r2_builder` F1 speed derivation | No — only `refTime[n] − refTime[n−1]`, a difference inside one clock domain |
| CPM `referenceTime` | anything downstream of R2 | Not forwarded — it is dropped at the R2 boundary |
| R2 `rxTime` (V2X clock) | ADA store | Record value only, once §6.2's ruling is applied |
| R3 `timestamps.*` | IVI | Parsed into `R3Timestamps` and **never read** — no use outside a preview stub and a round-trip test |
| R4 | IVI | **R4 carries no timestamp field at all**; `WARNING_TIMEOUT_MS` is a local countdown |
| V2X dedupe window | V2X | Deliberately `steady_clock` — "wall-clock jumps must neither open nor close the window" |

**(c) The AAOS/Skycraft node is out of the question entirely.** It is "a full guest VM" booting its own kernel, therefore its own clock domain, and CarSky documents no time sync into it (the ADB `shell` route returns 502 on this deployment, so it could not be set even if wanted). It does not matter: R4 has no time field, so the guest never compares clocks. **The one node where clock sync would be hardest is the one node that provably does not need it.**

**(d) The real defect is pacing, not clocks.** Two independent stimulus sources drive the demo, and one of them has no time base at all:

- **Bench** — sleeps a fixed `period` per tick with no drift correction, and derives scenario time as `t = cycle_tick * period` ([generator.py](../Scenario_Player/player/generator.py)), a tick counter rather than a clock. Scenario time therefore drifts from wall time by the per-tick work cost, unbounded over a run.
- **ADA detector** — designed and unbuilt, with **no pacing of any kind**: `DETECTOR_FRAME_STRIDE=4` is a decimation stride, and "5 Hz effective" is an assumed CPU throughput, not an enforced rate ([phase2-4-ada-ecu-hld.md](../ADA_ECU/doc/phase2-4-ada-ecu-hld.md)). A 60 s clip is consumed in whatever wall time the CPU takes, then looped. This is the dominant error term by two orders of magnitude, and it is a `time.sleep` away from being fixed.

## 3. The timing model

Three distinct quantities. Two already have repo names; the third is described rather than coined ([markdown-writing-style](../.claude/skills/markdown-writing-style/SKILL.md) rule 5).

| Quantity | Existing name | Where it lives |
|---|---|---|
| The timeline the demo is authored against | **scenario time** `t` — already the bench's term, emitted as `scenario_time_s` in its `[TX]` JSONL | Bench only. Nothing else in the system has one. |
| Per-container wall clock | `CLOCK_REALTIME` / `system_clock` / `time.time()` | V2X `rxTime`, R3 `timestamps.*`, `[EVT] epoch_ms` |
| Per-container interval clock | `CLOCK_MONOTONIC` / `steady_clock` / `time.monotonic()` | V2X dedupe window, `[EVT] mono_ms` |
| Bench-emit → ADA-store delay | described as **pipeline latency**; no term coined | §3.1 |

### 3.1 Pipeline latency budget

| Segment | Value | Source |
|---|---|---|
| Bench tick → CPM bytes (persistent `cpm_encode --stream` subprocess, not a fork per message) | unmeasured; pipe round-trip to a resident process | [SP D1](../Scenario_Player/doc/scenario-player-design-decisions.md) |
| Bench → V2X, 58 B UDP over R6 | unmeasured individually | — |
| V2X decode → validate → dedupe → build → `sendto` | **142–151 µs, measured, stable** | [phase1-comms-run.md](../plans/doc/phase1-comms-run.md) |
| V2X → ADA, ~339 B UDP | **inside the < 55 ms bound of §2(a)**, together with the clock offset | derived |
| ADA R2 ingest → store → R14 → R4 build | unmeasured; in-process C++ | — |
| ADA → IVI, UDP into the AAOS guest | unmeasured — the only hop crossing into a VM | — |
| IVI parse → Compose recomposition | unmeasured; one frame ≈ 16–33 ms | — |

Against the stimulus periods: **CPM period 100 ms** (`cpm_rate_hz` 10), **detector sample period ≤ 200 ms** (R12's ≥ 5 Hz floor), **video frame period 50 ms** at 20 fps.

### 3.2 The user's numeric example, worked

*"The video shows car B at 1:1001; the V2X message arrives at 1:1002; therefore the bench should have sent way before."*

Let `T` be the ADA wall instant at which the frame showing B is processed. For the store to hold matching C data at `T`, the bench must have emitted it at `T − L`, where `L` is pipeline latency.

- `L(bench → ADA store)` is bounded by §2(a) at **< 55 ms** end to end. So "way before" is **0.55 of one CPM period** — not seconds.
- Because the bench emits a *continuous* 10 Hz stream rather than a one-shot, that lead is supplied automatically: at any instant the store's newest C entry is at most `100 ms + 55 ms = 155 ms` old.
- The store's newest B entry is at most **200 ms** old (5 Hz detector).
- Therefore **C's data is structurally never staler than B's**, and the failure the question fears — ADA reading the video before the corresponding V2X information exists — cannot occur while both streams are running.

### 3.3 The constraint that *is* real

Not "the CPM must arrive before the frame" but **an ordering constraint on track admission**: Phase 4's output acceptance requires a `tracked` `own_sensor` B *and* a `tracked` `v2x_relayed` C in the store at the R4 emission. So:

```
t(B reaches tracked)  <  t(C crosses gate_enter)
```

Budget from the committed scenario ([default.yaml](../Scenario_Player/scenarios/default.yaml): `cpm_rate_hz` 10, `duration_s` 20, `loop` true; 60 m start, 2.5 m/s closing; R13 `gate_enter` 30 m):

| Event | Bench scenario time |
|---|---|
| Cycle start, C at 60 m | 0.0 s |
| C crosses `gate_exit` level, 35 m | 10.0 s |
| C crosses `gate_enter`, 30 m → admission | **12.0 s** |
| Cycle end, C at 10 m → loop back to 60 m | 20.0 s |

Required on the ADA side: detector warm-up (YOLO11n ONNX load + `VideoCapture` open, estimated 2–5 s, unmeasured) plus `confirm_hits` at 5 Hz (3 detections = 0.6 s) ≈ **5.6 s**. Against a 12.0 s lead-in, that is **≈ 6.4 s of slack** — the alignment tolerance the whole design rests on.

And `loop: true` supplies a second, stronger safety net: **a fresh admission event occurs every 20 s**, so any start misalignment self-corrects within one cycle. This is why an unbounded start offset between bench and ADA does not break the demo.

### 3.4 Error budget — where correlation error actually comes from

| Term | Magnitude today | Magnitude after §6 |
|---|---|---|
| Detector rate error (free-running, no pacing) | **unbounded and growing** — a 60 s clip may be consumed in ~10 s or ~120 s | ≤ 2 % of elapsed |
| Bench scenario-clock drift (`sleep(period)`, no deadline) | ~1 % accumulating (≈ 0.6 s over 60 s) | ≤ 1 % of elapsed |
| Start offset between the two sources | unbounded, but absorbed by the 6.4 s slack and the 20 s loop | same, plus explicit `start_delay_s` |
| Sampling granularity | `max(100 ms, 200 ms)` = 200 ms | unchanged — a floor, not a defect |
| Pipeline latency | < 55 ms | unchanged |
| **Cross-node clock offset** | **< 55 ms, and multiplied by zero — no consumer performs cross-node time arithmetic** | unchanged |

The last row is the finding: clock offset is both the smallest term and the only one with no consumer.

## 4. Question 1 — is a startup call flow or time-sync module needed?

Readiness and clock sync are separable and are treated separately, as asked.

### 4.1 Clock synchronisation — candidate comparison

Hard constraints ([solution-selection-criteria.md](../.claude/rules/solution-selection-criteria.md)) pass for all five (chrony, ntpd, linuxptp are open-source and Linux). Ranked criteria: **C1** accomplishability · **C2** fastest for M1 · **C3** future features · **C4** smaller dependency.

| # | Candidate | Cost by 2026-08-08 | Buys for R19 | Risk | Verdict |
|---|---|---|---|---|---|
| S-1 | **No sync — each node stamps from its own `CLOCK_REALTIME`/`CLOCK_MONOTONIC`** | **zero** | Nothing missing: §2(b) shows no consumer needs it | Cross-host placement could widen the offset — mitigated because no consumer reads it | **Selected** |
| S-2 | Demo-epoch broadcast — one node announces `t0`, others express time as offset-from-epoch | ~0.5 day: one datagram shape, one listener per node | Tighter start alignment than the 6.4 s slack already provides | New listener inside ego software; a demo-control surface in ADA | Runner-up — the *scheduling* half is kept in §5, the *time* half is dropped |
| S-3 | Request/response offset exchange (Cristian / SNTP-lite), ADA as server | ~1 day both sides | Measures an offset proven < 55 ms and used by nobody | Cannot **apply** the offset without either stepping the clock (S-4's problem) or making every consumer do offset arithmetic — which means touching R2/R3 semantics | Rejected on C1, C2 |
| S-4 | chrony / ntpd / linuxptp on the Room network | ~1–2 days plus unverified platform work | Nothing S-1 lacks | **Structurally wrong**: a container has no clock of its own to discipline — `settimeofday` reaches the *host* clock shared with every other pod. Needs `CAP_SYS_TIME` (grantable via the node's `capabilities`, unverified) and would need a time server nobody runs. On the AAOS guest it needs ADB, which returns 502 | Rejected on C1 |
| S-5 | No clock sync; sequence numbers + a start barrier | small | Ordering guarantees the demo actually needs | Its barrier half is §4.2's question | Merged into the pick — §5's design is this, minus the barrier |

**Pick: S-1, with a clock-domain discipline rule (§6.2).** Drivers **C1** (it is the only option that works without a platform fact CarSky does not document) and **C2** (zero implementation, six days out). C3 costs nothing: if a later milestone ever needs a common time base, R2 `rxTime` is already the natural cross-node reference and S-2 remains available behind the same config keys.

**Does the AAOS/IVI node participate? No** — and not by convenience. R4 carries no timestamp field, so the guest has nothing to compare; §2(c).

**On "the ADA ECU should hold the authoritative centralized clock":** declined, for four reasons. There is nothing to be authoritative about (§2(b)). It would put demo infrastructure inside ego software, which R19 excludes by wording ("no scripted shortcuts inside ego software"). It needs an ADA→bench reverse link that does not exist in the R5/R6 topology, whose only direction is bench→V2X→ADA→IVI. And the constructive version already exists: **the one cross-node timestamp of record is R2 `rxTime`, stamped by the V2X ECU and already flowing to ADA** — no new mechanism, no new authority.

### 4.2 Startup readiness — a separate question, separately answered

Is a Phase-0-style all-nodes-ping-all-nodes handshake worth having purely as a readiness barrier?

Facts that decide it:

- Phase 0's smoke test was **not** a handshake — it is a one-way relay chain, `bench → V2X → ADA → IVI`, with no reply, no ack, and no reverse path. Building a handshake means building something that has never existed here.
- Its only readiness mechanism is an open-loop `START_DELAY_S=20`, and the documented remedy for a late receiver is literally "raise `START_DELAY_S`".
- **Hop 3 was never confirmed received** — the Skycraft node cannot run the tool and had no listener (residual O4). A handshake involving the IVI is exactly the leg that could not be built or verified.
- The platform offers no barrier: no `dependsOn`, no readiness probe, no ordering statement, and **no "deployment started" event delivered into a container**. `wait_ready` and the node-status SSE stream are client-side facilities for an external operator, not for nodes.
- R5's acceptance already *is* a readiness barrier — "the Deployment Viewer reports every node Running" — executed by a human before the recording starts.

| # | Candidate | Cost | Buys | Verdict |
|---|---|---|---|---|
| B-1 | **Operator readiness check (R5) + one `[EVT] ready` line per node + bench `start_delay_s`** | ~2 h | Enough: the bench does not stream into a dead socket, and the operator sees four ready lines | **Selected** |
| B-2 | Node-to-node handshake with acks | ~1 day | Automation of a check a human already performs once per run | Needs a bench←V2X reverse path through the R7 adapter — the seam whose `send` is R10-deferred — and an ADA←IVI path on the guest that Phase 0 could never verify | Rejected on C1, C2 |
| B-3 | Nothing at all | zero | — | Rejected: the AAOS guest boots slower than a container, so the IVI can miss early warnings |

**Pick: B-1.** Drivers **C1** and **C2**. The one asymmetric case worth naming: the IVI is the only node whose readiness cannot be observed from a container, so `start_delay_s` must exceed the measured AAOS boot-to-listener time — a number Phase 5 produces anyway, and which is why the IVI's own design already carries a 20 s `START_DELAY_S` for its simulator.

## 5. Question 2 — should there be a demo trigger application?

| # | Candidate | Where offsets live | Cost | Verdict |
|---|---|---|---|---|
| T-1 | **Static self-schedule** — each source starts its scenario timeline at its own process start plus a configured delay; operator's bench restart is the GO | bench YAML `start_delay_s`; ADA env `DETECTOR_START_DELAY_S` | **~0.5 day**; no new message, no new link, no contract change | **Selected** |
| T-2 | Shared scenario config + a single "GO" broadcast (S-2's scheduling half) | same config keys, plus one datagram shape | ~1 day; new bench→ADA link, absent from the R5/R6 topology; new listener in ego software | **Fallback** — adopt only if the measured K3 in §6.4 exceeds its bound |
| T-3 | Central orchestrator/conductor node issuing start commands | orchestrator config | ~2 days; a fifth container node, listeners in bench and ADA, blueprint change | Rejected on C1, C2, and on R19's "no scripted shortcuts inside ego software" |
| T-4 | Bench alone drives; ADA arms the clip on the first R2 | bench YAML only | ~0.5 day; tightest alignment | Rejected — it puts demo-shaped behaviour inside ego software for a tolerance the 6.4 s slack already meets. Also blocked by the video-source design: ADA reads a clip baked into its own image with no external arming path |
| T-5 | Manual/human start | operator procedure | zero | **Retained as part of T-1** — the bench restart *is* the manual start |

**Pick: T-1 + T-5.** Drivers **C1** (nothing in its path is unverified) and **C2** (two config keys and a paced loop, against six days). C3 is preserved: T-2 slots in behind the same config keys without changing them.

**The tool that should exist is a checker, not a trigger.** `ADA_ECU/tools/check_run_alignment.py` — post-run verification of §6.4's KPIs from the R18 JSONL. Placement follows the `ADA_ECU/tools/check_zero_c.py` precedent in [node-code-layout.md](../.claude/rules/node-code-layout.md): a check script lives in the node folder whose log it reads. It is **sanctioned bench test equipment, not production code** (governing principle 2) and is never on the ego data path.

**Offsets live in config, never in code** (CLAUDE.md principle 5) — §6.1.

## 6. Question 3 — how

### 6.1 Config keys

Bench — [Scenario_Player/scenarios/*.yaml](../Scenario_Player/scenarios/), validated by `player/config.py`:

| Key | Status | Default | Meaning |
|---|---|---|---|
| `cpm_rate_hz` | exists | 10.0 | tick rate; period `1/cpm_rate_hz` |
| `duration_s`, `loop` | exist | 20.0 / true | cycle length and restart |
| `start_delay_s` | **new** | 0.0 | grace from process start before the first CPM; set above the AAOS boot-to-listener time for demo runs |
| `reference_time_epoch` | **new** | `its` | epoch used for CPM `referenceTime`; see §6.5(b). Never a literal in the loop |

ADA — env, per the HLD's env table:

| Key | Status | Default | Meaning |
|---|---|---|---|
| `DETECTOR_FRAME_STRIDE`, `DETECTOR_LOOP` | exist (design) | 4 / true | decimation and clip replay |
| `DETECTOR_REALTIME_PACING` | **new** | `true` | emit sampled frames at wall-clock rate rather than as fast as the CPU allows |
| `DETECTOR_CLIP_FPS` | **new** | from `CAP_PROP_FPS` | the declared rate pacing targets; overridable |
| `DETECTOR_START_DELAY_S` | **new** | 0.0 | grace from detector spawn before the first emitted frame |
| `TRACK_TIMEOUT_MS` | exists (design) | 1000 | unchanged |

IVI: **no new config, no participation.**

### 6.2 Clock-domain ruling — the design decision architecture must make

The ADA HLD leaves this open: its admission diagram compares `now - lastUpdated > TRACK_TIMEOUT_MS` without saying which clock `now` reads, while `timestamps.*` are epoch ms. Resolve it as:

| Purpose | Clock |
|---|---|
| Wire and log timestamps — `rxTime`, R3 `timestamps.*`, `[EVT] epoch_ms` | `CLOCK_REALTIME` (`system_clock` / `time.time()`) |
| Intervals — pacing deadlines, dedupe window, **track expiry** | `CLOCK_MONOTONIC` (`steady_clock` / `time.monotonic()`) |
| Arithmetic mixing two nodes' timestamps | **forbidden** |

R3 field semantics, which also fixes the swapped-timestamps defect recorded as M1 in [phase2-4-pr3-review.md](../plans/doc/phase2-4-pr3-review.md):

| Field | `v2x_relayed` | `own_sensor` |
|---|---|---|
| `measured` | `rxTime + timeOfMeasurement` — both from the same R2 message, so one clock domain, and correct-by-construction when `measurementDeltaTime` stops being 0 | detector's stamp at frame capture |
| `received` | `rxTime` (V2X clock, record value) | detector emit time |
| `lastUpdated` | **always ADA's own `CLOCK_REALTIME` at store write** | same |

Expiry keeps a parallel `CLOCK_MONOTONIC` stamp per track and compares against that — five lines, and it makes the run immune to the host's own NTP daemon stepping the shared wall clock mid-demo, which would otherwise expire every track at once.

### 6.3 Where the latency numbers come from, and how to measure them

- **Bench→V2X and V2X→ADA hops, and the clock offset in one shot.** The `[CAP]` tcpdump lines already carry µs timestamps on both nodes, and a datagram is identifiable by length plus the `rxTime` in its body. The same R2 frame's `Out` timestamp on V2X and `In` timestamp on ADA differ by `transit + clock offset`; over ~100 samples the minimum is a tight bound on the offset. This repeats §2(a)'s derivation with three more digits and no new tooling.
- **ADA internal.** `[EVT]` deltas between `r2_ingest` and `r4_tx` using `mono_ms` — the V2X event log already emits both `mono_ms` and `epoch_ms`; ADA's should match that shape.
- **Bench pacing.** The `[TX]` JSONL currently carries `{seq, scenario_time_s, bytes}` and **no time stamp at all**; add `mono_ms` so scenario time can be regressed against elapsed time.
- **ADA→IVI.** Not measurable without a guest-side stamp; declared outside the budget. The IVI is edge-triggered and renders on receipt, so it needs no budget.

### 6.4 How a run is verified — `ADA_ECU/tools/check_run_alignment.py`

Every check reads timestamps produced by **one** clock, so none of them depends on cross-node agreement.

| # | Check | Bound | Source |
|---|---|---|---|
| K1 | At every `r4_tx`, a `tracked` `own_sensor` B entry exists whose `lastUpdated` is within `TRACK_TIMEOUT_MS` | binary pass | ADA `[EVT]` JSONL |
| K2 | The first `own_sensor` → `tracked` transition precedes the first `v2x_relayed` → `tracked` transition | binary pass | ADA `[EVT]` JSONL |
| K3 | `max │lastUpdated(own_sensor B) − lastUpdated(v2x_relayed C)│` over all `r4_tx` | **≤ 1000 ms** | ADA `[EVT]` JSONL |
| K4 | Detector frame-index advance rate vs its own emit-timestamp advance rate, over ≥ 60 s | within **±2 %** of `DETECTOR_CLIP_FPS / DETECTOR_FRAME_STRIDE` | detector R3 JSONL |
| K5 | Bench `scenario_time_s` advance vs `mono_ms` advance, over ≥ 60 s | within **±1 %** | bench `[TX]` JSONL |

### 6.5 What is *not* changed

- **(a) No frozen contract changes.** R1 keeps `referenceTime` + `measurementDeltaTime`; R2 keeps `rxTime` + `timeOfMeasurement`; R3 keeps its `timestamps` triple; R4 keeps no timestamp field. A `t` field on R4 was considered for IVI-side staleness and rejected: it costs a re-freeze across the schema, the ADA emitter, the Kotlin decoder and both round-trip suites, to replace a countdown the IVI already runs locally.
- **(b) One conformance defect, not a contract change.** The bench populates `referenceTime` with `int(time.time() * 1000)` — Unix epoch ms — while the frozen R1 profile defines it as `TimestampIts`, ms since 2004-01-01T00:00:00.000 TAI (golden vector `716084805123`). It passes the schema's upper bound so nothing rejects it, and it changes no M1 behaviour, because the V2X ECU uses `referenceTime` only as a *difference* for the F1 speed derivation and never forwards it. It is still non-conformant to a frozen profile and should be fixed with an epoch constant in config, not a literal.
- **(c) `measurementDeltaTime` is always 0 on the wire** — the generator never passes the third argument to `Scenario.sample`. Harmless for M1; §6.2's `measured` rule stays correct when it changes.
- **(d) No blueprint or topology change.** No new node, no new pin, no new edge.

## 7. Enumerated requirements

Two new numbers, continuing after R19. Ordering is by **urgency** — R20 is the enabler R21 measures.

**R20 — Real-time paced stimulus sources.** *(new)*

- **Definition:** every source of demo stimulus advances scenario time at 1.0× wall time, scheduled against `CLOCK_MONOTONIC` deadlines rather than accumulated fixed sleeps. Two sources: the bench CPM generator (R11) and the ADA video detector (R12). Offsets and rates are configuration (§6.1), never literals.
- **Vague → precise:** *"our scenario may need time sync between the ECU"* → **no clock-synchronisation protocol**; each source advances its own scenario time at 1.0× wall time, verified against its own clock. *"trigger event requiring ADA-ECU to detect video early enough"* → the detector runs continuously and paced; "early enough" is R21's ordering constraint, not a trigger.
- **Measurable output:** K5 ≤ ±1 % (bench) and K4 ≤ ±2 % (detector) over a ≥ 60 s run, per §6.4.
- **Dependency:** R11 (bench half), R12 (detector half).
- **Feasibility: at-risk.** The bench half is **achievable** — a deadline-scheduled loop plus a `mono_ms` field in the `[TX]` line, roughly one hour against code that already runs live at 10 Hz. The detector half is **at-risk by inheritance**: Phase 3 has no committed detector code (the current one decodes frames and discards them, returning a synthetic distance) and the clip is still an undelivered user deliverable. The pacing change itself is ~2 hours *inside* Phase 3 work that must happen regardless — it adds no new risk of its own, but it cannot land before Phase 3 does.
- **Tech stack:** — (Python `time.monotonic()`; C++ `std::chrono::steady_clock`).

**R21 — Run alignment and cross-source temporal correlation.** *(new)*

- **Definition:** one demo run presents one scenario timeline. Vehicle B reaches `tracked` before vehicle C is admitted, and at every R4 emission the store's newest `own_sensor` and newest `v2x_relayed` entries are close enough in time that the composed geometry `d_AC = d_AB + d_BC` describes a single instant. Achieved by configured start offsets against each node's own process start (§6.1) with the operator's bench restart as the run start — **no orchestrator, no trigger message, no clock exchange**.
- **Vague → precise:** *"Bench should have sent the V2X message way before"* → the bench leads by the pipeline latency, bounded < 55 ms — 0.55 of one CPM period, supplied automatically by the continuous 10 Hz stream (§3.2). *"the ADA-ECU should hold the authoritative centralized clock"* → the cross-node timestamp of record is R2 `rxTime`, stamped by the V2X ECU; no node serves time to another. *"sync time between ECU at system startup"* → readiness is R5's Deployment-Viewer check plus `start_delay_s`; no offset exchange. *"early enough"* → B reaches `tracked` at least 0.6 s (3 detections at 5 Hz) before C crosses `gate_enter`, i.e. the detector must be producing detections within 11.4 s of the bench cycle start, against a 12.0 s lead-in.
- **Measurable output:** K1 and K2 pass, and K3 ≤ 1000 ms, on the recorded R19 run (§6.4).
- **Dependency:** R20, R13, R14, R15, R18.
- **Feasibility: at-risk.** The mechanism is trivial (two config keys, one check script, ~0.5 day). The risk is entirely inherited: K1–K3 are unmeasurable until Phase 3 produces `own_sensor` B tracks and Phase 4's ADA runtime is repaired and merged. With six days left and Phase 3 not started, the honest verdict is at-risk.
- **Tech stack:** — (Python check script; no new library).

**Whole-input feasibility verdict: achievable, and smaller than the question implies.** The clock-synchronisation half of the input is **not needed** and is therefore free. The pacing and alignment half is **at-risk**, not because of its own cost — roughly one day total — but because it sits on top of Phase 3, which has no code and no video clip six days from the deadline.

## 8. Scope flags — for the user to accept or reject

1. **R20 and R21 are demo-quality work, not R19-gating work.** R19's acceptance is the recorded run, the two pcaps, zero C on the detection log, and ghost C sourced from `v2x_relayed`. **None of those four fails when the timing is misaligned.** What misalignment costs is credibility: a god view where B's distance sweeps a whole clip in ten seconds while C closes over twelve reads as broken to a jury. Accepting R20/R21 is accepting polish, and it should be scheduled behind Phase 3 and Phase 4 acceptance, not in front of them.
2. **The cheap half should be done regardless.** The bench deadline-scheduling fix and the `mono_ms` field in `[TX]` are ~1 hour on code that already runs live, and they close K5 immediately. The detector pacing is ~2 hours *inside* Phase 3 rather than extra to it.
3. **Nothing here pulls a deferred item into M1.** No item from the report's § Future developments or [milestone1.md §6](../plans/milestone1.md) is touched. Note the interaction the other way: if the deferred **IVI dashcam view** is ever accepted ([m1-video-source-and-ivi-dashcam.md §8](m1-video-source-and-ivi-dashcam.md)), R20's detector pacing stops being polish and becomes mandatory — that note already flagged it, and R20 is where it now lives.
4. **The `referenceTime` epoch defect (§6.5(b)) is a conformance fix awaiting the user's word** on whether to spend an hour on a field that changes no M1 behaviour.

## 9. Open items — what could not be verified, and the check that would settle it

| # | Unverified | Check |
|---|---|---|
| 1 | Whether the Room's pods are co-scheduled on one host — CarSky documents namespace and resource pool only, never affinity | Re-run §6.3's `[CAP]` correlation on each redeploy; the design does not depend on the answer |
| 2 | Bench→V2X and ADA-internal latency — only the V2X in→out 142–151 µs is measured | §6.3, on the next live Room |
| 3 | ADA→IVI hop latency into the AAOS guest | Needs a guest-side stamp; declared out of budget |
| 4 | Detector warm-up time (ONNX load + `VideoCapture` open), estimated 2–5 s — it consumes the 6.4 s slack | First Phase 3 run on the deployed node |
| 5 | AAOS boot-to-listener time, which sets the bench `start_delay_s` floor | Phase 5 produces it; Phase 0 could not (ADB returns 502, residual O4) |
| 6 | Ethernet Bridge MTU and jitter — the bridge is a tunnelled fabric on TCP 29400, and no platform figure exists | Smoke-test residual O3's `PAD=1400` bisect |
| 7 | Whether the host's own NTP daemon steps the shared wall clock during a run | Unobservable from inside; §6.2's monotonic expiry stamp removes the exposure rather than measuring it |

## Sources

- [m1-cooperative-awareness.md](m1-cooperative-awareness.md) — R1–R6 contracts, R11–R15, R19; §4 decision record.
- [milestone1.md](../plans/milestone1.md) — §2 assumptions, §4 track-admission gate, Phases 0–6 acceptance, §6 deferred scope.
- [phase1-comms-run.md](../plans/doc/phase1-comms-run.md) — the live R2 excerpt with `rxTime` against the ADA sink's own log clock (§2(a)); the 142–151 µs V2X in→out measurement; the `[CAP]` In/Out/P reading rules.
- [phase0-smoke-test-run.md](../plans/doc/phase0-smoke-test-run.md) · [baseline-connectivity-smoke-test.md](../plans/doc/research_notes/baseline-connectivity-smoke-test.md) · [netcheck.py](../tools/netcheck/netcheck.py) — the smoke test is a one-way relay chain, not a handshake; `START_DELAY_S` is its only readiness mechanism; hop 3 unconfirmed (O4).
- [phase2-4-pr3-review.md](../plans/doc/phase2-4-pr3-review.md) — the true ADA starting state: no service loop, detector is a placeholder, R3 timestamps swapped (M1).
- [scenario-player-hld.md](../Scenario_Player/doc/scenario-player-hld.md) · [generator.py](../Scenario_Player/player/generator.py) · [config.py](../Scenario_Player/player/config.py) · [default.yaml](../Scenario_Player/scenarios/default.yaml) — the tick-counter scenario clock, the fixed-sleep loop, the YAML key set, the persistent `cpm_encode --stream` codec path (D1).
- [phase2-4-ada-ecu-hld.md](../ADA_ECU/doc/phase2-4-ada-ecu-hld.md) — `TRACK_TIMEOUT_MS` as wall-clock silence with the clock source unspecified; `DETECTOR_FRAME_STRIDE` as decimation, not a rate; `Frame.timestamp_ms` source unstated.
- [ivi-ecu-hld.md](../IVI_ECU/doc/ivi-ecu-hld.md) · `R4Message.kt` · `build.gradle.kts` — R4 carries no timestamp; `WARNING_TIMEOUT_MS` is a local countdown; R3 timestamps parsed and unused.
- [r1-cpm-profile.md](../contracts/r1-cpm-profile.md) and the R1–R4 schemas under [contracts/](../contracts/) — `TimestampIts` epoch definition, F1/F8/F9, and the exact frozen time fields.
- [Car-Sky-Platform.html](development-platform-doc/Car-Sky-Platform.html) — Room as a K8s namespace; Device fixes namespace and resource pool only; Skycraft as a full guest VM; Ethernet Bridge as an unmanaged software switch on TCP 29400; no time service, no scheduler, no readiness probe, no ordering primitive.
- [BTC_phan_hoi_V2X_team.pdf](development-platform-doc/BTC_phan_hoi_V2X_team.pdf) — §3, the ADA ECU as an external node on a separate server, with the bench required to follow it.
- [m1-video-source-and-ivi-dashcam.md](m1-video-source-and-ivi-dashcam.md) — the clip is baked into the ADA image with no external arming path; detector real-time pacing becomes mandatory if the dashcam view is accepted.
