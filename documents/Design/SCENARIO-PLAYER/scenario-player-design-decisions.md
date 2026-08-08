# Scenario Player — design decisions

> The decision record for [scenario-player-hld.md](scenario-player-hld.md), which cites these by number. Binding on implementation: a decision is revisited by changing its entry here, never by an implementation that departs from it.

## D1 — Python reaches the R1 encoder through a `cpm_encode` helper subprocess

A small C++ CLI built from the same Vanetza-based codec seam sources as the V2X ECU, invoked by Python as a **persistent** subprocess: one `CpmContent` JSON per stdin line, one base64 UPER payload per stdout line. An encode failure returns an `{"error": …}` line and never kills the stream.

Against [solution-selection-criteria](../../../.claude/rules/solution-selection-criteria.md): **C1** — it reuses the frozen codec, so the bytes are byte-verifiable against the golden vectors, and the subprocess-over-stdio pattern is already sanctioned in this repo (the R12 detector boundary); **C2** — one CMake target and a small CLI, no new toolchain; **C4** — nothing new pulled in.

| Rejected alternative | Why |
|---|---|
| pybind11 binding | A new toolchain layer and cross-compile friction inside the image build — fails C2 |
| Pre-encoded vectors with byte patching | Drifts from the codec, and cannot satisfy R11's different-configs-different-streams acceptance — fails C1 |

Encoding stays behind the R1 codec seam, so a future in-process encoder (R10, deferred) is a different caller of the same `ICpmCodec::encode`, not a contract change.

## D2 — One codec source, two build contexts, joined by the sync manifest

Node folders are self-contained build contexts with no cross-folder reads, so the codec seam reaches this folder the way schemas do — **byte-synced copies** under `contracts/sync-manifest.json`, gated by `contracts/check_sync.py`.

| Master (normative home) | Synced copy here |
|---|---|
| `V2X_ECU/src/codec/cpm_codec.hpp` · `vanetza_cpm_codec.{hpp,cpp}` | `codec_helper/src/codec/` |
| `contracts/vanetza-pin.cmake` — the Vanetza git tag and the ASN.1-only target list | `V2X_ECU/cmake/` · `codec_helper/cmake/` |
| `contracts/golden-vectors/*` | `tests/fixtures/golden/`, both `.json` and `.uper` |

Drift is caught twice: by byte identity in `check_sync.py`, and by wire truth in `test_encoder_golden.py`, which asserts `cpm_encode(golden .json) == golden .uper`. **A synced copy is never edited in place** — the change is made in the master and re-synced.

## D3 — Scenarios are declarative YAML; kinematics is one model, not code branches

`player/scenario.py` implements a single constant-velocity model in B's frame and nothing else: B holds a static WGS84 pose and heading, C starts at `initial_distance_m` and closes along x at `closing_speed_mps` with a fixed `lateral_offset_m`. Different behaviour comes from different data.

- **Scenario variants are new files**, selected at runtime by `SCENARIO_CONFIG`. A variant that needs a code branch is a design change, not a scenario.
- **Every scenario tunable lives in the YAML**, validated by `player/config.py`; nothing about the content is an env var or a literal.
- The two committed variants are chosen to drive the R13 lifecycle from the consumer's side: `default.yaml` closes C from 70 m to 20.5 m across its cycle, crossing the 30 m admission gate 8.0 s in (D7), and `c-out-of-range.yaml` holds C static at 60 m, beyond the 35 m exit gate, so no track is ever admitted.

