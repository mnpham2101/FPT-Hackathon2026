# ADA ECU + IVI ECU — Consolidated Implementation Plan (Phases 3, 4, 5)

> **What this file is.** The single execution view over the two nodes the user asked to be planned together — [ADA_ECU/](../ADA_ECU/) (R3, R12–R15) and [IVI_ECU/](../IVI_ECU/) (R4, R16–R17) — after the requirement update of 2026-08-02.
>
> **What it is not.** It does not restate the 89 subtask briefs that already exist. Those live in [phase3_tasks.md](phase3_tasks.md), [phase4_tasks.md](phase4_tasks.md) and [phase5_minh_tasks.md](phase5_minh_tasks.md) and stay the authority for their own phases; this file indexes them, states the cross-node execution order, and carries the decisions the update forced. Phase *content* — objectives and acceptance boxes — remains [milestone1.md](milestone1.md)'s.
>
> **Why a new file rather than an extension of [milestone1.md](milestone1.md).** `milestone1.md` is milestone-scoped and node-agnostic: it carries seven phases across four nodes, and its per-phase sections are deliberately short because the decomposition lives in the `phase<N>_tasks.md` files. A two-node cross-phase execution view has no slot in that structure, and inlining one would make the phase sections asymmetric — Phases 3–5 fat, Phases 0–2 and 6 thin. `milestone1.md` instead received two surgical edits (Phase 3's clip-input bullet, and an explicit deferred-scope line for the IVI dashcam view), and the new subtask briefs went where subtask briefs go, in [phase3_tasks.md](phase3_tasks.md) group 3.7.

**Authority order:** [m1-cooperative-awareness.md](../requirements/m1-cooperative-awareness.md) (R1–R19, §4 decisions, § Future developments) → [milestone1.md](milestone1.md) → the per-phase task files → this file. On conflict, the higher one wins.

**The update this plan absorbs:** [m1-video-source-and-ivi-dashcam.md](../requirements/m1-video-source-and-ivi-dashcam.md) (commit `a7808c8`, 2026-08-02) — a researcher environment study, **no new requirement numbers**. It settles how the R12 video reaches a deployed container (§5), what the blueprint needs (§6), and whether the feed can fan out to the IVI (§4, §8).

## 1. What the update changed, and what it did not

| # | The update's finding | Effect on this plan |
|---|---|---|
| 1 | **The clip is baked into the ADA image** — `COPY media/ /app/media/` as its own early layer, read via `VIDEO_CLIP_PATH`; no volume, no bind mount, no `video` pin, no runtime upload (§4 answer 4, §5, §9) | Confirms the path already planned. New: the layer must be **ordered early** and its size/cache economics measured — [`12.3.7.2`](phase3_tasks.md), [`5.3.7.3`](phase3_tasks.md) |
| 2 | **CarSky serves no video content** — the `video` pin is a transport with nothing behind it; the Videos library is a sink with no upload API (§1 answer 1, §2) | Unchanged from [video-source-for-r12.md](../ADA_ECU/doc/research_notes/video-source-for-r12.md). No blueprint change; the ADA node gains **two env vars and nothing else** |
| 3 | **The clip must be supplied by the team** — no platform footage exists, and BTC names none (§2, BTC §6(3)) | **Changed by the user's direction:** sourcing and post-production become **agent work** — new task group 3.7. `12.2.9.3` (human task) stays open as the preferred source but no longer gates Phase 3 |
| 4 | **One feed cannot usefully fan out to both nodes** — the `video` pin has four independent blockers; UDP video means writing RTP against an unmeasured MTU (§4) | Moot: the IVI view is deferred (row 5). Recorded so it is not re-litigated |
| 5 | **The IVI dashcam view is deferred scope** and pulling it in is the user's decision (§8) | **The user's decision, 2026-08-02: it stays deferred.** See §5 below — this is a hard exclusion, not a "later in the phase" |
| 6 | Real-time detector pacing "stops being optional **if** the dashcam view is built" (§4, §7) | With the view excluded, R20's detector half stays optional and unplanned ([phase3_tasks.md open item 7](phase3_tasks.md#open-items--flags-no-phase-3-subtask-may-silently-close-them)) |
| 7 | New measurable outputs: KPI 7 (media layer ≤ 60 MB, digest stable across builds), KPI 8 (second push transfers 0 bytes), KPI 9 (the baked-in clip opens **on the deployed node**) | KPIs 7–8 → [`5.3.7.3`](phase3_tasks.md); KPI 9 → [`5.3.6.2`](phase3_tasks.md) |
| 8 | The `container-file` API is a real post-deploy file channel, and is **not** the deploy path (§5) | Sanctioned only as a rehearsal-time clip-swap trick. **No subtask uses it**; recorded in §6 flags |

**Nothing in the update touches a frozen contract.** R1–R6 stand as frozen in Phase 0; the ADA node's pin set is unchanged (one `ethernet` OUTPUT at `10.99.0.12`), the IVI's likewise (`10.99.0.13`), and the topology stays the four-node star of [carsky-4-node-blueprint.md](../requirements/car-sky-guide/carsky-4-node-blueprint.md).

## 2. The two nodes, and the one thing they share

| | ADA ECU | IVI ECU |
|---|---|---|
| Folder | [ADA_ECU/](../ADA_ECU/) | [IVI_ECU/](../IVI_ECU/) |
| CarSky node | Container Node, `10.99.0.12` | Skycraft Node (AAOS guest), `10.99.0.13` |
| Requirements | R3, R12–R15 (+ R5, R6, R18) | R4, R16–R17 (+ R5, R6, R18) |
| Phases | **3** (detection) and **4** (fusion + warning), which run in parallel with each other | **5** (HMI), which runs in parallel with 3 and 4 |
| Build + test | `cmake -S ADA_ECU -B ADA_ECU/build && cmake --build ADA_ECU/build -j $(nproc) && ctest --test-dir ADA_ECU/build --output-on-failure` · `pip install -r ADA_ECU/detector/requirements.txt -r ADA_ECU/detector/requirements-dev.txt && python -m pytest ADA_ECU/detector/tests` | `./gradlew :app:assembleDebug` · `./gradlew test` (all modules) |
| Image / artifact | `docker buildx build --platform linux/arm64 --provenance=false --sbom=false -t m1-ada-ecu:latest ADA_ECU/` → `registry.hackathon-2.carsky.io/m1-ada-ecu:latest` | APK via Gradle, installed post-deploy over ADB ([node-ivi-ecu.md](../requirements/car-sky-guide/node-ivi-ecu.md)) |
| Deploy guide | [node-ada-ecu.md](../requirements/car-sky-guide/node-ada-ecu.md) | [node-ivi-ecu.md](../requirements/car-sky-guide/node-ivi-ecu.md) |

**They share exactly one thing: the frozen R4 contract** (`contracts/r4-ada-ivi.schema.json`, frozen in Phase 0). ADA is the sender, IVI the receiver, UDP `10.99.0.13:47300`. Neither reads the other's source; no cross-node import exists or may be introduced ([node-code-layout.md](../.claude/rules/node-code-layout.md)).

**Therefore the two nodes are fully parallel from day one and stay parallel until Phase 6.** That is not a scheduling convenience — it is the direct consequence of the contract-first principle, and it is why Phase 5 develops against its own `:r4-simulator` rather than waiting for ADA output. The only real coupling is an **open cross-phase defect**, not a dependency: [phase4_tasks.md open item 4](phase4_tasks.md) records that the unmerged Phase 5 branch's `R4WarningMessage.kt` cannot decode Phase 4's output, and that `main`'s `R4Message.kt` is the binding. Phase 5 group 5.1 fixes it on the IVI side; Phase 4 changes nothing for it.

## 3. Phase-by-phase — input, output, tasks, branch

### Phase 3 — ADA: object detection from video (R12)

**Full decomposition:** [phase3_tasks.md](phase3_tasks.md) — **7 task groups, 23 subtasks** (21 agent, 1 [[car-sky]], 1 user-manual, 1 superseded). Design of record: [phase2-4-ada-ecu-hld.md D6](../ADA_ECU/doc/phase2-4-ada-ecu-hld.md).

**Input.** Phase 2 complete (the C++ core, the R3 store, the R13 machine, `src/observer/detector_reader` and its `DETECTOR_CMD` + stdout-JSONL process contract, the ADA image and its CI lane, `tools/check_clip_spec.py`); Phase 0's frozen `detector/contracts/tracked_object.py`. **No longer required as an input: a user-supplied clip** — group 3.7 produces it.

**Output (the four [milestone1.md](milestone1.md#phase-3--object-detection-from-video-r12--runs--with-phase-4) boxes).**

- Detection log over the provided clip with per-frame objects and distance estimates (R12) — `12.3.5.2`.
- Entries enter the store via the same R3 interface as relayed entries, `source = own_sensor`, mock retired — `3.3.5.3`.
- Zero detections labelled C — `12.3.5.1` + `12.3.5.4` + D6's structural argument + the clip content rows established at `12.3.7.1` and **re-established on the encoded file** at `12.3.7.2`.
- CPU-only, offline pace acceptable, effective inference ≥ 5 Hz — `12.3.5.2` (host) + `5.3.6.2` (deployed node).

Plus, new from the update and carrying no milestone box: media layer ≤ 60 MB with a stable digest and a 0-byte second push (`5.3.7.3`), and the baked-in clip proven to open on the deployed node (`5.3.6.2`).

**Task groups and IDs.**

| Group | Subject | IDs |
|---|---|---|
| 3.1 | arm64 dependency de-risking | `12.3.1.1` |
| 3.2 | Detector modules | `12.3.2.1` … `12.3.2.7` |
| 3.3 | Model, CI fixture, detector lane | `12.3.3.1` · `12.3.3.2` · `12.3.3.3` |
| 3.4 | Clip intake and calibration | `12.3.4.1` · ~~`12.3.4.2`~~ **superseded by `12.3.7.2`** · `12.3.4.3` |
| 3.5 | Zero-C evidence and store integration | `12.3.5.1` · `12.3.5.2` · `3.3.5.3` · `12.3.5.4` |
| 3.6 | **Image and deployment** | `5.3.6.1` · `5.3.6.2` |
| **3.7** | **Demo clip: sourcing, licence, post-production, delivery — new 2026-08-02** | `12.3.7.1` · `12.3.7.2` · `5.3.7.3` |

**Suggested branch (suggestion only — creation, checkout and push are the user's call):** `feat/phase3-ada-detector`.

### Phase 4 — ADA: obscured-object fusion, risk, warning (R13–R15)

**Full decomposition:** [phase4_tasks.md](phase4_tasks.md) — **11 task groups, 46 subtasks** (24 agent incl. 1 optional, 7 [[car-sky]], 11 user-manual, 4 of the agent set gated on user ratification). Groups 4.9–4.11 were added 2026-08-03 as stage 2 of [walkthrough-driven-delivery.md](../.claude/rules/walkthrough-driven-delivery.md) over [deploy-ada-ecu-walkthrough.md](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md); the 2026-08-02 video/dashcam update touched none of this phase.

**Input.** Phase 2 complete, with the CRA seam frozen; a live or mocked R2 source (`tools/mock_v2x_sender.py`, or the deployed Phase 1 chain for live evidence). **Phase 3 is not an input** — the detector's own-sensor B track is interchangeable with the Phase 2 fixture everywhere except the live deployed runs in groups 4.10–4.11 and 4.6, which need the real detector because half of § Output rests on a `tracked` `own_sensor` B.

**Two Rooms, and the isolated one runs first.** [Walkthrough §1.4](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#14-blueprints) defines an isolated blueprint — Ethernet Bridge + a V2X bench mock at `10.99.0.11` + the ADA node at `.12` + an IVI sink mock at `.13`, both mocks from one image `m1-ada-bench:latest` selected by `ROLE` — beside the full 5-node one. **Groups 4.9 → 4.10 → 4.11 run the isolated Room and depend on nothing outside `ADA_ECU/` and `tools/ada-bench/`**: no V2X ECU, no Scenario Player, no IVI app. Group 4.6 is the full-chain re-run of [§5.6](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#56-the-full-blueprint-route), where the ADA node's own config is unchanged and only the neighbours differ. **For this cross-node plan the consequence is the one that matters: ADA's deployed evidence no longer waits on Phase 5's app, and Phase 5's `5.5.9.1` still does not wait on the real ADA node.** The two nodes stay fully parallel through their in-Room evidence, not only through development.

**Output.** The six [milestone1.md](milestone1.md#phase-4--obscured-object-fusion-relayed-c--risk--warning-r13r15--runs--with-phase-3) boxes, whose sharp form is [phase4_tasks.md § Phase 4 output acceptance](phase4_tasks.md#phase-4-output-acceptance--what-b-and-c-reach-the-ivi-means-precisely): with a scenario live, the ADA `[EVT]` log carries a `tracked` `own_sensor` B and a `tracked` `v2x_relayed` C with full R3 fields plus an `r4_tx`, **and** an ADA→IVI pcap decodes to the same R4 body.

**Task groups and IDs.** 4.1 `15.4.1.1` · `14.4.1.2` · `14.4.1.3` — 4.2 `15.4.2.1` … `15.4.2.4` — 4.3 `18.4.3.1` … `18.4.3.3` — 4.4 `6.4.4.1` · `6.4.4.2` — 4.5 `15.4.5.1` — **4.6 (full-blueprint deploy + live evidence)** `5.4.6.1` · `5.4.6.2` · `13.4.6.3` · `18.4.6.4` · `15.4.6.5` — 4.7 (gated) `4.4.7.1` … `4.4.7.4` — 4.8 (last) `20.4.8.1` · `21.4.8.2` — **4.9 (bench image, CI lanes, blueprint reference)** `5.4.9.1` · `2.4.9.2` · `4.4.9.3` · `5.4.9.4` · `5.4.9.5` · `5.4.9.6` · `2.4.9.7` — **4.10 (create and deploy the isolated Room)** `5.4.10.1` … `5.4.10.8` — **4.11 (the three checks, negative case, teardown)** `18.4.11.1` · `2.4.11.2` · `13.4.11.3` · `15.4.11.4` · `13.4.11.5` · `14.4.11.6` · `5.4.11.7`.

**Suggested branch:** `feat/phase4-ada-fusion-warning` — one branch for the phase, per [task-planning-conventions.md § Branch suggestion](../.claude/rules/task-planning-conventions.md#branch-suggestion-per-phase); creation, checkout and push stay the user's call. Groups 4.9–4.11 write only under `tools/ada-bench/`, `.github/workflows/`, `requirements/car-sky-guide/` and `plans/doc/`, so `feat/phase4-ada-isolated-room` is a conflict-free alternative if the user wants the bring-up lane on its own branch.

### Phase 5 — IVI: HMI and warning view (R4, R16, R17)

**Full decomposition:** [phase5_minh_tasks.md](phase5_minh_tasks.md) — 9 task groups, 45 subtasks (39 agent, 6 user-manual), nothing started. Design of record: [phase5-ivi-hld.md](../IVI_ECU/doc/phase5-ivi-hld.md) (commit `85387b5`). **Unaffected by the update** — see §5 for why, which is the point.

> **Which Phase 5 file is authoritative.** Two independent breakdowns exist: [phase5_tasks.md](phase5_tasks.md) (2026-07-24) and [phase5_minh_tasks.md](phase5_minh_tasks.md) (2026-08-02). **`phase5_minh_tasks.md` is the one to execute** — it was written from the HLD, gap-checked against the older file (four gaps closed, and everything deliberately dropped is listed), and it corrects two facts the older file has wrong: the R4 UDP port is **47300, not 5004**, and the IVI address is **10.99.0.13, not 10.88.0.12**. The two files also **collide on five IDs** (`4.5.1.1`–`4.5.1.5` mean different subtasks in each). `phase5_tasks.md` now carries a superseded banner; no ID in either file was renumbered.

**Input** (all present as of 2026-08-02). R4 frozen in Phase 0 — `contracts/r4-ada-ivi.schema.json`, four samples, the committed Kotlin binding and its round-trip/additive-version tests; the Phase 5 HLD and its research notes; `IVI_ECU/` as a single-module Gradle project on AGP 8.13 / Kotlin 2.2.20 / `minSdk 29`; CarSky access with the baseline blueprint, the `AAOS` artifact, and the Zot API key secret.

**Output (the five [milestone1.md](milestone1.md#phase-5--ivi-hmi-mock-driven-r16-r17--display-track-parallel-from-the-start) boxes).**

- The HMI runs on the AAOS node with the R16 layout; button/app areas switch what the Display area shows.
- **(Dev)** A mock R4 warning brings the warning view up showing ego, B and ghost C at the composed positions.
- Ghost C renders from `v2x_relayed` data only; the 2D drawing is delivered (3D stays optional).
- A newer message with an unknown `warningType` degrades gracefully.
- Optional paths only if built: wake-on-warning across processes; 3D through the view seam.

**Task groups and IDs.** 5.1 `4.5.1.1`–`4.5.1.5` — 5.2 `4.5.2.1`–`4.5.2.3` — 5.3 `4.5.3.1`–`4.5.3.5` — 5.4 `4.5.4.1`–`4.5.4.3` · `17.5.4.4` · `16.5.4.5` — 5.5 `18.5.5.1` · `4.5.5.2` · `4.5.5.3` · `16.5.5.4` · `16.5.5.5` · `17.5.5.6`–`17.5.5.9` — 5.6 `4.5.6.1`–`4.5.6.5` · `5.5.6.6` · `4.5.6.7` — 5.7 `16.5.7.1` · `4.5.7.2` · `5.5.7.3` — **5.8 (retire the deployment unknowns)** `5.5.8.2` · `16.5.8.3` · `16.5.8.4` — **5.9 (in-Room R4 evidence run)** `5.5.9.1` · `16.5.9.2` · `17.5.9.3` · `4.5.9.4`.

**Suggested branch:** `feat/phase5-ivi-hmi`.

## 4. Execution — what runs in parallel, and what does not

### Across the two nodes

**ADA and IVI run concurrently for the whole of Phases 3–5.** They share only the frozen R4 contract, which is a schema file, not a build dependency — so per [task-planning-conventions.md § Parallel vs. sequential](../.claude/rules/task-planning-conventions.md#parallel-vs-sequential-execution) they default to parallel and nothing here overrides that. Concretely:

- **No file is written by both nodes' subtasks.** ADA subtasks write under `ADA_ECU/` (plus `.github/workflows/phase3-ci.yml`, `phase4-ci.yml` and `plans/doc/`); IVI subtasks write under `IVI_ECU/` (plus `phase5-ci.yml` and `requirements/car-sky-guide/node-ivi-ecu.md`). One ADA subtask, `20.4.8.1`, writes in `Scenario_Player/` and is flagged as such in its own file.
- **No IVI subtask waits on an ADA subtask**, and none waits on real ADA data — Phase 5 builds its own `:r4-simulator` (group 5.6) as its stimulus, and its in-Room evidence run (`5.5.9.1`) *replaces* the ADA node with the simulator image rather than waiting for the real one.
- **No ADA subtask waits on an IVI subtask.** Phase 4's R4 emission is verified against `tools/mock_ivi_receiver.py` (`18.4.3.1`) and a pcap, never against the real app.
- **Three shared platform constraints** are the real contention, and they are scheduling constraints rather than dependencies: the **2-concurrent-Room deployment quota** — now contended **four** ways, since Phase 4's isolated Room (group 4.10) joins Phase 3's `5.3.6.2`, Phase 4's full-chain group 4.6 and Phase 5's groups 5.8/5.9; serialize the *deploys*, not the development, and take `5.3.6.2`'s reading off group 4.10's Room rather than booking a slot for it — the single registry namespace, which now carries `m1-ada-bench:latest` alongside `m1-ada-ecu`, `m1-v2x-ecu`, `m1-netcheck` and `m1-r4-sim`; and the CI runner, where `phase4-ci.yml` gains two jobs.

### Within ADA

Phase 3 and Phase 4 are parallel with each other. The only shared files are `ADA_ECU/CMakeLists.txt` (Phase 3 adds no C++ target) and `ADA_ECU/Dockerfile` (Phase 3 adds `COPY` lines, Phase 4 adds `capture.sh`) — **sequence those two edits, not the phases**.

Inside Phase 3 after the update, the clip lane is parallel rather than blocking:

```
day one, all at once:   12.3.7.1  (clip sourcing - the only lane with an external unknown)
                        12.3.1.1  (arm64 wheels - the only lane with a platform unknown)
                        12.3.3.3  (CI lane, guarded)
modules (sequential):   12.3.2.1 -> {12.3.2.2 | 12.3.2.3 | 12.3.2.4} -> 12.3.2.5/6 -> 12.3.2.7
clip lane (sequential): 12.3.7.1 -> 12.3.4.1 -> 12.3.7.2 -> 12.3.4.3 -> 12.3.5.2
image/deploy:           5.3.6.1 -> {5.3.7.3 | 5.3.6.2}
```

Only four subtasks sit behind the clip; the other 15 do not. That is the substantive scheduling change the update bought.

### Within IVI

Lane A (`4.5.1.1` → `4.5.1.4`) is strictly sequential and gates lanes B–F; lane I (the in-Room evidence run) is strictly sequential and last. Four independent day-one start points: `4.5.1.1` (code), `5.5.8.2` (mini-blueprint, USER), `16.5.7.1` (CI lane), `17.5.5.8` (mapper test). Full graph and the 26-step critical path: [phase5_minh_tasks.md § Execution order & parallelism](phase5_minh_tasks.md#execution-order--parallelism).

**Lane H is the one to start on day one regardless of anything else** — `5.5.8.2` → `16.5.8.3` probes whether ADB reaches the Skycraft guest and what Android version it runs. A negative answer degrades the whole of group 5.9 to emulator evidence, and that is not something to discover in the last two days.

## 5. Deferred and excluded — read before picking up any task

**The ego video clip display on the IVI ("dashcam view") is NOT in scope and no subtask in this plan may implement it.** The user confirmed the deferral on 2026-08-02; the report already carries it under § Future developments ("Deferred from M1 for time") and restates it in §4's decision record, [node-ivi-ecu.md](../requirements/car-sky-guide/node-ivi-ecu.md) records it as the reason the IVI wires no `video` pin, and [update §8](../requirements/m1-video-source-and-ivi-dashcam.md) flags that pulling it in would be silently absorbing deferred scope days before the deadline. It appears in no acceptance criterion of Phase 3, 4, 5 or 6.

The exclusion is written out item by item because the update's §4/§6 contain a *worked design* for it (option B4), and a worked design in a document an implementer is told to read is exactly how deferred scope leaks in:

| Excluded | Which would otherwise have appeared as |
|---|---|
| **HTTP clip serving from the ADA node** | `python3 -m http.server` in `ADA_ECU/entrypoint.sh`, with `CLIP_HTTP_ENABLED` / `CLIP_HTTP_PORT` env |
| **An `exposedPorts` entry on the ADA node** | `[{ "name": "clip", "port": 8080, "protocol": "http" }]` in the blueprint node config, and a gateway route |
| **Media3 / ExoPlayer on the IVI** | A Gradle dependency, a `DASHCAM_MEDIA_URI` `buildConfigField`, a player surface |
| **A dashcam `DisplayMode`** | A new mode in `DisplayMode.kt` and a Compose surface behind the warning overlay |
| **A local clip copy in the APK** (fallback B5) | A raw resource and the APK-size cost |
| **A `video` pin anywhere** | A pin plus edge on the ADA and/or IVI node — an R5/R6 re-freeze |
| **Real-time detector pacing as a requirement** | `DETECTOR_REALTIME_PACING`, `DETECTOR_CLIP_FPS`, `DETECTOR_START_DELAY_S` — mandatory only *if* the view is built ([update §4](../requirements/m1-video-source-and-ivi-dashcam.md)); with the view out, R20's detector half stays optional |

If the view is ever accepted, it re-enters as **additive and timeboxed** work started only after Phases 3 and 5 are green ([update §8](../requirements/m1-video-source-and-ivi-dashcam.md)) — or, if the user wants it tracked with its own acceptance criteria, as a new requirement number minted by a [[project-researcher]] run. **An environment-research note may not mint requirement numbers, and neither may this plan.**

Also excluded, and already recorded in their own files: the IVI 3D view and multi-process wake-on-warning (optional, report §4); ADA's periodic awareness state `15.4.2.4` (optional, off by default); R10 ego Tx (deferred to a future milestone); Phase 4 group 4.7 (`trackedObjects` on R4 — gated on user ratification, not started).

## 6. Open items and flags carried by this plan

Items owned inside a phase file are not duplicated here; these are the ones that span the two nodes or need the user.

| # | Item | Owner |
|---|---|---|
| 1 | **Repo size — commit ~40 MB of write-once binaries to plain git** (`models/yolo11n.onnx` ~10 MB, `media/ego-b-occluding-c.mp4` ~30 MB). Both must be in the build context because a Container Node has no volume. Recommendation: **yes, plain git, one final file each; Git LFS rejected** (remote-storage dependency days from the deadline). Needed before `12.3.3.1` and `12.3.7.2` | **user** |
| 2 | **A non-commercially-licensed clip is not an agent's call.** If the only viable footage is CC BY-NC, `12.3.7.1` stops and escalates — NC restricts redistribution of every image the project pushes | **user** |
| 3 | **If no acceptably-licensed clip satisfying the content rows can be found**, `12.3.7.1` reports the three closest candidates and their failing rows. **The synthetic fixture is never substituted** — it forfeits R12's evidence box and, through it, R19 | **user** |
| 4 | **Two Phase 5 task files with five colliding IDs.** `phase5_minh_tasks.md` is authoritative and `phase5_tasks.md` now carries a superseded banner. Recommendation: leave both in place (IDs are never renumbered) and execute only the former | project-planner (done) · user (ack) |
| 5 | **Deployment quota — 2 concurrent Rooms** against **four** groups that each need one (`5.3.6.2`, Phase 4 group 4.10 *(isolated)*, Phase 4 group 4.6 *(full chain)*, Phase 5 groups 5.8/5.9). Serialize the deploys and tear each Room down; `5.4.11.7` and `4.5.9.4` both end with a teardown, and `5.3.6.2` should read off group 4.10's Room instead of booking its own | project-planner (scheduling) |
| 6 | **`container-file` API is not the deploy path.** [Update §5](../requirements/m1-video-source-and-ivi-dashcam.md) documents `POST /api/v1/deployments/:roomId/container-file/:nodeKey` as a real post-deploy file channel with no schema, no size limit and no example. Sanctioned **only** as a rehearsal-time clip swap; no subtask depends on it, and nothing deployed may differ from its image tag | recorded, no action |
| 7 | **No documented registry size ceiling.** [Update §10 item 1](../requirements/m1-video-source-and-ivi-dashcam.md) — a ~1.2 GB artifact is observed succeeding on the platform, so ~30 MB is unremarkable, but the number is unverified. `5.3.7.3` is where a real push either confirms it or fails loudly | `5.3.7.3` |
| 8 | **R4 binding defect across the Phase 4/Phase 5 boundary** — [phase4_tasks.md open item 4](phase4_tasks.md): the unmerged Phase 5 branch's `R4WarningMessage.kt` cannot decode Phase 4's output; `main`'s `R4Message.kt` is the binding. Fixed on the IVI side in Phase 5 group 5.1; **not** an ADA change | Phase 5 group 5.1 |
| 9 | **AAOS reachability is Phase 5's dominant unknown** — whether ADB reaches the Skycraft guest, and the guest's API level against `minSdk 29`. Probed on day one at `16.5.8.3`; a negative answer degrades group 5.9 to emulator evidence | `16.5.8.3`, then user |
| 10 | **`tools/ada-bench/` is a fifth container build context outside the four node folders**, sanctioned by [walkthrough §2.4](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#24-where-the-bench-sources-live-and-why) against [node-code-layout.md](../.claude/rules/node-code-layout.md), which itself names only four and does not yet name `tools/`. [tools/netcheck/](../tools/netcheck/) is the standing precedent. **Requested: one sanctioning paragraph in the rule.** Recorded here because it is a cross-node placement question, not an ADA one | [[project-architecture]] · [phase4_tasks.md open item 9](phase4_tasks.md#open-items--flags-no-phase-4-subtask-may-silently-close-them) |
| 11 | **[node-ada-ecu.md](../requirements/car-sky-guide/node-ada-ecu.md) contradicts the walkthrough on three node facts** — `command`, `capabilities` and the registry host — and [§8.1 item 7](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#81-confirm-before-relying-on-these) requires that resolved **before any ADA deploy**. Phase 2 `5.2.9.4` is the planned fix and is now a hard prerequisite of both `5.3.6.2` and `5.4.10.5`, so it blocks the deployed evidence of **two** phases, not one | [[project-architecture]] · Phase 2 `5.2.9.4` |

## 7. Traceability

Every subtask ID's `X` names the requirement it serves, `Y` the phase, per [task-planning-conventions.md](../.claude/rules/task-planning-conventions.md#task-id-scheme). Across the three phases: **R3** store interface (`3.3.5.3`) · **R4** the whole IVI ingest and contract layer (`4.5.*`) and the gated Phase 4 group 4.7 · **R5** deployment (`5.3.6.*`, `5.3.7.3`, `5.4.6.*`, `5.5.6.6`, `5.5.7.3`, `5.5.8.*`, `5.5.9.1`) · **R6** network and capture (`6.4.4.*`) · **R12** detection and the clip (`12.3.*`) · **R13** admission (`13.4.6.3`) · **R14** CRA (`14.4.*`) · **R15** composition and warning emission (`15.4.*`) · **R16** layout and view switching (`16.5.*`) · **R17** the God view (`17.5.*`) · **R18** evidence (`18.4.3.*`, `18.4.6.4`, `18.5.5.1`) · **R20/R21** run alignment (`20.4.8.1`, `21.4.8.2`, scheduled last and gating no box).

---

*Updated 2026-08-03 by project-planner after stage 2 of [walkthrough-driven-delivery.md](../.claude/rules/walkthrough-driven-delivery.md) over [deploy-ada-ecu-walkthrough.md](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md). Phase 4 gains groups 4.9–4.11 (22 subtasks, isolated-Room bring-up) and its group 4.6 is corrected to the full-blueprint route; Phase 3's `5.3.6.2` names the same isolated Room. Two new cross-node open items (10, 11). No ID renumbered.*

*Created 2026-08-02 by project-planner from [m1-cooperative-awareness.md](../requirements/m1-cooperative-awareness.md), [m1-video-source-and-ivi-dashcam.md](../requirements/m1-video-source-and-ivi-dashcam.md) (`a7808c8`), [milestone1.md](milestone1.md), the ADA and IVI HLDs and their research notes, and the existing Phase 3/4/5 task files. It assigns three new IDs (`12.3.7.1`, `12.3.7.2`, `5.3.7.3`), supersedes one in place (`12.3.4.2`), and renumbers nothing.*
