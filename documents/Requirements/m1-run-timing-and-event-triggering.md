# Run Timing, Startup Flow, and Demo Event Triggering Across the Four M1 Nodes

Requirement analysis & technical solution report per [research-report-format.md](../../.claude/rules/research-report-format.md). Answers the three questions in [2026-08-02-time-sync-and-demo-event-triggering.md](../../.claude/prompts/2026-08-02-time-sync-and-demo-event-triggering.md) — is a startup call flow / time-sync module needed, is a demo trigger application needed, and how — and the run-choreography requirement that fixes when the IVI's warning screen appears (§6.6).

It defines **three new requirement numbers, R20, R21 and R22** (§7), continuing the project-global numbering after R19. R1–R19 in [m1-cooperative-awareness.md](m1-cooperative-awareness.md) are untouched, and **no frozen contract changes** (§6.5).

Diagrams: [m1-run-timing-startup-flow.puml](m1-run-timing-startup-flow.puml) (startup + triggered run) · [m1-run-timing-alignment.puml](m1-run-timing-alignment.puml) (one cycle on the run timeline).

## 1. The three answers, up front

| # | Question | Answer |
|---|---|---|
| 1 | Startup call flow + time-sync module between ECUs | **No clock synchronisation, and no node-to-node handshake.** The measured offset between the two container nodes is already **under 55 ms** (§2), no node performs arithmetic on another node's timestamp, and the AAOS guest reads no timestamp at all. Readiness is a separate question with a separate answer: it is already covered by R5's Deployment-Viewer check plus a configured bench `start_delay_s` — building a ping-everyone barrier would cost an ego-software reverse channel and buy nothing R5 does not already give. |
| 2 | Demo/test trigger application issuing "read video at A, send CPM at B" | **No orchestrator process, no trigger message.** Each stimulus source self-schedules from its own config against its own process start; the operator's *restart the bench node* action is the GO. The only tool worth writing is a **post-run verification script**, not a trigger. |
| 3 | How | Pace both stimulus sources to real time on `CLOCK_MONOTONIC` deadlines, put every offset in config (§6.1), fix which clock stamps which R3 field (§6.2), place the demo events with bench scenario data and node configuration (§6.6), and verify with `ADA_ECU/tools/check_run_alignment.py` against ADA's own clock alone (§6.4). Total new code: roughly one day, most of it inside Phase 3 work that has to happen anyway. |

## 2. What contradicts the premise — read this first

The problem statement assumes clock divergence between ECUs. Four findings say the problem is elsewhere.

**(a) The container-node clocks already agree to within 55 ms — measured, not assumed.** The Phase 1 run record ([phase1-comms-run.md](../../plans/doc/phase1-comms-run.md)) contains a usable cross-node measurement that nobody took it for. The V2X ECU stamps `rxTime` from its own `system_clock`; the ADA-side sink prints its own `time.strftime('%H:%M:%S')` ([netcheck.py:13](../../tools/netcheck/netcheck.py)) on receipt. Five consecutive messages:

| Msg | `rxTime` (V2X clock) | as UTC | ADA sink printed |
|---|---|---|---|
| #5899 | 1785591216646 | 13:33:36.646 | 13:33:36 |
| #5900 | 1785591216745 | 13:33:36.745 | 13:33:36 |
| #5901 | 1785591216846 | 13:33:36.846 | 13:33:36 |
| #5902 | 1785591216946 | 13:33:36.946 | **13:33:36** |
| #5903 | 1785591217046 | 13:33:37.046 | **13:33:37** |

Let δ = (ADA print instant, ADA clock) − (`rxTime` instant, V2X clock) = transit + handling + clock offset. #5902 printing in second 36 requires `0.946 + δ < 1.000` → **δ < 54 ms**. #5903 printing in second 37 requires `0.046 + δ ≥ 0` → **δ > −46 ms**. Transit and handling are non-negative, so the clock offset is under 54 ms in one direction and, with per-datagram handling under 10 ms, under about 56 ms in the other. **|offset| < ~55 ms, and the whole V2X→ADA hop including transit is inside the same bound.**

Caveat, stated rather than glossed: this bounds two *container* nodes in one deployment. CarSky documents no co-scheduling — a Room is "a Kubernetes namespace where the Nydus Operator builds pods/services for every node", the Device only fixes "namespace, resource pool", and there is no `nodeSelector`/affinity statement anywhere ([Car-Sky-Platform.html](../../requirements/development-platform-doc/Car-Sky-Platform.html)). BTC's advisory even contemplates the ADA ECU as an external node on a separate server. So this is a per-deployment property to re-check, not a guarantee — which is exactly why the recommended design does not depend on it.

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