The scenario values pair with the R13 gate constants that [milestone1_high_level_plan.md §4](../../Plan%20and%20Proposal/milestone1_high_level_plan.md#track-admission-gate-r13) fixes. The pairing is a property of the data, so a gate change is answered by editing the YAML, never by editing the model.

## D4 — Runtime composition, and one base image for both build stages

`main.py` is the composition root and the blueprint-fixed entrypoint (`command: ["python", "main.py"]`): load env and YAML → spawn `cpm_encode --stream` → run the rate loop → `[TX]` per datagram. The collaborators are injected, so every one of them is replaceable in a test without touching the loop.

**Failure is graded, because the bench is test equipment that must stay alive and observable:**

| Failure | Response |
|---|---|
| Invalid configuration | one `[FATAL]` line, exit 1 — a broken node config must fail loudly at start |
| One un-encodable sample | one `[ENC-SKIP]` line, the tick is skipped, the loop continues |
| Helper subprocess death | one `[ENC]` line, restart with backoff |
| Transient send error | one `[SND-ERR]` line, the loop continues |

**Both Docker stages resolve the same base image**, `python:3.11-slim`, through one `ARG`. `cpm_encode` therefore links against exactly the glibc and libstdc++ it runs on, and a stage-1-to-stage-2 symbol skew is impossible by construction rather than something a runtime check has to catch. CMake comes from a pip wheel because this base's apt CMake is below the 3.28 floor the helper needs. The image is single-platform `linux/arm64` with attestations disabled — a Container Node rejects a manifest index and hangs in `Provisioning`.

## D5 — The scenario clock is deadline-scheduled, and every offset is configuration

Scenario time advances at 1.0× wall time, scheduled against `CLOCK_MONOTONIC` deadlines. This is the bench half of R20 ([m1-run-timing-and-event-triggering.md §6.1](../../../requirements/m1-run-timing-and-event-triggering.md)).

- **The deadline is computed, not accumulated.** Tick *n* is due at `t0 + n × period` on `time.monotonic()`; the loop sleeps until that instant. A fixed `sleep(period)` per tick accumulates the per-tick work cost into scenario time, which drifts unbounded over a run.
- **`[TX]` carries `mono_ms`**, so `scenario_time_s` can be regressed against elapsed time — R20's K5 check, ±1 % over ≥ 60 s.
- **`start_delay_s`** is a grace period from process start before the first CPM. The operator restarting this node *is* the run start (R21) — there is no orchestrator, no trigger message and no reverse channel to wait on — so a configured delay is the only lever that aligns this node's scenario time with the ADA detector's clip time (D7).
- **`reference_time_epoch`** names the epoch `referenceTime` is stamped against. The frozen profile defines it as `TimestampIts` — milliseconds since 2004-01-01T00:00:00.000 TAI — so a Unix-epoch stamp is non-conformant even though it passes the schema's upper bound. The epoch is a config value, never a literal in the loop.
- **`loop: true` repeats the choreography, and corrects nothing.** A fresh admission cycle every `duration_s` means a recording that starts late still captures a complete cycle. With the bench cycle and the clip loop at the same period, an offset between them is constant for the whole run — which is why `start_delay_s` is a measured value rather than an approximation (D7).

## D6 — Standing decisions binding on this design

- **The wire is unidirectional.** Bench → V2X ECU only: no listener, no reply, no acknowledgement, no handshake, and no in-band authentication. Production Rx is already "read from socket", so adding a handshake would lower fidelity rather than raise it.
- **CPM is the only V2X message family in M1.** DENM is the named family for future hazard types and is not encoded here.
- **The bench simulates B and C; the ego builds nothing of them.** Every value describing either vehicle originates in a scenario file on this node.
- **No signing and no PKI.** The R1 profile carries no security envelope, and the V2X protocol stack ships in the modem — out of scope for the whole project, not only M1.
- **R10 ego Tx is deferred**, so nothing on this node ever receives. The encoder returns for R10 as a second caller of `ICpmCodec::encode`, with no change here.
- **No capture on this node.** The V2X ECU's interface sees both live flows, which makes it the single capture point.

## D7 — The demo cycle is one clip length, and its geometry is solved backwards from the first warning

Realizes R22 ([m1-run-timing-and-event-triggering.md §6.6](../../../requirements/m1-run-timing-and-event-triggering.md)) on the bench side. Every lever is scenario data in `scenarios/default.yaml`, so the choreography is a file, never a code branch (D3).

| Key | What it is bound to |
|---|---|
| `duration_s` | the ego clip's length, exactly |
| `object.initial_distance_m` | `GATE_ENTER_M + closing_speed_mps × 8.0 s`, which places C's gate crossing 8.0 s into the cycle |
| `object.closing_speed_mps` | a rate that keeps ego-to-C and ego-to-B closing at comparable speeds, and `initial_distance_m` inside a plausible CPM perceived-object range |
| `cpm_rate_hz` | the R1 profiled rate; three ticks are what promote C to `tracked` after the crossing |
| `loop` | `true`, so the cycle repeats without operator action |
| `start_delay_s` | the measured ADA detector warm-up `W` |

- **`duration_s` is bound to the clip, not chosen for the bench.** Matched periods are what keep B and C admitted and dropped inside one window. A longer bench cycle leaves C tracked across a clip wrap at which B is absent, and the ADA assessment falls to `low` on its `b_unknown` path mid-run ([ADA D11](../ADA-ECU/ada-ecu-design-decisions.md)). Retuning either period alone breaks the run.
- **`start_delay_s` is a measurement, not a guess.** Cancelling `W` is what makes bench scenario time equal clip time. The value holds to **−0.5 / +1.1 s** around the true `W` before the first warning leaves R22's window. `W` is read on the deployed ADA node as the interval from detector spawn to its first emitted R3 line.
- **The gate crossing is placed at 8.0 s of the cycle**, one second above R22's floor and, once the ADA node's confirm count, risk dwell, fusion tick and pipeline latency are added, at worst 1.1 s below its ceiling.

| Rejected alternative | Why |
|---|---|
| A cycle longer than the clip, with the crossing placed inside it | Breaks the matched-period constraint above |
| Leaving the geometry alone and shifting the cycle with `start_delay_s` | No offset places the resulting first warning in the required interval; the offset moves the whole cycle, crossing included |
| A closing speed high enough to trigger the ADA node's TTC threshold instead of its range threshold | Needs a B-to-C closing rate of ~47 km/h, and makes the trigger depend on a differentiated range estimate rather than one distance comparison |