- **Bench** — sleeps a fixed `period` per tick with no drift correction, and derives scenario time as `t = cycle_tick * period` ([generator.py](../../Scenario_Player/player/generator.py)), a tick counter rather than a clock. Scenario time therefore drifts from wall time by the per-tick work cost, unbounded over a run.
- **ADA detector** — designed and unbuilt, with **no pacing of any kind**: `DETECTOR_FRAME_STRIDE=4` is a decimation stride, and "5 Hz effective" is an assumed CPU throughput, not an enforced rate. A 60 s clip is consumed in whatever wall time the CPU takes, then looped. This is the dominant error term by two orders of magnitude, and it is a `time.sleep` away from being fixed — the pacer this finding produced is [ada-ecu-design-decisions.md D10](../Design/ADA-ECU/ada-ecu-design-decisions.md#d10--clock-domains-and-stimulus-paced-against-clock_monotonic).

## 3. The timing model

Three distinct quantities. Two already have repo names; the third is described rather than coined ([markdown-writing-style](../../.claude/skills/markdown-writing-style/SKILL.md) rule 5).

| Quantity | Existing name | Where it lives |
|---|---|---|
| The timeline the demo is authored against | **scenario time** `t` — already the bench's term, emitted as `scenario_time_s` in its `[TX]` JSONL | Bench only. Nothing else in the system has one. |
| Per-container wall clock | `CLOCK_REALTIME` / `system_clock` / `time.time()` | V2X `rxTime`, R3 `timestamps.*`, `[EVT] epoch_ms` |
| Per-container interval clock | `CLOCK_MONOTONIC` / `steady_clock` / `time.monotonic()` | V2X dedupe window, `[EVT] mono_ms` |
| Bench-emit → ADA-store delay | described as **pipeline latency**; no term coined | §3.1 |

The **run timeline** every demo event is placed on is `T0`-relative, and `T0` is the ADA detector's first emitted R3 frame — defined and justified in §6.6.

### 3.1 Pipeline latency budget

| Segment | Value | Source |
|---|---|---|
| Bench tick → CPM bytes (persistent `cpm_encode --stream` subprocess, not a fork per message) | unmeasured; pipe round-trip to a resident process | [SP D1](../Design/SCENARIO-PLAYER/scenario-player-design-decisions.md) |
| Bench → V2X, 58 B UDP over R6 | unmeasured individually | — |
| V2X decode → validate → dedupe → build → `sendto` | **142–151 µs, measured, stable** | [phase1-comms-run.md](../../plans/doc/phase1-comms-run.md) |
| V2X → ADA, ~339 B UDP | **inside the < 55 ms bound of §2(a)**, together with the clock offset | derived |
| ADA R2 ingest → store → R14 → R4 build | unmeasured; in-process C++, driven by the `FUSION_TICK_MS` = 100 ms tick | — |
| ADA → IVI, UDP into the AAOS guest | unmeasured — the only hop crossing into a VM | — |
| IVI parse → Compose recomposition | unmeasured; one frame ≈ 16–33 ms | — |

Against the stimulus periods: **CPM period 100 ms** (`cpm_rate_hz` 10), **detector sample period ≤ 200 ms** (R12's ≥ 5 Hz floor), **video frame period 50 ms** at 20 fps. The whole bench-tick → rendered-frame path is budgeted at **≤ 200 ms**, dominated by the fusion tick.

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

Budget over one cycle, on the `T0` run timeline of §6.6. Inputs: [default.yaml](../../Scenario_Player/scenarios/default.yaml) (`cpm_rate_hz` 10, `duration_s` 10.0, `loop` true; C at 70 m closing 5.0 m/s), the committed clip `ADA_ECU/media/ego-b-occluding-c.mp4` (10.0 s, B's range closing ~60 m → ~10 m), R13 (`gate_enter` 30 m, `CONFIRM_HITS` 3) and R14 (`RISK_DWELL_MS` 300 ms).

| Event | Time from `T0` |
|---|---|
| Cycle start — C at 70 m, B at ~60 m | 0.0 s |
| B's estimated range crosses `gate_enter`, 30 m | 6.0 s |
| B reaches `tracked` — 3 detections at 5 Hz | **6.6 s** |
| C's relayed range crosses `gate_enter`, 30 m | 8.0 s |
| C reaches `tracked` — 3 CPMs at 10 Hz | **8.3 s** |
| First R4 warning on the wire — confirmation, dwell 300 ms, fusion tick and pipeline | **8.5–8.9 s** |
| Cycle end — bench wraps to 70 m, clip wraps to ~60 m, both tracks dropped | 10.0 s |

**B leads C by 1.7 s.** That is the ordering margin the whole design rests on, and it tolerates a detector range bias up to a factor of 1.40 before B's admission crosses C's (§6.6).

Two properties of the numbers, both load-bearing:

- **The bench cycle length equals the clip length**, so B and C are admitted and dropped inside the same window. A bench cycle longer than the clip leaves C tracked while B is absent, which sends the assessment to `low` on D5's `b_unknown` path and drops the warning mid-run.
- **Detector warm-up sits before `T0`**, not inside the 1.7 s. ONNX load plus `VideoCapture` open — estimated 2–5 s, unmeasured — is what the bench's `start_delay_s` cancels, and it consumes none of the ordering margin.

`loop: true` restarts the choreography every 10.0 s, so the demo repeats without operator action. It does **not** correct a start offset: with bench cycle and clip loop at the same period, a constant offset between them persists for the whole run. `start_delay_s` is therefore a measured value, not a guess — budget in §6.6.

### 3.4 Error budget — where correlation error actually comes from

| Term | Magnitude without §6 | Magnitude with §6 |
|---|---|---|
| Detector rate error (free-running, no pacing) | **unbounded and growing** — a 60 s clip may be consumed in ~10 s or ~120 s | ≤ 2 % of elapsed |
| Bench scenario-clock drift (`sleep(period)`, no deadline) | ~1 % accumulating (≈ 0.6 s over 60 s) | ≤ 1 % of elapsed |
| Start offset between the two sources | unbounded | set by `start_delay_s` to the measured detector warm-up; budget **−0.5 / +1.1 s** (§6.6) |
| Sampling granularity | `max(100 ms, 200 ms)` = 200 ms | unchanged — a floor, not a defect |
| Pipeline latency | < 55 ms bench→ADA; ≤ 200 ms bench→rendered frame | unchanged |
| **Cross-node clock offset** | **< 55 ms, and multiplied by zero — no consumer performs cross-node time arithmetic** | unchanged |

The last row is the finding: clock offset is both the smallest term and the only one with no consumer.

## 4. Question 1 — is a startup call flow or time-sync module needed?

Readiness and clock sync are separable and are treated separately, as asked.

### 4.1 Clock synchronisation — candidate comparison

Hard constraints ([solution-selection-criteria.md](../../.claude/rules/solution-selection-criteria.md)) pass for all five (chrony, ntpd, linuxptp are open-source and Linux). Ranked criteria: **C1** accomplishability · **C2** fastest for M1 · **C3** future features · **C4** smaller dependency.

| # | Candidate | Cost by 2026-08-08 | Buys for R19 | Risk | Verdict |
|---|---|---|---|---|---|
| S-1 | **No sync — each node stamps from its own `CLOCK_REALTIME`/`CLOCK_MONOTONIC`** | **zero** | Nothing missing: §2(b) shows no consumer needs it | Cross-host placement could widen the offset — mitigated because no consumer reads it | **Selected** |
| S-2 | Demo-epoch broadcast — one node announces `t0`, others express time as offset-from-epoch | ~0.5 day: one datagram shape, one listener per node | Removes the `start_delay_s` measurement R22 needs | New listener inside ego software; a demo-control surface in ADA | Runner-up — the *scheduling* half is kept in §5, the *time* half is dropped |
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

**Pick: B-1.** Drivers **C1** and **C2**.

**Where the AAOS boot time binds.** `start_delay_s` is set by the detector warm-up (§6.6), so it is not free to absorb the guest's boot time. The IVI-readiness constraint is stated directly on the IVI instead: **the app must be listening on its R4 port before `T0` + 8.0 s**, because that is the earliest instant a warning can arrive. Phase 5 produces the boot-to-listener number, and the guest boots before the ADA detector finishes warming up in every observed deployment — the check is R5's Deployment-Viewer pass, performed by the operator before the recording starts.

## 5. Question 2 — should there be a demo trigger application?

| # | Candidate | Where offsets live | Cost | Verdict |
|---|---|---|---|---|
| T-1 | **Static self-schedule** — each source starts its scenario timeline at its own process start plus a configured delay; operator's bench restart is the GO | bench YAML `start_delay_s`; ADA env `DETECTOR_START_DELAY_S` | **~0.5 day**; no new message, no new link, no contract change | **Selected** |
| T-2 | Shared scenario config + a single "GO" broadcast (S-2's scheduling half) | same config keys, plus one datagram shape | ~1 day; new bench→ADA link, absent from the R5/R6 topology; new listener in ego software | **Fallback** — adopt only if the measured K3 or K6 in §6.4 exceeds its bound |
| T-3 | Central orchestrator/conductor node issuing start commands | orchestrator config | ~2 days; a fifth container node, listeners in bench and ADA, blueprint change | Rejected on C1, C2, and on R19's "no scripted shortcuts inside ego software" |
| T-4 | Bench alone drives; ADA arms the clip on the first R2 | bench YAML only | ~0.5 day; tightest alignment | Rejected — it puts demo-shaped behaviour inside ego software. Also blocked by the video-source design: ADA reads a clip baked into its own image with no external arming path |
| T-5 | Manual/human start | operator procedure | zero | **Retained as part of T-1** — the bench restart *is* the manual start |

**Pick: T-1 + T-5.** Drivers **C1** (nothing in its path is unverified) and **C2** (two config keys and a paced loop, against six days). C3 is preserved: T-2 slots in behind the same config keys without changing them.

**The tool that should exist is a checker, not a trigger.** `ADA_ECU/tools/check_run_alignment.py` — post-run verification of §6.4's KPIs from the R18 JSONL. Placement follows the `ADA_ECU/tools/check_zero_c.py` precedent in [CLAUDE.md § Repository layout](../../CLAUDE.md): a check script lives in the node folder whose log it reads. It is **sanctioned bench test equipment, not production code** (governing principle 2) and is never on the ego data path.

**Offsets live in config, never in code** (CLAUDE.md principle 5) — §6.1.

## 6. Question 3 — how

### 6.1 Config keys

Bench — [Scenario_Player/scenarios/*.yaml](../../Scenario_Player/scenarios/), validated by `player/config.py`:

| Key | Status | Value | Meaning |
|---|---|---|---|
| `cpm_rate_hz` | exists | 10.0 | tick rate; period `1/cpm_rate_hz` |
| `duration_s` | exists | **10.0** | cycle length; equal to the committed clip's length so the two wrap together (§3.3) |
| `loop` | exists | true | restart the cycle at `duration_s` |
| `object.initial_distance_m` | exists | **70.0** | C's relayed range at cycle start; derived in §6.6 |
| `object.closing_speed_mps` | exists | **5.0** | C's closing rate on B; derived in §6.6 |
| `start_delay_s` | **new** | **the measured detector warm-up `W`**, proposed 3.0 | grace from process start before the first CPM; budget −0.5 / +1.1 s around `W` (§6.6) |
| `reference_time_epoch` | **new** | `its` | epoch used for CPM `referenceTime`; see §6.5(b). Never a literal in the loop |

ADA — env, per the HLD's env table:

| Key | Status | Value | Meaning |
|---|---|---|---|
| `DETECTOR_FRAME_STRIDE`, `DETECTOR_LOOP` | exist (design) | 4 / true | decimation and clip replay |
| `DETECTOR_REALTIME_PACING` | **new** | `true` | emit sampled frames at wall-clock rate rather than as fast as the CPU allows. **Mandatory for R22** — without it clip time is not run time |
| `DETECTOR_CLIP_FPS` | **new** | 20.0, from `CAP_PROP_FPS` | the declared rate pacing targets; overridable |
| `DETECTOR_START_DELAY_S` | **new** | 0.0 | grace from detector spawn before the first emitted frame. R22 aligns by moving the bench, so this stays at 0 and leaves `W` as the only unknown |
| `TRACK_TIMEOUT_MS` | exists (design) | 1000 | unchanged |
| `RISK_NEAR_M` | exists (design) | **60** | the `medium` threshold on the **composed** range `d_AC`; sized so the range clause alone commits the transition at C's admission (§6.6) |
| `RISK_CRITICAL_M` | exists (design) | **30** | the `high` threshold on `d_AC` (§6.6) |

IVI: **no new config.** `WARNING_TIMEOUT_MS` stays 10000 — the countdown that returns the Display Area to Home on R4 silence.

### 6.2 Clock-domain ruling — the design decision architecture must make

The ADA HLD leaves this open: its admission diagram compares `now - lastUpdated > TRACK_TIMEOUT_MS` without saying which clock `now` reads, while `timestamps.*` are epoch ms. Resolve it as:

| Purpose | Clock |
|---|---|
| Wire and log timestamps — `rxTime`, R3 `timestamps.*`, `[EVT] epoch_ms` | `CLOCK_REALTIME` (`system_clock` / `time.time()`) |
| Intervals — pacing deadlines, dedupe window, **track expiry** | `CLOCK_MONOTONIC` (`steady_clock` / `time.monotonic()`) |
| Arithmetic mixing two nodes' timestamps | **forbidden** |

R3 field semantics, which also fixes the swapped-timestamps defect recorded as M1 in [phase2-4-pr3-review.md](../../plans/doc/phase2-4-pr3-review.md):

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
- **Detector warm-up `W`.** The interval between the detector process spawn and its first emitted R3 line, on the deployed 2-vCPU node. It is the value `start_delay_s` is set to, so it is the one measurement R22 cannot proceed without.
- **ADA→IVI.** Not measurable without a guest-side stamp; declared outside the budget. The IVI is edge-triggered and renders on receipt, so it needs no budget.

### 6.4 How a run is verified — `ADA_ECU/tools/check_run_alignment.py`

Every check reads timestamps produced by **one** clock, so none of them depends on cross-node agreement.

| # | Check | Bound | Source |
|---|---|---|---|
| K1 | At every `r4_tx`, a `tracked` `own_sensor` B entry exists whose `lastUpdated` is within `TRACK_TIMEOUT_MS` | binary pass | ADA `[EVT]` JSONL |
| K2 | The first `own_sensor` → `tracked` transition precedes the first `v2x_relayed` → `tracked` transition | **by ≥ 1.0 s** | ADA `[EVT]` JSONL |
| K3 | `max │lastUpdated(own_sensor B) − lastUpdated(v2x_relayed C)│` over all `r4_tx` | **≤ 1000 ms** | ADA `[EVT]` JSONL |
| K4 | Detector frame-index advance rate vs its own emit-timestamp advance rate, over ≥ 60 s | within **±2 %** of `DETECTOR_CLIP_FPS / DETECTOR_FRAME_STRIDE` | detector R3 JSONL |
| K5 | Bench `scenario_time_s` advance vs `mono_ms` advance, over ≥ 60 s | within **±1 %** | bench `[TX]` JSONL |
| K6 | Δ between `T0` — the first `own_sensor` R3 line of the run — and the first `r4_tx` | **8.0 s ≤ Δ < 10.0 s** | ADA `[EVT]` JSONL |
| K7 | The IVI's first `[UI] mode=WarningView cause=warning` line is the run's first Warning-mode line, and follows the app's own startup `[UI] mode=HomeView` line by ≥ 8.0 s | binary pass | IVI logcat + the screen recording |

K6 and K7 each read one node's own clock. K7 is read by the operator from the guest, not by the check script, because logcat is not among the script's inputs.

### 6.5 What is *not* changed

- **(a) No frozen contract changes**, R20, R21 and R22 included. R1 keeps `referenceTime` + `measurementDeltaTime`; R2 keeps `rxTime` + `timeOfMeasurement`; R3 keeps its `timestamps` triple; R4 keeps no timestamp field. R22 changes *when* the first R4 leaves ADA, never its shape — the IVI is edge-triggered and renders on receipt, so it needs no time field to know the warning is current. A `t` field on R4 was considered for IVI-side staleness and rejected: it costs a re-freeze across the schema, the ADA emitter, the Kotlin decoder and both round-trip suites, to replace a countdown the IVI already runs locally. R5 gains configuration values in existing node-config blocks, which is additive; R6 gains nothing.
- **(b) One conformance defect, not a contract change.** The bench populates `referenceTime` with `int(time.time() * 1000)` — Unix epoch ms — while the frozen R1 profile defines it as `TimestampIts`, ms since 2004-01-01T00:00:00.000 TAI (golden vector `716084805123`). It passes the schema's upper bound so nothing rejects it, and it changes no M1 behaviour, because the V2X ECU uses `referenceTime` only as a *difference* for the F1 speed derivation and never forwards it. It is still non-conformant to a frozen profile and should be fixed with an epoch constant in config, not a literal.
- **(c) `measurementDeltaTime` is always 0 on the wire** — the generator never passes the third argument to `Scenario.sample`. Harmless for M1; §6.2's `measured` rule stays correct when it changes.
- **(d) No blueprint or topology change.** No new node, no new pin, no new edge.

### 6.6 R22 run choreography — geometry, configuration and contracts

The technical-change study behind R22 (§7). Every number below is scenario data or node configuration; none of it is code, and none of it touches a frozen contract (§6.5(a)).

#### (a) The run timeline

`T0` is the instant the ADA detector emits its first R3 `own_sensor` line. Every R22 statement is measured from it.

- **Why not deployment start.** Before `T0` the ego system perceives nothing, the god view has nothing to draw, and the HMI is trivially on its normal screen. A long startup only *lengthens* R22's normal-screen window, so measuring from `T0` is the conservative reading of "at least the first 8 s".
- **Why not the operator's GO.** `T0` is visible in the R18 JSONL without reading any other node's clock, which keeps K6 inside §6.2's single-domain rule. The operator's GO is not machine-observable.
- **Under R20's pacing, run time from `T0` equals clip time.** A clip second and a run second are the same second, which is what makes the choreography expressible as scenario data.

#### (b) What is fixed before the derivation

| Input | Value | Source |
|---|---|---|
| Clip length, frame rate | 10.0 s, 20 fps CFR, 200 frames | [ego-b-occluding-c.source.md](../../ADA_ECU/media/ego-b-occluding-c.source.md) |
| B's estimated range across the clip | ~60 m → ~10 m, taken as `d_AB(t) ≈ 60 − 5.0 t` | same, § Content verdict — an assumption until the retune measures it |
| Detector sample rate | 5 Hz (`DETECTOR_FRAME_STRIDE` 4 of 20 fps) | R12 |
| R13 gate | `GATE_ENTER_M` 30, `GATE_EXIT_M` 35, `CONFIRM_HITS` 3 | R13, ADA HLD §6 |
| Bench tick rate | `cpm_rate_hz` 10 | `default.yaml` |
| Risk debounce | `RISK_DWELL_MS` 300 | D5 |
| Bench tick → rendered frame | ≤ 200 ms | §3.1 |

#### (c) Bench geometry — the arithmetic

Solve backwards from the observable R22 constrains, the first R4:

```
t(first R4) = t_gate + CONFIRM_HITS/cpm_rate_hz + RISK_DWELL_MS + fusion tick + pipeline
            = t_gate + 0.2…0.3 s + 0.3 s + ≤0.1 s + ≤0.2 s
            = t_gate + 0.5…0.9 s
```

The 0.2…0.3 s confirmation term is the two readings of whether the admitting update counts toward `CONFIRM_HITS` (§9 item 6); the window below covers both.

R22 requires `8.0 s < t(first R4) < 10.0 s` and `7.0 s < t_gate < 10.0 s`. Setting **`t_gate = 8.0 s`** places the first R4 at **8.5–8.9 s** — at worst 0.5 s above R22's floor and 1.1 s below its ceiling — and puts `t_gate` itself a full second above its own 7.0 s floor.

With `duration_s = 10.0`:

```
initial_distance_m = GATE_ENTER_M + closing_speed_mps × t_gate
                   = 30 + 5.0 × 8.0
                   = 70.0 m
```

| Check | Value | Verdict |
|---|---|---|
| C outside the gate until 7.0 s | `d_BC(7.0) = 70 − 35 = 35 m` | pass — at the `gate_exit` level, still outside `gate_enter` |
| C crosses `gate_enter` | `(70 − 30)/5.0 = 8.0 s` | in (7.0, 10.0) |
| C reaches `tracked` | `8.0 + 3/10 = 8.3 s` | before the cycle end |
| C stays in the gate to the wrap | `d_BC` monotonically decreasing | one admission per cycle, no flicker |
| C's range at the last tick | `d_BC(9.9) = 20.5 m` | positive, and beyond B's `d_AB(9.9) = 10.5 m` |
| C dropped at the wrap | range jumps 20.5 m → 70.0 m, above `GATE_EXIT_M` 35 | pass |

**Why `closing_speed_mps = 5.0`.** It makes ego-to-C close at `5 + 5 = 10 m/s`, the same order as ego-to-B, so the god view's two ranges shrink at comparable rates. A larger value pushes `initial_distance_m` past a plausible CPM perceived-object range; a smaller one leaves the composed range nearly flat.

**Why `duration_s = 10.0`, matching the clip.** Any longer bench cycle leaves C tracked across a clip wrap, when B has been dropped and re-admitted; the assessment then returns `low` on D5's `b_unknown` path and the warning falls away mid-run. K1 fails for that whole interval. Equal periods is the only ratio that keeps both tracks inside one window.

**Why `loop: true`.** The choreography repeats every 10.0 s with no operator action, so a recording that starts late still captures a complete cycle.

#### (d) B's side of the ordering

- `d_AB` crosses `GATE_ENTER_M` at `(60 − 30)/5 = 6.0 s`; three detections at 5 Hz add 0.6 s, so **B reaches `tracked` at 6.6 s**, 1.7 s before C.
- The detector's range estimate carries an unmeasured bias. Writing `estimated = k × true`, B's crossing moves to `12 − 6/k` seconds. The ordering `t(B tracked) < t(C tracked)` holds while `12 − 6/k + 0.6 < 8.3`, i.e. **`k < 1.40`** — the detector may over-estimate B's range by 40 % before K2 breaks. Under-estimation only admits B earlier and is always safe.
- B is a coach, wider than the `VEHICLE_WIDTH_M` default, so the expected bias is `k < 1` — the safe direction.

#### (e) The risk band — why the bench alone cannot satisfy R22

The R13 gate and the R14 risk bands are thresholds on **different quantities**:

| Threshold | Quantity | Value at C's admission (8.3 s) |
|---|---|---|
| `GATE_ENTER_M` 30 | `d_BC`, the relayed range (R2 `object.distance`) | 28.5 m |
| `RISK_NEAR_M`, `RISK_CRITICAL_M` | `d_AC = d_AB + d_BC`, the composed range | `18.5 + 28.5 = 47.0 m` |

The composed range closes at 10.0 m/s, so `ttc(8.3) = 4.7 s`.

D5's band table is total and ordered, the first matching row winning: `high` = C `tracked` and (`d_AC ≤ RISK_CRITICAL_M` or `ttc ≤ RISK_TTC_CRITICAL_S`) · `medium` = C `tracked` and (`d_AC ≤ RISK_NEAR_M` or `ttc ≤ RISK_TTC_WARN_S`) · `low` = everything else. Two clauses can raise the warning, and they differ in robustness:

- **The range clause is a comparison; the TTC clause is a derivative.** `ttc = d_AC / closingRateMps`, and `closingRateMps` is `-(d_AC(t) − d_AC(t−Δ))/Δ` taken over the *estimated* `d_AB` — the noisiest quantity in the system — held through a 300 ms debounce.
- With `RISK_NEAR_M = 25` the range clause is not met at 8.3 s and only the TTC clause is (`4.7 ≤ RISK_TTC_WARN_S` 6). `d_AC` reaches 25 m at 10.5 s, past the wrap.
- With `RISK_NEAR_M = 60` the range clause is met outright — `47.0 ≤ 60`, 13 m of margin at `k = 1` — and the transition rests on no derivative.
- The 25 m value cannot be met by the range clause under any bench geometry. **At the admission instant** `d_BC = GATE_ENTER_M`, so `d_AC = d_AB + GATE_ENTER_M`; `d_AB` across the window in which C can be admitted is 10–20 m, so `d_AC ≥ 40 m` whenever C is first admitted. `d_BC` keeps shrinking afterwards, but the cycle wraps 1.7 s later — not enough for `d_AC` to reach 25 m.
- Sizing `RISK_NEAR_M` above `d_AC` at admission is what the range bias `k` sets the margin for: `d_AC(8.3) = 18.5k + 28.5`, which is 41.5 m at `k = 0.7` and 52.5 m at `k = 1.3`.

| `RISK_NEAR_M` / `RISK_CRITICAL_M` | Clause met at admission | First R4 | Display from the first R4 to the wrap | Range-clause bias tolerated |
|---|---|---|---|---|
| 25 / 15 | TTC only — `4.7 ≤ 6` | `medium` at 8.5–8.9 s | `medium` throughout; `high` needs `ttc ≤ 3`, reached at 10.0 s | none — the range clause never fires |
| **60 / 30** | range — `47.0 ≤ 60` | `medium` at 8.5–8.9 s | `medium` throughout | `k ≤ 1.70` |
| 60 / 40 | range | `medium` at 8.5–8.9 s | `medium`, then `high` from ≈ 9.5 s | `k ≤ 1.70` |
| 60 / 50 | range | `high` at 8.5–8.9 s | `high` throughout | `k ≤ 1.16` |

**Pick: 60 / 30**, on **C1** — it is the only pair whose transition rests on a distance comparison rather than a derived closing rate, with 13 m of margin at `k = 1` and the widest range-bias tolerance. The 25 / 15 pair reaches the same instant through the TTC clause alone; the 60 / 40 and 60 / 50 pairs trade the margin for a `high` state inside the ~1.3 s between the first R4 and the wrap.

#### (f) Candidate comparison — how the choreography is placed

All four candidates are configuration-only, so the hard constraints ([solution-selection-criteria.md](../../.claude/rules/solution-selection-criteria.md)) are met by all: no library, no platform lock, no licence.

| # | Candidate | What it changes | Verdict |
|---|---|---|---|
| G-A | **TTC route** — leave the ADA risk bands alone and pick a closing speed high enough that `ttc ≤ RISK_TTC_CRITICAL_S` at C's admission | bench only: `initial_distance_m` ≈ 134 m, `closing_speed_mps` ≈ 13 m/s | Rejected on **C1**. `ttc` is derived from a numerical derivative of the *estimated* `d_AB`, the noisiest quantity in the system; the transition would have to survive that jitter through a 300 ms debounce. It also needs a B-to-C closing rate of 47 km/h to work |
| G-B | **Rescale the risk bands to the composed range** and keep a plausible geometry | bench `initial_distance_m` 70.0, `closing_speed_mps` 5.0, `duration_s` 10.0; ADA `RISK_NEAR_M` 60, `RISK_CRITICAL_M` 30 | **Selected** |
| G-C | Keep a 20 s bench cycle and place the crossing inside it | bench only | Rejected on **C1**. The clip wraps at 10 s, so B is dropped and re-admitted mid-cycle while C stays tracked; the assessment returns `low` on the `b_unknown` path and K1 fails for that interval |
| G-D | Change nothing but `start_delay_s`, shifting the committed geometry into the window | bench `start_delay_s` only | Rejected on **C1**. A 20 s cycle against the 10 s clip produces its first risk transition at ≈ 17.0 s, when the composed range has fallen far enough for the TTC clause; no offset places that in (8.0 s, 10.0 s), and the cycle-length mismatch G-C fails on applies here too |

**Pick: G-B.** Drivers **C1** — its trigger is one threshold comparison rather than a differentiated range, and its geometry stays physically plausible — and **C2**, four values in one YAML file plus two node-config values, with no new code beyond R20's pacing. **C3** is preserved: G-A remains reachable behind the same keys if a later milestone wants a TTC-driven alarm, and the bands stay externalized so a retune is a node-config edit.

#### (g) Start alignment and its budget

`start_delay_s` makes the bench's scenario time coincide with the clip's time:

```
start_delay_s = W + DETECTOR_START_DELAY_S − σ
```

where `W` is the detector warm-up and `σ` the skew between the two nodes' process starts. With `DETECTOR_START_DELAY_S = 0` and `σ` small, **`start_delay_s = W`**.

Writing `δ` for the amount by which the bench runs ahead of the clip, the first R4 lands 8.5–8.9 s minus `δ` from `T0`, and each bound is set by the end of that window nearest it:

| Constraint | Bound on `δ` |
|---|---|
| First R4 after `T0` + 8.0 s — earliest first R4, 8.5 s | `δ < 0.5 s` |
| First R4 before `T0` + 10.0 s — latest first R4, 8.9 s | `δ > −1.1 s` |
| B `tracked` before C admitted | `δ < 1.6 s` |

**`start_delay_s` must be within −0.5 / +1.1 s of the true `W`.** `W` is estimated at 2–5 s and unmeasured (§9 item 4), so the file ships a proposed 3.0 s and the deployed value is set from the measurement. Matching periods make the offset stable across the run but do not correct it — a wrong `start_delay_s` is wrong for every cycle.

#### (h) Frozen contracts — none changes

| Contract | Effect of R22 |
|---|---|
| R1 CPM profile | None. The bench varies the values of existing fields; the field set, the encoding and the golden vectors are untouched |
| R2 V2X→ADA object message | None. Same fields, same rate |
| R3 TrackedObject | None. R22 constrains when a track is admitted, not what a track record holds |
| R4 ADA→IVI warning | None. R22 constrains the instant the first warning event leaves ADA; the message keeps its five required fields and no timestamp |
| R5 CarSky node deployment | Additive only — values inside existing `env` blocks on the ADA node, and a key in the bench's scenario file. No node, image or pin change |
| R6 Ethernet-bridge network | None |

A re-freeze is therefore not proposed, and none is needed: every R22 lever is a value, not a shape.

#### (i) The clip's content

The landed clip is `ADA_ECU/media/ego-b-occluding-c.mp4` — 1280×720, 20 fps CFR, 200 frames, 10.0 s. Its sidecar records a frame-by-frame inspection at 2 fps: **B is present and is the frontmost in-lane vehicle in 20 of 20 sampled frames**, including the first.

- **The clip therefore does not show a vehicle merging into the ego lane at 3 s.** B is the lead vehicle from frame 0.
- **The inspection sampled 20 frames of 200.** It fixes B's presence at half-second intervals; it does not fix B's estimated range series, which is what sets B's admission instant.

| # | Candidate | Cost | Verdict |
|---|---|---|---|
| V-1 | **Map the choreography onto the clip as landed** — B is the in-lane lead from frame 0 and reaches `tracked` when its estimated range crosses 30 m | zero | **Selected.** R22's ordering requirement is that B is `tracked` before C is admitted, which the clip satisfies with 1.7 s to spare. Nothing in R19's claim depends on how B entered the frame |
| V-2 | Re-cut from the raw source to include a pre-merge segment — the sidecar records the coach as "not yet the lead vehicle" before raw `t ≈ 6 s`, so a cut at raw `t = 3 s` would place its becoming-lead at clip `t ≈ 3 s` | ~half a day: re-download the gitignored raw file (SHA-256 recorded, so re-verifiable), one `ffmpeg` command, re-inspect 20 frames, update the sidecar's segment and hashes, rebuild and re-push the ADA image, re-run the detector evidence run and the range retune | **Available, not recommended.** "Not yet the lead vehicle" is not evidence of a lane merge — the coach may simply be further away, and a *different* vehicle may hold the ego lane in that window, which would be exactly the decoy the sidecar warns against |
| V-3 | Re-source a clip containing a lane merge at a specified second | the 22-clip search that produced this one, with a much narrower constraint | Rejected on **C1** and **C2**. Six days out, with no guarantee such footage exists under an acceptable licence |

**Pick: V-1**, drivers **C1** and **C2**. The merge is a narrative detail with no acceptance consequence; the ordering it was meant to secure is secured by the range gate instead.

#### (j) The IVI observable

- **Before the first R4** the Display Area shows `DisplayMode.Home` — the R16 layout with its Home / Apps / Settings areas and the bottom status bar. `MainViewModel` starts there, and only a message (`cause=warning`) or a tap (`cause=user`) moves it.
- **On the first `medium` R4** the Display Area switches to `Warning` and a `[UI]` line carrying the mode and `cause=warning` is logged. That line is K7's observable.
- **Active risk raises the warning; only silence lowers it.** A `medium` or `high` R4 raises the warning view. A `low` R4 updates the scene and restarts the countdown, raising nothing and dismissing nothing. `WARNING_TIMEOUT_MS` = 10000 of R4 silence dismisses it.
- **At the cycle wrap** both tracks are dropped and the assessment returns to `low`, emitted as a downgrade R4. The warning view stays up across the wrap, so a run shows one Home → Warning transition and no return. The longest R4 gap in a looping run is 8.1–8.5 s against the 10 s countdown — §9 item 9.
- **Evidence.** K6 comes from the R18 JSONL through `check_run_alignment.py`; K7 comes from the guest's logcat and the screen recording that R19 already requires. R22 adds no new evidence mechanism.

## 7. Enumerated requirements

Three new numbers, continuing after R19. Ordering is by **urgency** — R20 is the enabler R21 measures, and R22 is the run-level observable both serve.

**R20 — Real-time paced stimulus sources.** *(new)*

- **Definition:** every source of demo stimulus advances scenario time at 1.0× wall time, scheduled against `CLOCK_MONOTONIC` deadlines rather than accumulated fixed sleeps. Two sources: the bench CPM generator (R11) and the ADA video detector (R12). Offsets and rates are configuration (§6.1), never literals.
- **Vague → precise:** *"our scenario may need time sync between the ECU"* → **no clock-synchronisation protocol**; each source advances its own scenario time at 1.0× wall time, verified against its own clock. *"trigger event requiring ADA-ECU to detect video early enough"* → the detector runs continuously and paced; "early enough" is R21's ordering constraint, not a trigger.
- **Measurable output:** K5 ≤ ±1 % (bench) and K4 ≤ ±2 % (detector) over a ≥ 60 s run, per §6.4.
- **Dependency:** R11 (bench half), R12 (detector half).
- **Feasibility: at-risk.** The bench half is **achievable** — a deadline-scheduled loop plus a `mono_ms` field in the `[TX]` line, roughly one hour against code that already runs live at 10 Hz. The detector half is **at-risk by inheritance**: Phase 3 has no committed detector code, so the pacing change (~2 hours) cannot land before Phase 3 does. It adds no new risk of its own.
- **Tech stack:** — (Python `time.monotonic()`; C++ `std::chrono::steady_clock`).

**R21 — Run alignment and cross-source temporal correlation.** *(new)*

- **Definition:** one demo run presents one scenario timeline. Vehicle B reaches `tracked` before vehicle C is admitted, and at every R4 emission the store's newest `own_sensor` and newest `v2x_relayed` entries are close enough in time that the composed geometry `d_AC = d_AB + d_BC` describes a single instant. Achieved by configured start offsets against each node's own process start (§6.1) with the operator's bench restart as the run start — **no orchestrator, no trigger message, no clock exchange**.
- **Vague → precise:** *"Bench should have sent the V2X message way before"* → the bench leads by the pipeline latency, bounded < 55 ms — 0.55 of one CPM period, supplied automatically by the continuous 10 Hz stream (§3.2). *"the ADA-ECU should hold the authoritative centralized clock"* → the cross-node timestamp of record is R2 `rxTime`, stamped by the V2X ECU; no node serves time to another. *"sync time between ECU at system startup"* → readiness is R5's Deployment-Viewer check plus `start_delay_s`; no offset exchange. *"early enough"* → B reaches `tracked` at least 1.0 s before C is admitted, which the committed geometry delivers with 1.7 s (§3.3).
- **Measurable output:** K1 and K2 pass, and K3 ≤ 1000 ms, on the recorded R19 run (§6.4).
- **Dependency:** R20, R13, R14, R15, R18.
- **Feasibility: at-risk.** The mechanism is trivial (two config keys, one check script, ~0.5 day). The risk is entirely inherited: K1–K3 are unmeasurable until Phase 3 produces `own_sensor` B tracks and Phase 4's ADA runtime is repaired and merged.
- **Tech stack:** — (Python check script; no new library).

**R22 — Demo run choreography: warning onset after the eighth second.** *(new)*

- **Definition:** one demo run presents one 10.0 s cycle, repeating. `T0` — the ADA detector's first emitted R3 frame — is the run origin (§6.6(a)). Within one cycle: vehicle B reaches R13 `tracked` from ego's own perception before vehicle C is admitted; C's relayed range crosses `GATE_ENTER_M` strictly after `T0` + 7.0 s and strictly before `T0` + 10.0 s; and the **first R4 warning event leaves the ADA ECU strictly after `T0` + 8.0 s and strictly before `T0` + 10.0 s**, so the IVI holds its normal (Home) screen for at least the first 8.0 s of the run and shows the warning screen thereafter. Achieved by bench scenario data and node configuration only (§6.6) — no orchestrator, no trigger message, no contract change.
- **Vague → precise:**
  - *"a car merges into the ego lane at video t = 3 s and becomes a tracked object"* → the landed clip carries B in the ego lane from frame 0 (§6.6(i)); R22 fixes instead that **B reaches `tracked` at `T0` + 6.6 s and at least 1.0 s before C is admitted**.
  - *"at video t = 7 s vehicle C comes close — the moment B would broadcast its CPM"* → **C's relayed range `d_BC` crosses `GATE_ENTER_M` = 30 m at `T0` + 8.0 s**; the bench streams CPMs continuously at 10 Hz from its own start rather than broadcasting once.
  - *"some time later than the 7th second and before the clip ends"* → the gate crossing lies in the **open interval (`T0` + 7.0 s, `T0` + 10.0 s)**, placed at 8.0 s.
  - *"the ego system has a startup procedure, so the ADA ECU does not begin reading video at t = 0 of the run"* → detector warm-up `W` (ONNX session load + `VideoCapture` open) precedes `T0` and is cancelled by the bench's **`start_delay_s = W`**, held to **−0.5 / +1.1 s** (§6.6(g)).
  - *"the HMI shows the normal screen for some time of at least 8 s"* → **no R4 warning event is emitted before `T0` + 8.0 s**, so `DisplayMode` stays `Home`.
  - *"only after the 8th second does it display the warning screen"* → the first R4 lands in **(`T0` + 8.0 s, `T0` + 10.0 s)**, at `T0` + 8.5–8.9 s by design.
- **Measurable output:** **K6** — the first `r4_tx` occurs 8.0–10.0 s after `T0` — and **K7** — the IVI's first `[UI] mode=WarningView cause=warning` line follows its startup `[UI] mode=HomeView` line by ≥ 8.0 s — both per §6.4, on the recorded R19 run. K2's ≥ 1.0 s ordering margin holds on the same run.
- **Dependency:** R20 (without real-time pacing, clip time is not run time and no choreography is expressible), R21, R11, R12, R13, R14, R15, R16, R17, R18.
- **Feasibility: at-risk.** The mechanism is four values in [default.yaml](../../Scenario_Player/scenarios/default.yaml) and two on the ADA node — under an hour of edits. Three inputs it rests on are unmeasured: the detector warm-up `W` that sets `start_delay_s` (§9 item 4), B's estimated range series across the clip that sets B's admission instant (§9 item 8), and the clip's frame content beyond the 2 fps inspection (§9 item 8). It also carries the risk-band rescale of §8 flag 5, and it cannot be observed until Phase 3 and Phase 4 both land.
- **Tech stack:** — (scenario YAML and node configuration; no new library, no new tool beyond `check_run_alignment.py`).

**Whole-input feasibility verdict: achievable, and smaller than the questions imply.** The clock-synchronisation half of the input is **not needed** and is therefore free. The pacing, alignment and choreography half is **at-risk** — roughly one day of work in total, sitting on top of Phase 3 and Phase 4, six days from the deadline, with three measurements still outstanding.

## 8. Scope flags — for the user to accept or reject

1. **R20 and R21 are demo-quality work; R22 is an acceptance requirement in its own right.** R19's four acceptance items — the recorded run, the two pcaps, zero C on the detection log, ghost C sourced from `v2x_relayed` — **fail on none of them when the timing is misaligned**. What misalignment costs there is credibility: a god view where B's range sweeps a whole clip in ten seconds while C closes over twelve reads as broken to a jury. R22 is different: it states the observable directly, so a run that warns at 5 s fails R22 while still passing R19. **Scheduling R22 means scheduling R20 with it** — the pacing is what makes the choreography meaningful.
2. **The cheap half should be done regardless.** The bench deadline-scheduling fix and the `mono_ms` field in `[TX]` are ~1 hour on code that already runs live, and they close K5 immediately. The detector pacing is ~2 hours *inside* Phase 3 rather than extra to it.
3. **Nothing here pulls a deferred item into M1.** No item from the report's § Future developments or [milestone1_high_level_plan.md §6](../Plan/milestone1_high_level_plan.md) is touched. Note the interaction the other way: if the deferred **IVI dashcam view** is ever accepted ([m1-video-source-and-ivi-dashcam.md §8](m1-video-source-and-ivi-dashcam.md)), R20's detector pacing stops being polish and becomes mandatory — that note already flagged it, and R20 is where it now lives.
4. **The `referenceTime` epoch defect (§6.5(b)) is a conformance fix awaiting the user's word** on whether to spend an hour on a field that changes no M1 behaviour.
5. **R22 rescales the ADA risk bands to `RISK_NEAR_M` 60 and `RISK_CRITICAL_M` 30.** That is what puts the first R4 on a plain range comparison instead of on a closing rate derived from the estimated `d_AB` (§6.6(e)). The gate clause of the ordering rule `RISK_CRITICAL_M < RISK_NEAR_M < GATE_ENTER_M` is withdrawn: no assertion relates a risk threshold to a gate threshold, because they are thresholds on different quantities. `RISK_CRITICAL_M < RISK_NEAR_M` and `RISK_TTC_CRITICAL_S < RISK_TTC_WARN_S` stand. The clause is carried by the phase-2 subtask's validation list and by ADA decision D5's prose, so it is those two documents that drop it.
6. **R22 compresses the risk progression to two states.** The warning window is the ~1.3 s between the first R4 and the cycle end, so at most one further transition fits inside it. A run showing `low → medium → high` needs either a longer cycle — which breaks K1 (§6.6(f), G-C) — or bands tuned to fire twice inside 1.2 s, which is a flicker rather than a progression. The recommended pair shows a clean `low → medium`; the 60/50 pair shows `high` from the first R4. The choice is aesthetic and belongs to the user.
7. **The landed clip does not show a lane merge at 3 s.** R22 maps the choreography onto the clip as landed (§6.6(i), V-1) at zero cost. Re-cutting for a literal merge is ~half a day and rests on an unverified reading of the raw source; re-sourcing is not recommended six days out.
8. **Changing `default.yaml` invalidates three assertions in `Scenario_Player/tests/test_streams_differ.py`** — the `60.0 − 2.5 t` kinematics check, the `−250` velocity check, and the module docstring's 20 s / 60 m grid. Those are node tests and belong to [[project-planner]]'s decomposition, not to this report.

## 9. Open items — what could not be verified, and the check that would settle it

| # | Unverified | Check |
|---|---|---|
| 1 | Whether the Room's pods are co-scheduled on one host — CarSky documents namespace and resource pool only, never affinity | Re-run §6.3's `[CAP]` correlation on each redeploy; the design does not depend on the answer |
| 2 | Bench→V2X and ADA-internal latency — only the V2X in→out 142–151 µs is measured | §6.3, on the next live Room |
| 3 | ADA→IVI hop latency into the AAOS guest | Needs a guest-side stamp; declared out of budget |
| 4 | Detector warm-up `W` (ONNX load + `VideoCapture` open), estimated 2–5 s — it is the value `start_delay_s` is set to, and R22's alignment budget is −0.5 / +1.1 s around it | First Phase 3 run on the deployed node: the interval from detector spawn to its first emitted R3 line |
| 5 | AAOS boot-to-listener time — R22 requires the app listening before `T0` + 8.0 s | Phase 5 produces it; Phase 0 could not (ADB returns 502, residual O4) |
| 6 | Ethernet Bridge MTU and jitter — the bridge is a tunnelled fabric on TCP 29400, and no platform figure exists | Smoke-test residual O3's `PAD=1400` bisect |
| 7 | Whether the host's own NTP daemon steps the shared wall clock during a run | Unobservable from inside; §6.2's monotonic expiry stamp removes the exposure rather than measuring it |
| 8 | B's estimated range series across the clip, and whether any frame shows a vehicle entering the ego lane — the sidecar's inspection sampled 20 of 200 frames at 2 fps and recorded B present in all of them | Extract frames at 5 fps and run the detector over the whole clip; the range-estimate retune produces the series, and with it the instant B's estimate crosses 30 m |
| 9 | The margin between `WARNING_TIMEOUT_MS` and the longest R4 gap in a looping run — the cycle-wrap `low` at ≈ 10.4 s to the next cycle's `medium` at 18.5–18.9 s is an 8.1–8.5 s gap, leaving ≥ 1.5 s before the countdown would dismiss the warning | Measure the largest inter-`r4_tx` gap on the recorded run; raise `WARNING_TIMEOUT_MS` if it exceeds 10 s |
| 10 | The skew `σ` between the bench node's and the ADA node's process start in one deployment, which adds to `start_delay_s` | Compare each node's first `[EVT] ready` line on a deployed Room |

## Sources

- [m1-cooperative-awareness.md](m1-cooperative-awareness.md) — R1–R6 contracts, R11–R19; §4 decision record.
- [milestone1_high_level_plan.md](../Plan/milestone1_high_level_plan.md) — §2 assumptions, §4 track-admission gate, Phases 0–6 acceptance, §6 deferred scope.
- [phase1-comms-run.md](../../plans/doc/phase1-comms-run.md) — the live R2 excerpt with `rxTime` against the ADA sink's own log clock (§2(a)); the 142–151 µs V2X in→out measurement; the `[CAP]` In/Out/P reading rules.
- [phase0-smoke-test-run.md](../../plans/doc/phase0-smoke-test-run.md) · [baseline-connectivity-smoke-test.md](../../plans/doc/research_notes/baseline-connectivity-smoke-test.md) · [netcheck.py](../../tools/netcheck/netcheck.py) — the smoke test is a one-way relay chain, not a handshake; `START_DELAY_S` is its only readiness mechanism; hop 3 unconfirmed (O4).
- [phase2-4-pr3-review.md](../../plans/doc/phase2-4-pr3-review.md) — the ADA starting state: no service loop, detector is a placeholder, R3 timestamps swapped (M1).
- [scenario-player-hld.md](../Design/SCENARIO-PLAYER/scenario-player-hld.md) · [generator.py](../../Scenario_Player/player/generator.py) · [config.py](../../Scenario_Player/player/config.py) · [default.yaml](../../Scenario_Player/scenarios/default.yaml) — the tick-counter scenario clock, the fixed-sleep loop, the YAML key set, the persistent `cpm_encode --stream` codec path (D1).
- [ada-ecu-hld.md](../Design/ADA-ECU/ada-ecu-hld.md) and its [decisions](../Design/ADA-ECU/ada-ecu-design-decisions.md) — `TRACK_TIMEOUT_MS` as silence and `DETECTOR_FRAME_STRIDE` as decimation rather than a rate (the unstated clock source that §6.2 settles); D3's one admission machine for both sources with `CONFIRM_HITS` and the `GATE_ENTER_M`/`GATE_EXIT_M` Schmitt band; D5's risk bands on the composed range `d_AC`, its `RISK_DWELL_MS` debounce, its edge-triggered emission in both directions and its `b_unknown` path; D6's frame-source seam and range estimator; the env table's committed threshold values.
- [ego-b-occluding-c.source.md](../../ADA_ECU/media/ego-b-occluding-c.source.md) — the clip's provenance, its 10.0 s / 20 fps / 200-frame encode, the 2 fps content inspection recording B present and frontmost in 20 of 20 sampled frames, B's ~60 m → ~10 m approach, and the raw source's "not yet the lead vehicle" window before `t ≈ 6 s`.
- [video-source-for-r12.md](../KnowledgeBase/video-source-for-r12.md) — the clip baked into the image with no external arming path; `VIDEO_CLIP_PATH` and `DETECTOR_FRAME_STRIDE`; the 5 Hz sample-rate budget.
- [ivi-ecu-hld.md](../Design/IVI-ECU/ivi-ecu-hld.md) and its [decisions](../Design/IVI-ECU/ivi-ecu-design-decisions.md) · `R4Message.kt` · `build.gradle.kts` — R4 carries no timestamp; `DisplayMode` Home/Apps/Settings/Warning and the `[UI]` line with its `cause`; `WARNING_TIMEOUT_MS` = 10000 as a local countdown, with active risk raising the warning and only silence lowering it; R3 timestamps parsed and unused.
- [r1-cpm-profile.md](../../contracts/r1-cpm-profile.md) and the R1–R4 schemas under [contracts/](../../contracts/) — `TimestampIts` epoch definition, F1/F8/F9, the exact frozen time fields, and the R4 warning event's five required fields with its nullable `geometry.vehicleC`.
- [Car-Sky-Platform.html](../../requirements/development-platform-doc/Car-Sky-Platform.html) — Room as a K8s namespace; Device fixes namespace and resource pool only; Skycraft as a full guest VM; Ethernet Bridge as an unmanaged software switch on TCP 29400; no time service, no scheduler, no readiness probe, no ordering primitive.
- [BTC_phan_hoi_V2X_team.pdf](../../requirements/development-platform-doc/BTC_phan_hoi_V2X_team.pdf) — §3, the ADA ECU as an external node on a separate server, with the bench required to follow it.
- [m1-video-source-and-ivi-dashcam.md](m1-video-source-and-ivi-dashcam.md) — the deferred IVI dashcam view, which makes detector real-time pacing mandatory if accepted.
