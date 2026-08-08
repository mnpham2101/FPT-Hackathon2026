# Phase 3 — Object Detection from Video (R12): Full Task Breakdown

> **Authority & context:**
> - **Phase content:** [milestone1_high_level_plan.md § Phase 3](../documents/Plan/milestone1_high_level_plan.md#phase-3--object-detection-from-video-r12--runs--with-phase-4) — its four acceptance checkboxes are the phase output.
> - **Design:** [ada-ecu-hld.md](../documents/Design/ADA-ECU/ada-ecu-hld.md) — §4 folder structure for every path, §6 env tables for every constant; **[D6](../documents/Design/ADA-ECU/ada-ecu-design-decisions.md#d6--r12-detector-frame-source-seam-inference-distance-zero-c-evidence)** is this phase's design (frame-source seam, inference, distance, association, emission, zero-C evidence, model provenance) and **[D10](../documents/Design/ADA-ECU/ada-ecu-design-decisions.md#d10--clock-domains-and-stimulus-paced-against-clock_monotonic)** adds the detector's real-time pacing.
> - **Video source:** video-source-for-r12.md *(deprecated)* §3 the clip spec and its KPIs; and **the clip's own record**, [ADA_ECU/media/ego-b-occluding-c.source.md](../ADA_ECU/media/ego-b-occluding-c.source.md) — provenance, licence, encode command, content verdict, and the accepted duration deviation.
> - **Requirements:** [m1-cooperative-awareness.md §2](../requirements/m1-cooperative-awareness.md) R3, R5, R12, R18 and §3(g) — referenced by number, never restated.
> - **Run timing:** [m1-run-timing-and-event-triggering.md](../requirements/m1-run-timing-and-event-triggering.md) — §6.2's detector timestamp ruling (`12.3.2.6`), §6.1's three pacing keys and R20's detector half (`12.3.2.1`, `12.3.2.8`), and §6.6(g)'s warm-up budget (`12.3.5.2`, `22.3.6.3`).
> - **Phase 2 baseline (do not re-plan):** [phase2_tasks.md § Phase 2 overview](phase2_tasks.md#phase-2-overview) — the C++ core, the store, the R13 machine, the CRA seam, `src/observer/detector_reader` with its `DETECTOR_CMD` process contract, the image and the `ada-ecu-image` lane, `tools/check_clip_spec.py`.
> - **Rules:** [task-planning-conventions.md](../.claude/rules/task-planning-conventions.md); [node-code-layout.md](../.claude/rules/node-code-layout.md).
>
> **Task ID legend:** `X.3.Z.W` — X = requirement served · 3 = this phase · Z = task group · W = subtask position. IDs are stable; never renumber, never reuse a retired one. Reserved, not in use: `12.3.4.1`, `12.3.4.2`.
>
> **Runs in parallel with Phase 4.** The two never call each other — they meet only at the R3 store: this phase writes `own_sensor` entries through the JSONL subprocess boundary, Phase 4 reads the store.

## Phase 3 overview

**Objective.** Replace Phase 2's JSONL fixture with real perception: a YOLO11n ONNX detector reads the committed clip, finds **B — the visible occluder**, estimates its range, and streams R3 JSONL on stdout into the same store through the same interface. Zero entries labelled C, checked by `ADA_ECU/tools/check_zero_c.py`.

**Input (must exist before start).** Every input below exists on `main` except the first bullet's Phase 2 deliverables, which Phase 2 produces:

- Phase 2 complete: `src/observer/detector_reader` (the `DETECTOR_CMD` + stdout-JSONL contract), `src/parser/r3_parser`, the store, `ada_ecu`, the ADA image and its CI lane, `ADA_ECU/tools/check_clip_spec.py`.
- Phase 0's frozen `ADA_ECU/detector/contracts/tracked_object.py` binding, `detector/requirements-dev.txt`, and `ADA_ECU/detector/tests/test_r3_roundtrip.py`.
- **The clip is in the repo.** `ADA_ECU/media/ego-b-occluding-c.mp4` — 1280×720, 20 fps CFR, H.264 High / yuv420p, 200 frames / 10.0 s, 5 261 876 bytes, no audio — with its provenance record beside it. Licence: Pexels License, commercial use and modification permitted, attribution given anyway. **Sourcing, cutting and re-encoding video is `12.3.7.1`'s alone; no other subtask in this phase touches a video file.**

**Output (phase acceptance = the four milestone boxes):**

- [x] Detection log over the provided clip with per-frame objects and distance estimates (R12) — closed by `12.3.5.2`.
- [x] Entries enter the store via the same R3 interface as relayed entries, `source = own_sensor` — mock no longer required — closed by `3.3.5.3`.
- [x] **Zero detections labeled C** — closed by `12.3.5.1` (`tools/check_zero_c.py`) + `12.3.5.4` (the CI lane that makes it repeatable) + the structural argument in [HLD D6](../documents/Design/ADA-ECU/ada-ecu-design-decisions.md#d6--r12-detector-frame-source-seam-inference-distance-zero-c-evidence) + the clip's own content verdict.
- [ ] Runs CPU-only on the provided clip; offline pace acceptable — closed by `12.3.5.2` (measured effective inference rate ≥ 5 Hz, i.e. ≤ 200 ms per sampled frame) on the CI Linux runner, and `5.3.6.2` (the same measurement on the deployed node — research note KPI 3 *(deprecated)*).

**The clip is 10 s, a deviation from the research note §3 *(deprecated)* 60–120 s duration row, accepted with looping as its remedy** ([provenance record § The remaining deviation](../ADA_ECU/media/ego-b-occluding-c.source.md)). B is the lead vehicle only across a 10 s window of the source. A longer run is obtained by **looping** (`DETECTOR_LOOP=true`, default), each loop reading as a fresh approach cycle with B re-appearing at ~60 m and closing again. Every duration-sensitive check in this phase and in Phase 4 is worded against a **looped** run, not a single pass.

**Suggested branch (suggestion only — creation is the user's call):** `feat/phase3-ada-detector`. One branch for the whole phase; subtasks commit onto it. It branches from Phase 2's branch (or from `main` once Phase 2 merges) — it needs `detector_reader` and the image, nothing from Phase 4.

### Execution labels

`AI` and `Human`, the vocabulary of [deploy-ada-ecu-walkthrough.md §7](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#7-work-division-between-ai-and-human); the definitions are [phase2_tasks.md § Execution labels](phase2_tasks.md#execution-labels).

| Label | Who performs it |
|---|---|
| `AI` | A spawned agent. Code, tests and CI go to an implementation subagent; a row marked `AI — car-sky` goes to [[car-sky]] for the authenticated REST calls, registry checks and deployed-log reads. |
| `Human` | A person, outside any tool an agent holds. The planner keeps the ID and the done-tracking in every case. |

**Phase 3 is 23 `AI` subtasks — 21 to an implementation subagent, 2 to [[car-sky]] — plus the three `Human` steps below.** The human steps carry no subtask ID because the person performs them outside the task tree. A subtask's label is its executor; a step inside a subtask performed by someone else names that executor in the step.

| `Human` step | What depends on it |
|---|---|
| Create and push the phase branch `feat/phase3-ada-detector` | every acceptance worded "lane green on the pushed branch" — `12.3.1.1`, `12.3.3.3`, `12.3.3.4`, `12.3.5.4` |
| Confirm a workflow run in the Actions web UI and download its artifact — an agent session holds no GitHub token ([walkthrough §7](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#7-work-division-between-ai-and-human)) | every lane acceptance, and the run artifact `12.3.4.3` and `12.3.5.2` read |
| Fetch `yolo11n.pt` from the Ultralytics release assets when the export host has no egress to them | `12.3.3.1` |

Four constraints specific to this phase:

- **Detector dependencies may not install on the dev host.** `onnxruntime` and `opencv-python-headless` have no guaranteed wheel for Windows-on-ARM. Every detector test therefore runs on CI (`ada-detector-tests`, `12.3.3.3`) and degrades to `pytest.importorskip` locally — the skip-locally / run-on-CI pattern of [`Scenario_Player/tests/test_encoder_golden.py`](../Scenario_Player/tests/test_encoder_golden.py). A subtask whose tests only *skip* locally is not done until its CI lane is green.
- **Every run of the real detector happens on the CI Linux runner**, in lane `ada-detector-run` (`12.3.3.4`), which captures stdout and uploads it with the measured summary as a workflow artifact. `12.3.4.3` and `12.3.5.2` read that artifact rather than running the detector themselves.
- **Every image build, `docker history` read and image push happens in GitHub Actions.** The dev host carries no Docker daemon and no binfmt emulation, and nothing in this repo is built by hand ([milestone1_high_level_plan.md § Phase 4](../documents/Plan/milestone1_high_level_plan.md#phase-4--obscured-object-fusion-relayed-c--risk--warning-r13r15--runs--with-phase-3)).
- **Detector modules read env only through `detector/config.py`** (HLD §6). No `os.environ` outside that file.

### Subtask discipline

Identical to [phase2_tasks.md § Subtask discipline](phase2_tasks.md#subtask-discipline-applies-to-every-subtask-below) — not restated. **21 of 24 subtasks are done**; `5.3.6.2`, `22.3.6.3` and `5.3.7.3` wait on Phase 4's isolated-Room log and car-sky.

### Per-node build commands (cited in acceptance below)

| Area | Build + test command | Verified |
|---|---|---|
| `ADA_ECU/detector/` | `pip install -r ADA_ECU/detector/requirements.txt -r ADA_ECU/detector/requirements-dev.txt && python -m pytest ADA_ECU/detector/tests` | CI `ada-detector-tests` (`12.3.3.3`); local with skips |
| `ADA_ECU/` (C++ core) | `cmake -S ADA_ECU -B ADA_ECU/build && cmake --build ADA_ECU/build -j $(nproc) && ctest --test-dir ADA_ECU/build --output-on-failure` | CI `ada-core-build` |
| `ADA_ECU/tools/` | `python -m py_compile ADA_ECU/tools/<script>.py` | local + CI |
| ADA image | `docker buildx build --platform linux/arm64 --provenance=false --sbom=false -t m1-ada-ecu:latest ADA_ECU/` | CI `ada-ecu-image` |

**Local tag and registry tag are different strings.** The local build tag is `m1-ada-ecu:latest` ([HLD D9](../documents/Design/ADA-ECU/ada-ecu-design-decisions.md#d9--deployment-shape) and [§11](../documents/Design/ADA-ECU/ada-ecu-hld.md#11-tech-stack-build-and-ci)); the tag CI builds, pushes and the blueprint pulls is `registry.hackathon-2.carsky.io/m1-ada-ecu:latest` ([node-ada-ecu.md](../requirements/car-sky-guide/node-ada-ecu.md)). Every acceptance below names which of the two it means.

### CI ruling for this phase

New lanes go in a new `.github/workflows/phase3-ci.yml` — *a lane belongs to the phase that created it* ([phase1-ci.yml](../.github/workflows/phase1-ci.yml) header). Four jobs: `ada-detector-wheels` (`12.3.1.1`), `ada-detector-tests` (`12.3.3.3`), `ada-detector-run` (`12.3.3.4`), `ada-zero-c` (`12.3.5.4`). `ada-core-build` and `python-tests` in phase0-ci.yml stay untouched; `ada-ecu-image` in `phase4-ci.yml` is reused, not duplicated.

---

## Task Group 3.1 — arm64 dependency verification (serves R12; HLD §11 item 6)

> The one unproven platform assumption in this phase, taken first. If `onnxruntime` has no aarch64 wheel for `python:3.11-slim`, the image build falls back to a source build under emulation whose duration is unbounded and which can exhaust the job timeout.

### [x] `12.3.1.1` — `phase3-ci.yml` + lane `ada-detector-wheels` *(AI — runs before every other subtask in this phase)*

**Objective:** prove, before any detector code exists, that `onnxruntime`, `opencv-python-headless` and `numpy` install as prebuilt wheels for `linux/arm64` on `python:3.11-slim`.

**Scope — steps in order:**

1. Create `.github/workflows/phase3-ci.yml` with the same `on:`/`concurrency:` block as [phase1-ci.yml](../.github/workflows/phase1-ci.yml).
2. Write a header comment naming what the file carries.
3. Add job `ada-detector-wheels` with a qemu setup step.
4. Run `docker run --platform linux/arm64 python:3.11-slim` executing `pip install --only-binary=:all: onnxruntime opencv-python-headless numpy`.
5. Follow it with an import smoke: `import onnxruntime, cv2, numpy; print(versions)`.
6. Keep `--only-binary=:all:` so the lane exits non-zero, naming the package with no wheel, rather than starting a source build.
7. Print the escalation note on failure: pin an older `onnxruntime` carrying an aarch64 wheel, switch the base to a distro whose apt carries the package, or accept a source build with a raised timeout. The choice is [[project-architecture]]'s, not the implementing subagent's.

`.github/workflows/` is explicitly in this subtask's write scope.

**Acceptance:** lane green on the pushed branch with the three versions printed; or lane red with the failing package named and the item escalated (§ Open items item 1). A red lane completes this subtask; the escalation in § Open items item 1 follows, and `12.3.2.5` and `5.3.6.1` stay blocked until it is resolved.

**Dependencies:** none — first in the phase. **Commit:** `[12.3.1.1] chore: add the arm64 detector-wheel CI lane`

---

## Task Group 3.2 — Detector modules (serves R12; HLD D6)

> The Python subprocess. Paths from [HLD §4](../documents/Design/ADA-ECU/ada-ecu-hld.md#4-folder-structure); every module is importable and unit-testable on its own, and none of them opens a socket or reads env directly.

### [x] `12.3.2.1` — `detector/config.py` + `detector/requirements.txt` *(AI)*

**Objective:** the detector's **only** env reader. Its runtime dependency manifest is inseparable from it: the loader's own test run needs the pinned runtime set, and `12.3.1.1` proved exactly those versions.

**Scope — steps in order:**

1. Write `detector/config.py` as a frozen dataclass loaded from env, carrying the [HLD §6](../documents/Design/ADA-ECU/ada-ecu-hld.md#6-internal-components) detector half with the defaults in the table below.
2. Take the env mapping as an injectable argument so tests never mutate `os.environ`.
3. Validate: positive stride, thresholds in [0, 1], positive width, HFOV in (0, 180), positive `DETECTOR_CLIP_FPS` when set, `DETECTOR_START_DELAY_S` ≥ 0.
4. Raise `ValueError` naming the variable on any invalid value; `main.py` exits non-zero on it (`12.3.2.7`).
5. Write `detector/requirements.txt` with `onnxruntime`, `opencv-python-headless` and `numpy`, each **pinned** to the version `12.3.1.1` proved.
6. Add a leading `-r requirements.txt` to Phase 0's `requirements-dev.txt` so a dev install carries the runtime set.
7. Write `detector/tests/test_config.py`: defaults when unset; each override; each rejection case.

| Env key | Default |
|---|---|
| `VIDEO_CLIP_PATH` | `/app/media/ego-b-occluding-c.mp4` |
| `DETECTOR_FRAME_STRIDE` | `4` |
| `DETECTOR_LOOP` | `true` |
| `DETECTOR_REALTIME_PACING` | `true` |
| `DETECTOR_CLIP_FPS` | `20.0` — the committed clip's declared rate ([HLD §6](../documents/Design/ADA-ECU/ada-ecu-hld.md#6-internal-components)); overridable, and the frame source's `CAP_PROP_FPS` is the fallback when a different clip is mounted |
| `DETECTOR_START_DELAY_S` | `0.0` |
| `MODEL_PATH` | `/app/models/yolo11n.onnx` |
| `CONF_THRESHOLD` · `IOU_THRESHOLD` | `0.35` · `0.45` |
| `TRACK_IOU_MIN` | `0.3` |
| `VEHICLE_WIDTH_M` · `CAMERA_HFOV_DEG` | `1.8` · `60` |

The three pacing keys are [HLD §6](../documents/Design/ADA-ECU/ada-ecu-hld.md#6-internal-components) rows and are consumed by `12.3.2.8`; their behaviour is [D10](../documents/Design/ADA-ECU/ada-ecu-design-decisions.md#d10--clock-domains-and-stimulus-paced-against-clock_monotonic). `DETECTOR_FRAME_STRIDE` is a decimation stride, not a rate, and 5 Hz effective is an assumed CPU throughput rather than an enforced one ([m1-run-timing-and-event-triggering.md §2(d)](../requirements/m1-run-timing-and-event-triggering.md)).

**Acceptance:** pytest green on CI `ada-detector-tests`; no `os.environ` access anywhere else in `detector/`.

**Dependencies:** after `12.3.1.1` (pin versions to what installed). **Commit:** `[12.3.2.1] feat: add the detector config loader and runtime requirements`

### [x] `12.3.2.2` — Frame-source seam `detector/frame_source.py` *(AI)*

**Objective:** the mandatory D6 seam — frame acquisition behind an interface, so a future CarSky `video` pin is one new implementation and touches nothing downstream (research note §2 (a′) *(deprecated)*).

**Scope — steps in order:**

1. Define the `Frame(index, timestamp_ms, image, width, height)` dataclass.
2. Define the `FrameSource` protocol with `iter_frames() -> Iterator[Frame]`, and expose the source's declared rate so `12.3.2.8` can read it.
3. Implement `FileFrameSource` over an OpenCV `VideoCapture` on `VIDEO_CLIP_PATH`, yielding every `DETECTOR_FRAME_STRIDE`-th frame.
4. Set `timestamp_ms = frame_index / fps * 1000` — clip time, a detector-local field, never an R3 timestamp (`12.3.2.6`).
5. At EOF, re-open from frame 0 when `DETECTOR_LOOP` is true; otherwise stop.
6. Raise with the path named when the file cannot be opened.
7. Keep `frame_index` accumulating across loops rather than resetting to 0: the pacer's deadline is computed from the sampled-frame ordinal ([D10](../documents/Design/ADA-ECU/ada-ecu-design-decisions.md#d10--clock-domains-and-stimulus-paced-against-clock_monotonic)), so a resetting index would move the deadline backwards at every loop. Expose the within-clip index separately if a caller wants it.
8. Implement `SyntheticFrameSource`, generating N frames of a configurable size with no clip on disk — the CI import-and-plumbing path, never a detection-evidence path (`12.3.3.2`).
9. Write `detector/tests/test_frame_source.py`: stride selects the expected indices; loop restarts the *file* while the yielded index keeps increasing; EOF without loop terminates; the synthetic source yields the declared count and shape; a missing file raises.

Pacing is `12.3.2.8`'s, not this file's: `iter_frames()` stays a generator that assumes nothing about the consumer's rate.

**Acceptance:** pytest green on CI; no OpenCV call anywhere else in `detector/` except this file.

**Dependencies:** after `12.3.2.1`. **Commit:** `[12.3.2.2] feat: add the detector frame-source seam`

### [x] `12.3.2.3` — Distance estimation `detector/distance.py` *(AI — parallel with 12.3.2.2)*

**Objective:** the pinhole known-width range and lateral offset (D6) — pure maths, no model, no frames.

The three formulas, from D6:

```
f_px = (frame_w / 2) / tan(CAMERA_HFOV_DEG / 2)
d    = VEHICLE_WIDTH_M × f_px / bbox_w_px
y    = (bbox_u_center − frame_w / 2) × d / f_px
```

**Scope — steps in order:**

1. Implement the three formulas above in `detector/distance.py`.
2. Take every input as a parameter; this module performs no env read.
3. Return `None` for a zero or negative `bbox_w_px` rather than dividing.
4. State the rationale in a one-line docstring linking D6; the rejected estimators stay in D6 and are not restated in code comments.
5. Write `detector/tests/test_distance.py` with four cases: hand-computed values for at least three (bbox width, frame width, HFOV) triples; a wider bbox is always nearer; right of centre is `+y`; the degenerate-width guard returns `None`.

**Acceptance:** pytest green locally **and** on CI (this module has no heavy dependency, so it must pass locally too).

**Dependencies:** after `12.3.2.1`. **Commit:** `[12.3.2.3] feat: add pinhole known-width distance estimation`

### [x] `12.3.2.4` — Frame-to-frame association `detector/tracker.py` *(AI — parallel)*

**Objective:** stable `own:<n>` ids across sampled frames by greedy IoU matching (D6) — no tracking library.

**Scope — steps in order:**

1. Greedily IoU-match the current frame's boxes against the previous **sampled** frame's.
2. Hold an id while IoU ≥ `TRACK_IOU_MIN`.
3. Mint a new id from a monotonic counter otherwise.
4. Format every id as `own:<n>`; the detector can never mint a `v2x:` id (the structural half of the zero-C argument, D6).
5. Perform no time-based prediction; one dominant occluder is the design target.
6. Carry several concurrent tracks rather than one — the clip holds adjacent-lane and oncoming traffic, which is expected and is not C ([provenance record § C is synthetic](../ADA_ECU/media/ego-b-occluding-c.source.md#c-is-synthetic--what-the-criterion-actually-constrains)).
7. Write `detector/tests/test_tracker.py` with four cases: an id survives a small box drift across frames; a box jump below `TRACK_IOU_MIN` yields a new id; two boxes get distinct ids and do not swap; ids are never reused.

**Acceptance:** pytest green locally and on CI.

**Dependencies:** after `12.3.2.1`. **Commit:** `[12.3.2.4] feat: add greedy IoU frame-to-frame association`

### [x] `12.3.2.5` — Inference `detector/inference.py` *(AI)*

**Objective:** the YOLO11n ONNX Runtime CPU session behind a swappable `Detector` protocol (D6, report §3(g)).

**Scope — steps in order:**

1. Define the `Detector` protocol: `detect(image) -> list[Detection(bbox_xywh, score, coco_class)]`.
2. Implement `OnnxDetector` on an ONNX Runtime **CPU** provider over `MODEL_PATH`.
3. Letterbox each frame to 640×640, recording the scale and pad so boxes map back to source pixels.
4. Apply the score floor `CONF_THRESHOLD` and NMS at `IOU_THRESHOLD`.
5. Collapse the COCO classes `car`, `truck`, `bus` and `motorcycle` to the R3 `class: "vehicle"`, and drop everything else.
6. Put the `FakeDetector` returning scripted boxes in the test file, not in `detector/`.
7. Write `detector/tests/test_inference.py` (planner-designated path, § Open items item 2) with four cases: a box in letterboxed coordinates maps back to the expected source pixels; NMS suppresses an overlapping duplicate; the class map collapses the four vehicle classes and drops a person; the ONNX session test loads the model and reports the expected input/output shapes.
8. Guard the session test with `pytest.importorskip` and skip it additionally when `models/yolo11n.onnx` is absent.

**Acceptance:** pytest green on CI `ada-detector-tests` with the session test **executed** (the lane installs the wheels and the model is committed by `12.3.3.1`).

**Dependencies:** after `12.3.2.1` + `12.3.1.1`; the session test unskips once `12.3.3.1` lands. **Commit:** `[12.3.2.5] feat: add the YOLO11n ONNX inference stage`

### [x] `12.3.2.6` — Emission `detector/emit.py` *(AI)*

**Objective:** one R3 JSONL line per detection per sampled frame, through the **frozen** Phase 0 binding (D6).

**Scope — one step per emitted field, then the write:**

1. Build the object through `detector/contracts/tracked_object.py`; declare no second model (D1).
2. `source` — the constant `own_sensor`.
3. `id` — the tracker's `own:<n>`.
4. `class` — the constant `vehicle`.
5. `position` — `{x: distance, y: lateral}` from `12.3.2.3`.
6. `distance` — the pinhole range from `12.3.2.3`.
7. `speed` — `|Δdistance| / Δt` between consecutive sampled frames for the same id, and 0 on a track's first sampled frame ([D6](../documents/Design/ADA-ECU/ada-ecu-design-decisions.md#d6--r12-detector-frame-source-seam-inference-distance-zero-c-evidence): frozen R3 bounds `speed` at `minimum: 0`, so a signed closing rate is not representable there; the sign lives in the `distance` series). `Δt` is the difference of the two frames' clip-time `Frame.timestamp_ms`, which stays exact under a constant frame rate.
8. `confidence` — the detection score.
9. `state` — the constant `not_tracked`; the store is the sole `state` writer (D3).
10. `timestamps` — per the ruling below.
11. Write one compact JSON object per line to stdout, flushed per line.

**Timestamp ruling — [HLD §10.2](../documents/Design/ADA-ECU/ada-ecu-hld.md#102-r3--the-object-model-of-the-store-owned) and [m1-run-timing-and-event-triggering.md §6.2](../requirements/m1-run-timing-and-event-triggering.md).** All R3 timestamps are `CLOCK_REALTIME` epoch ms, which is what the frozen `contracts/r3-tracked-object.schema.json` defines `measured` and `received` to be.

| Field | Value for `own_sensor` |
|---|---|
| `measured` | `CLOCK_REALTIME` at frame capture |
| `received` | `CLOCK_REALTIME` at the emit instant |
| `lastUpdated` | written by the store, not by the detector |

The clip-time value `frame_index / fps * 1000` stays a detector-local `Frame` field (`12.3.2.2`) and reaches no R3 timestamp.

**Tests — `detector/tests/test_emit_contract.py`:**

1. Emitted lines validate against the synced `ADA_ECU/contracts/r3-tracked-object.schema.json`, loaded from disk.
2. The id and source conventions are exact, and no `v2x_relayed` line is producible.
3. A track's first sampled frame emits `speed` 0.
4. The second frame's `speed` matches the hand-computed `|Δd| / Δt`.
5. A closing track — distance decreasing across three sampled frames — emits a positive `speed` on every frame after the first.
6. `measured` is an epoch stamp preceding `received`, and both advance across sampled frames.

**Acceptance:** pytest green locally and on CI; the schema is loaded, never restated.

**Dependencies:** after `12.3.2.3` + `12.3.2.4`. **Commit:** `[12.3.2.6] feat: emit R3 JSONL from the detector`

### [x] `12.3.2.8` — Real-time pacer `detector/pacer.py` *(AI — parallel)*

**Objective:** the [HLD §6](../documents/Design/ADA-ECU/ada-ecu-hld.md#6-internal-components) `detector/pacer` component — hold each sampled frame until its wall-clock instant, and apply `DETECTOR_START_DELAY_S` before the first.

**Scope — steps in order:**

1. Create `detector/pacer.py` at the path [HLD §4](../documents/Design/ADA-ECU/ada-ecu-hld.md#4-folder-structure) designates.
2. Wrap an `Iterator[Frame]` and yield the same frames, released on schedule; the pacer adds, drops and reorders nothing.
3. Compute each deadline as `t0 + n × DETECTOR_FRAME_STRIDE / DETECTOR_CLIP_FPS` on `time.monotonic()`, where `n` is the sampled-frame ordinal ([D10](../documents/Design/ADA-ECU/ada-ecu-design-decisions.md#d10--clock-domains-and-stimulus-paced-against-clock_monotonic)).
4. Compute the deadline rather than accumulating fixed sleeps — an accumulated sleep folds the inference cost into scenario time and drifts unbounded over a run (D10).
5. Sleep `DETECTOR_START_DELAY_S` from `t0` before releasing the first frame.
6. Take `DETECTOR_CLIP_FPS` from the config when it is set, and from the frame source's declared rate otherwise (`12.3.2.1`, `12.3.2.2`).
7. Pass frames through with no sleep when `DETECTOR_REALTIME_PACING` is false.
8. Release a late frame immediately rather than sleeping a negative interval, so a slow inference step never runs the schedule backwards.
9. Write `detector/tests/test_pacer.py` — the path [HLD §4](../documents/Design/ADA-ECU/ada-ecu-hld.md#4-folder-structure) designates — with an injected monotonic clock and sleep, and five cases: deadlines are `stride / fps` apart; the start delay precedes the first frame; a slow consumer does not shift later deadlines; pacing off yields no sleep; the frame sequence is unchanged in both modes.

**Acceptance:** pytest green locally and on CI; no `time.sleep` call anywhere else in `detector/`.

**Dependencies:** after `12.3.2.1`. **Commit:** `[12.3.2.8] feat: add the detector real-time pacer`

### [x] `12.3.2.7` — Entrypoint `detector/main.py` *(AI)*

**Objective:** the ego side of the D2 process contract — argv, wiring, exit codes. Controller only, no rules.

**Scope — steps in order:**

1. Load `config` (`12.3.2.1`).
2. Build the frame source — the file source by default, `--synthetic N` for the CI import-and-plumbing smoke.
3. Wrap it in the pacer (`12.3.2.8`).
4. Construct the `OnnxDetector` (`12.3.2.5`).
5. Drive the [HLD §6](../documents/Design/ADA-ECU/ada-ecu-hld.md#6-internal-components) pipeline in its stated order: source → pacer → inference → distance → tracker → emit.
6. Write startup and fatal errors to **stderr**, so stdout carries only R3 JSONL — the reader parses every stdout line.
7. Document the exit codes: 0 on clean EOF; non-zero for a missing clip, a missing model, or an invalid config.
8. Exit cleanly and promptly on SIGTERM.
9. Write `detector/tests/test_main.py` (planner-designated path) with three cases: a short synthetic run with a fake detector emits N valid JSONL lines on stdout and nothing else; a missing clip exits non-zero with the path named; SIGTERM terminates within a bounded time.

**Acceptance:** pytest green on CI; **stdout contains only JSONL** — asserted, because a stray print corrupts the process contract; the pacer is wired unconditionally and its behaviour is decided by config alone.

**Dependencies:** after `12.3.2.2` + `12.3.2.5` + `12.3.2.6` + `12.3.2.8`. **Commit:** `[12.3.2.7] feat: add the detector entrypoint`

---

## Task Group 3.3 — Model, CI fixture, detector lane (serves R12)

### [x] `12.3.3.1` — `tools/export_yolo11n.py` + committed `models/yolo11n.onnx` *(AI, with one Human step)*

**Objective:** the one-off Ultralytics → ONNX export and its committed artifact (D6 model provenance).

**External input, named rather than assumed:** the export downloads the `yolo11n.pt` weights from the Ultralytics release assets at run time, so the export host needs egress to them. Where the host has none, a **`Human`** fetches the file and places it beside the script, and the agent runs the export against the local file.

**Scope — steps in order:**

1. Write `ADA_ECU/tools/export_yolo11n.py`, which performs the Ultralytics → ONNX export (AGPL-3.0 accepted, report §4).
2. Record the input size and the opset in the script header.
3. Record the resolved weights URL, the SHA-256 of the downloaded `yolo11n.pt`, and the SHA-256 of the exported `yolo11n.onnx` in the script header, so a re-export is verifiable against the committed artifact.
4. Commit the resulting `ADA_ECU/models/yolo11n.onnx` (~10 MB).
5. Track it in plain git, as `media/ego-b-occluding-c.mp4` is tracked; do not use Git LFS.
6. Leave `.gitattributes` alone — it carries `*.onnx binary`, so no change is needed.
7. Keep the script out of CI and out of the image: committing the artifact is what keeps the image build offline-reproducible and Ultralytics out of the runtime dependency set.
8. Do not add the script's Ultralytics import to `requirements.txt`.

**Acceptance:** the ONNX file loads in an ONNX Runtime CPU session and reports the expected input/output shapes (asserted by `12.3.2.5`'s session test, which unskips here); `python -m py_compile` on the exporter passes.

**Dependencies:** after `12.3.1.1`. **Commit:** `[12.3.3.1] feat: export and commit the YOLO11n ONNX model`

### [x] `12.3.3.2` — CI video fixture `tools/make_sample_video.py` *(AI — parallel)*

**Objective:** the decoder smoke fixture, and **only** that (research note §2 (c) *(deprecated)*).

**Scope — steps in order:**

1. Write `ADA_ECU/tools/make_sample_video.py`, which writes a short MP4 to a path given on the command line.
2. Make the content deterministic and the size, fps and frame count configurable.
3. Commit no generated output, and add the generated files to `.gitignore`.
4. State in the module docstring, in one line, that it is a CI fixture and must never be the demo source: it writes flat grey rectangles, which a pretrained COCO detector will not classify as `car`, so a run against it produces zero detections and no R12 evidence.

**Acceptance:** `python -m py_compile` passes; the generated file opens through `FileFrameSource` and yields the declared frame count (asserted in `detector/tests/test_frame_source.py`, extended here).

**Dependencies:** after `12.3.2.2`. **Commit:** `[12.3.3.2] feat: add the CI sample-video fixture generator`

### [x] `12.3.3.3` — Lane `ada-detector-tests` *(AI)*

**Objective:** the Linux verification lane for group 3.2 — the only place the detector suite runs unskipped.

**Scope — the job's steps, in `phase3-ci.yml`:**

1. Check out the repository.
2. Install with pip cache: `pip install -r ADA_ECU/detector/requirements.txt -r ADA_ECU/detector/requirements-dev.txt`.
3. Run `python -m pytest ADA_ECU/detector/tests -q`.
4. Assert the skip count: the job fails when any test skips for a missing dependency.
5. Guard the job with a skip-with-notice while `ADA_ECU/detector/requirements.txt` is absent.

**Acceptance:** workflow YAML valid; run-blocks `bash -n` clean; lane green with 0 dependency-skips once group 3.2 lands.

**Dependencies:** after `12.3.1.1` (same file); lands before its consumers. **Commit:** `[12.3.3.3] chore: add the ADA detector test CI lane`

### [x] `12.3.3.4` — Lane `ada-detector-run` *(AI)*

**Objective:** the venue for every run of the real detector — a Linux runner that executes it over the committed clip and uploads the captured stdout as a workflow artifact.

**Scope — the job's steps, in `phase3-ci.yml`:**

1. Check out the repository, which carries `ADA_ECU/media/ego-b-occluding-c.mp4` and `ADA_ECU/models/yolo11n.onnx`.
2. Install the detector's runtime requirements with pip cache.
3. Run `detector/main.py` with `VIDEO_CLIP_PATH` set to the checked-out clip and `DETECTOR_LOOP=true`, for a wall-clock duration taken from a job input with a default of 60 s.
4. Capture stdout to a file, and capture stderr separately.
5. Compute and print a summary: sampled-frame count, elapsed wall time, effective sampled-frame rate, warm-up interval from process start to the first emitted line, and the interval between the last line of one clip pass and the first of the next.
6. Add a second arm with `DETECTOR_REALTIME_PACING=false`, and print the per-sampled-frame inference cost it measures. Pacing fixes the emit interval at `DETECTOR_FRAME_STRIDE / DETECTOR_CLIP_FPS`, so the paced arm cannot measure CPU throughput and the unpaced arm is what KPI 3 reads.
7. Upload the captured stdout, the stderr and both summaries as a workflow artifact.
8. Take the env values the run uses — stride, thresholds, `VEHICLE_WIDTH_M`, `CAMERA_HFOV_DEG` — from job inputs with the `12.3.2.1` defaults, so `12.3.4.3` can re-run with candidate constants without editing the workflow.
9. Guard the job with a skip-with-notice while `ADA_ECU/models/yolo11n.onnx` is absent.

**Acceptance:** workflow YAML valid; run-blocks `bash -n` clean; lane green on the pushed branch with a non-empty stdout artifact and the summary printed.

**Dependencies:** after `12.3.1.1` (same file) + `12.3.2.7` + `12.3.3.1`. **Commit:** `[12.3.3.4] chore: add the detector run CI lane`

---

## Task Group 3.4 — Distance calibration against the real clip (serves R12)

> The clip's content is judged in [the provenance record's § Content verdict](../ADA_ECU/media/ego-b-occluding-c.source.md) and is not re-judged here; its format gate and its image placement are `12.3.7.2`'s. This group owns the distance constants alone.

### [x] `12.3.4.3` — Retune the distance constants against the clip *(AI)*

**Objective:** close [HLD decision D6](../documents/Design/ADA-ECU/ada-ecu-design-decisions.md#d6--r12-detector-frame-source-seam-inference-distance-zero-c-evidence) — `VEHICLE_WIDTH_M` and `CAMERA_HFOV_DEG` are proposals with no calibration target until the detector runs on the real clip.

**Venue:** the run happens in lane `ada-detector-run` (`12.3.3.4`) on the CI Linux runner, and the agent reads its uploaded stdout artifact. The detector is not run on the dev host. Confirming the run and downloading the artifact is the **`Human`** step of § Execution labels.

**Scope — steps in order:**

1. Trigger `ada-detector-run` with the `12.3.2.1` default constants and read the uploaded stdout artifact.
2. Select B's track numerically: the track with the largest bounding-box area whose lateral offset stays within ±2 m of frame centre for ≥ 90 % of the sampled frames in which it appears. Adjacent-lane and oncoming vehicles are expected in the series, so no visual judgement decides which track is B.
3. Extract that track's estimated range series.
4. Compare it against the clip's recorded geometry — [the provenance record](../ADA_ECU/media/ego-b-occluding-c.source.md) states B is a white coach closing from roughly 60 m to roughly 10 m across the 10 s, present in every sampled frame.
5. Check the required property, which is a monotonic, consistently-biased range rather than absolute accuracy: the approach yields a decreasing series crossing `GATE_ENTER_M` (30 m) exactly once per loop.
6. On disagreement, retune **only** `VEHICLE_WIDTH_M` and `CAMERA_HFOV_DEG`, re-running the lane with the candidate values. Never retune the R13 gate (D3/D6): the gate is a requirement value and the camera constants are estimates. The coach is wider than the 1.8 m car default, so a systematic under-read of range is the expected first finding.
7. Record the final values as the defaults in `detector/config.py`, which is the node's only detector env reader.
8. Record the before/after series summary in `plans/doc/phase3-ada-detector-run.md`, naming the workflow run the artifact came from.
9. Hand the two final values to [[project-architecture]] for [HLD §6](../documents/Design/ADA-ECU/ada-ecu-hld.md#6-internal-components)'s **Env — detector** table and for [node-ada-ecu.md](../requirements/car-sky-guide/node-ada-ecu.md). **The implementing subagent does not edit either file** — the HLD is this node's sole design authority and the node guide is a platform reference, both owned by [[project-architecture]] ([CLAUDE.md § Roles](../CLAUDE.md), [hld-content-and-commit-format.md § How to apply](../.claude/rules/hld-content-and-commit-format.md)). The hand-off carries the measured series, the chosen values and the run they came from.

**Acceptance:** the estimated-range series for B is monotonic through the approach and crosses the gate once per loop; the two constants' final values are committed in `detector/config.py` and recorded in the run doc, and the hand-off to [[project-architecture]] is recorded there too.

**Dependencies:** after `12.3.2.7` + `12.3.3.1` + `12.3.3.4`. **Commit:** `[12.3.4.3] fix: retune the pinhole distance constants against the demo clip`

---

## Task Group 3.5 — Zero-C evidence and store integration (serves R12, R3, R18)

### [x] `12.3.5.1` — Zero-C check `tools/check_zero_c.py` *(AI)*

**Objective:** make the R19 zero-C claim falsifiable (D6) — the check fails the run on any of the three rules below.

**Scope — steps in order:**

1. Write `ADA_ECU/tools/check_zero_c.py` against the Python 3 standard library alone.
2. Take a detection log (R3 JSONL) as input, and the ADA `[EVT]` stream as an optional second input.
3. Take the spatial radius as a CLI argument with a default of 5.0 m, as [D6](../documents/Design/ADA-ECU/ada-ecu-design-decisions.md#d6--r12-detector-frame-source-seam-inference-distance-zero-c-evidence) words it — no env key and no literal in the comparison.
4. Fail the run when an own-sensor entry claims `source: v2x_relayed` (rule 1).
5. Fail the run when an own-sensor entry carries a `v2x:` id namespace (rule 2).
6. Fail the run when an own-sensor track sits within the radius argument of the relayed C position at the same timestamp (rule 3).
7. Add no fourth rule asserting that the detector found nothing but B: adjacent-lane and oncoming vehicles are expected in this clip, and such a rule would fail on correct footage ([provenance record § What this obliges downstream](../ADA_ECU/media/ego-b-occluding-c.source.md#what-this-obliges-downstream)).
8. Exit non-zero naming the offending line and the rule that fired.
9. Exit 0 printing the counts examined, so a vacuous pass is visible.
10. Write `ADA_ECU/tools/tests/test_check_zero_c.py` (planner-designated path) with three cases: a clean log exits 0; one planted violation per rule exits 1 naming that rule; an empty log exits non-zero, because nothing examined is not a pass.

**Acceptance:** `python -m py_compile` passes; the test passes locally and on CI.

**Dependencies:** after `12.3.2.6` (line shape). **Commit:** `[12.3.5.1] feat: add the R12 zero-C evidence check`

### [x] `12.3.5.2` — Detector run over the clip: the R12 detection log *(AI)*

**Objective:** the R12 acceptance artifact — a detection log over the provided clip with per-frame objects and distance estimates, at an acceptable offline pace.

**Venue:** the run happens in lane `ada-detector-run` (`12.3.3.4`) on the CI Linux runner, with the retuned constants of `12.3.4.3`, and the agent reads its uploaded stdout artifact. Confirming the run and downloading the artifact is the **`Human`** step of § Execution labels.

**Scope — steps in order:**

1. Trigger `ada-detector-run` with `DETECTOR_LOOP=true` and a duration of at least 60 s of wall time — six loops of the 10 s clip — so the rate measurement has a window and the loop path is exercised in the same run.
2. Download the stdout artifact, and commit a representative excerpt plus the summary into `plans/doc/phase3-ada-detector-run.md`; the full log is not committed.
3. Record research note KPI 2 *(deprecated)* — ≥ 99 % of declared frames read with zero decode errors, across loops.
4. Record KPI 3 — effective inference rate ≥ 5 Hz, i.e. wall-clock ≤ 200 ms per sampled frame — from the lane's unpaced arm, because pacing fixes the emit interval and the paced arm cannot measure CPU throughput (`12.3.3.4`).
5. Record KPI 4 — ≥ 1 `class = vehicle`, `source = own_sensor` entry with a distance estimate for ≥ 90 % of sampled frames.
6. Record KPI 5 — `check_zero_c.py` exit 0 over the whole log.
7. Raise `DETECTOR_FRAME_STRIDE` and re-run if KPI 3 fails, never changing the model (research note §5 *(deprecated)*); record the stride used.
8. Record the detector warm-up `W` — ONNX model load plus `VideoCapture` open, from process start to the first emitted R3 line. It is the value the bench's `start_delay_s` is set to, and R22's alignment budget is **−0.5 / +1.1 s** around it ([m1-run-timing-and-event-triggering.md §6.6(g)](../requirements/m1-run-timing-and-event-triggering.md); [SP D7](../documents/Design/SCENARIO-PLAYER/scenario-player-design-decisions.md#d7--the-demo-cycle-is-one-clip-length-and-its-geometry-is-solved-backwards-from-the-first-warning)). This run produces the **host** figure; the deployed one, which is what the bench is configured from, is `22.3.6.3`. The remedy for a large `W` is the bench's `start_delay_s`, never a change here.
9. Record the interval between the last emitted line of one clip pass and the first of the next, and whether B's track id is re-minted or carried. A long re-open stall is what would expire ego's own B track between cycles, which is `13.4.11.3`'s own-sensor half in Phase 4.
10. Record the paced-rate check: with `DETECTOR_REALTIME_PACING` true, the sampled-frame rate equals `DETECTOR_CLIP_FPS / DETECTOR_FRAME_STRIDE` within ±2 %, which is [HLD §12](../documents/Design/ADA-ECU/ada-ecu-hld.md#12-test-strategy) K4's bound read off this run's own stamps.

**Acceptance:** the four KPIs, the measured host warm-up `W`, the clip-pass interval and the paced-rate check recorded with their values; this closes the R12 detection-log box and the CPU-only/offline-pace box at runner level (`5.3.6.2` repeats KPI 3 on the deployed node, `22.3.6.3` the warm-up).

**Dependencies:** after `12.3.2.7` + `12.3.3.1` + `12.3.3.4` + `12.3.4.3` + `12.3.5.1`. **Commit:** `[12.3.5.2] docs: record the R12 detection log and KPI measurements`

### [x] `3.3.5.3` — Retire the mock: real detector into the real store *(AI)*

**Objective:** the second Phase 3 box — own-sensor entries enter the store through the **same** R3 interface as relayed entries, with the fixture no longer required.

**Scope — steps in order:**

1. Write no new module.
2. Add a sibling arm to `13.2.8.2`'s loopback lane that leaves the [HLD §6](../documents/Design/ADA-ECU/ada-ecu-hld.md#6-internal-components) default `DETECTOR_CMD` — `python3 /app/detector/main.py` — in place, where Phase 2's arm overrides it with the `cat …/own_sensor_mock.jsonl` fixture.
3. Drive that arm over the committed clip, `VIDEO_CLIP_PATH=ADA_ECU/media/ego-b-occluding-c.mp4`, as `12.3.5.4` does. A synthetic frame source produces zero detections (`12.3.3.2`), so it can produce neither an `own_sensor_ingest` line nor a track.
4. Assert `own_sensor_ingest` events and at least one `track_transition` to `tracked` for an `own:<n>` id — proving the JSONL crosses the process boundary, parses through `r3_parser`, and lands in the store via the same `upsert` as `r2_parser`.
5. Keep the synthetic source in a second, separate arm as the import-and-plumbing smoke, whose only expected observable is R3 JSONL on stdout — never a track, and never an `own_sensor_ingest` assertion.
6. Leave the Phase 2 fixture committed as a test artifact — it is what `--expect-no-tracks` and the deterministic admission cases run against.

**Acceptance:** the lane arm is green and observes `own:<n>` tracks reaching `tracked` with `source = own_sensor`; the ADA build + ctest stay green.

**Dependencies:** after `12.3.2.7` + `12.3.3.1` + Phase 2 `13.2.8.2`. **Commit:** `[3.3.5.3] test: drive the store from the real detector subprocess`

### [x] `12.3.5.4` — Lane `ada-zero-c` *(AI)*

**Objective:** make the zero-C check repeatable rather than a one-off run — it feeds R19, so it must not be a manual step.

**Scope — the job's steps, in `phase3-ci.yml`:**

1. Install the detector requirements.
2. Run `detector/main.py` over the committed clip, which is present in every checkout, capturing stdout. The synthetic source is not a fallback here: it produces zero detections and would leave nothing to examine.
3. Run `python ADA_ECU/tools/check_zero_c.py` over the captured stdout and fail the job on a non-zero exit.
4. Assert a non-zero examined-line count, so the lane cannot pass on an empty log.

**Acceptance:** lane green on the pushed branch with a non-zero examined count.

**Dependencies:** after `12.3.5.1` + `12.3.2.7` + `12.3.3.1` + `12.3.3.3` (same file). **Commit:** `[12.3.5.4] chore: add the zero-C evidence CI lane`

---

## Task Group 3.6 — Image and deployed measurement (serves R5)

### [x] `5.3.6.1` — Extend the ADA image with the detector and model *(AI)*

**Objective:** the deployable image carries both processes (D9 image layout).

**Scope — steps in order, all in `ADA_ECU/Dockerfile`:**

1. Verify `COPY media/ /app/media/` is present and sits above the lines added below; `12.3.7.2` owns that line and lands first. Do not duplicate or reorder it.
2. Add `COPY models/ /app/models/` after it.
3. Add `COPY detector/ /app/detector/` last, because it changes every commit while `media/` and `models/` rarely change.
4. Add `RUN pip install --no-cache-dir -r /app/detector/requirements.txt`.
5. Add nothing else — exactly two COPY lines and one RUN line.
6. Confirm `.dockerignore` keeps `detector/tests/` and `requirements-dev.txt` out of the image.

Layer order decides which blobs are re-pushed: `media/` and `models/` sit above `detector/` so their blobs are pushed once and cached thereafter. The image layout ends as `/app/ada_ecu`, `/app/entrypoint.sh`, `/app/detector/`, `/app/models/yolo11n.onnx`, `/app/media/ego-b-occluding-c.mp4`.

**Acceptance:** `ada-ecu-image` lane green; the lane's in-image run step starts `detector/main.py --synthetic` inside the pulled arm64 image and observes R3 JSONL on stdout — the import-and-plumbing smoke, whose only expected observable is that stream. It proves the wheels resolved for aarch64 in the real image, which is what [HLD decision D9](../documents/Design/ADA-ECU/ada-ecu-design-decisions.md#d9--deployment-shape) asks for.

**Dependencies:** after `12.3.1.1` + `12.3.2.7` + `12.3.3.1` + `12.3.7.2` (which must have written `COPY media/` first). **Commit:** `[5.3.6.1] feat: add the detector and model to the ADA image`

### [ ] `5.3.6.2` — Measure the deployed inference rate off the isolated ADA Room *(AI — car-sky)*

**Objective:** KPI 3 on the real 2-vCPU node rather than on a CI runner — and the proof that the **baked-in clip opens on the deployed node**, which no off-platform run can establish about the `COPY media/` delivery path.

**This subtask books no Room and performs no deploy.** It reads [phase4_tasks.md](phase4_tasks.md) group 4.11's already-saved `ada.log` from the isolated ADA Room — bridge + V2X bench mock + ADA + IVI sink mock. The detector needs no neighbour for this measurement and the ADA node's config is identical in every Room. The account allows two concurrent deployments; this subtask books no second Room slot, and group 4.11's saved log carries the measurement. If group 4.11 has not run yet, this subtask waits for it rather than deploying.

**Scope — reading steps, in order:**

1. Open `18.4.11.1`'s saved ADA node log.
2. Confirm `detector_spawn`, then a steady `own_sensor_ingest` stream.
3. Measure the effective sampled-frame rate from the event timestamps — ≥ 5 Hz, i.e. ≤ 200 ms per sampled frame. The deployed node runs with `DETECTOR_REALTIME_PACING` true, which caps the rate at `DETECTOR_CLIP_FPS / DETECTOR_FRAME_STRIDE`; reaching that cap is what proves the 2-vCPU node sustains the inference cost, and falling below it is what proves it does not.
4. Record the clip path the node reports opening. `detector_spawn` followed by `own_sensor_ingest` on the deployed node is the proof that the baked-in clip opened there ([m1-video-source-and-ivi-dashcam.md §7 KPI 9](../requirements/m1-video-source-and-ivi-dashcam.md)).
5. Record the deployed warm-up interval, which will exceed `12.3.5.2`'s runner figure, and the interval between the last line of one clip pass and the first of the next.
6. Report the rate whichever way it lands. If it falls short, the remedy is raising `DETECTOR_FRAME_STRIDE` in the ADA node config and re-reading on the next Room — config only, no rebuild. A rate below KPI 3 is also the Phase 4 failure mode where ego's own B track expires between updates and the composed geometry disappears ([deploy-ada-ecu-walkthrough.md §8.1 item 11](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#81-confirm-before-relying-on-these)).

**Acceptance:** the measured deployed rate, warm-up, clip-pass interval and reported clip path recorded in `plans/doc/phase3-ada-detector-run.md`, citing the Room and log the reading came from.

**Dependencies:** after `5.3.6.1` (so the deployed image carries the detector) + Phase 4 `18.4.11.1` (the log to read). **Commit:** `[5.3.6.2] docs: record the deployed ADA inference rate and clip-open evidence`

### [ ] `22.3.6.3` — Measure the detector warm-up `W` on the deployed node and derive `start_delay_s` *(car-sky)*

**Objective:** produce **one number** — the interval, on the deployed ADA node, from the detector process spawn to its first emitted R3 line. It is the value the bench's `start_delay_s` is set to, and R22 cannot be configured without it ([m1-run-timing-and-event-triggering.md §6.6(g)](../requirements/m1-run-timing-and-event-triggering.md), [§9 item 4](../requirements/m1-run-timing-and-event-triggering.md); [SP D7](../documents/Design/SCENARIO-PLAYER/scenario-player-design-decisions.md)).

**This subtask books no Room and performs no deploy.** Like `5.3.6.2` it reads [phase4_tasks.md](phase4_tasks.md) `18.4.11.1`'s already-saved `ada.log` from the isolated ADA Room. The account allows two concurrent deployments; a second slot for a log read is waste. If group 4.11 has not run, this subtask waits.

**Scope — reading and arithmetic, entirely:**

- From the saved ADA node `[EVT]` stream, take `mono_ms` of the **`detector_spawn`** line and `mono_ms` of the **first `own_sensor_ingest`** line. `W = Δ mono_ms / 1000`, seconds. Both stamps are this node's own `CLOCK_MONOTONIC`, so the measurement crosses no clock domain ([ADA D10](../documents/Design/ADA-ECU/ada-ecu-design-decisions.md#d10--clock-domains-and-stimulus-paced-against-clock_monotonic)).
- The first `own_sensor_ingest` is also **`T0`**, the R22 run origin ([ADA D11](../documents/Design/ADA-ECU/ada-ecu-design-decisions.md#d11--r22-run-choreography-the-run-origin-the-paced-clip-and-the-risk-band-pair)). Record its `mono_ms` and `epoch_ms` alongside `W`, so a later K6 read has the origin the run actually used.
- Confirm the run was paced: the node's `DETECTOR_REALTIME_PACING` as deployed, and the observed sampled-frame period against `DETECTOR_FRAME_STRIDE / DETECTOR_CLIP_FPS`. **An unpaced run yields no usable `W`** — clip time is not run time, so report it as unmeasured and hand back rather than deriving a number from it.
- Take `W` over **at least three detector spawns** where the log has them (a restart, or a second Room), and record the spread. A single sample with no spread is still acceptable evidence; state which it is.
- **Derive the bench value:** `start_delay_s = W`, since `DETECTOR_START_DELAY_S` stays `0.0` (D11). State it to one decimal, with the −0.5 / +1.1 s tolerance band around the measurement, so `22.1.13.4` writes a value with a stated margin rather than a guess.

**Executor:** *car-sky*, per [deploy-ada-ecu-walkthrough.md §7](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#7-work-division-between-ai-and-human)'s AI row for reading node logs. It performs no canvas step and no deploy click.

**Acceptance:** `W` in seconds, `T0`'s two stamps, the observed sampled-frame period, the deployed `DETECTOR_REALTIME_PACING` value, and the derived `start_delay_s` with its tolerance band — all recorded in `plans/doc/phase3-ada-detector-run.md`, citing the Room and the log the reading came from.

**Dependencies:** after `5.3.6.1` + `12.3.2.8` (the run must be paced) + Phase 4 `18.4.11.1` (the log). **Unblocks** Phase 1 `22.1.13.4`. **Commit:** `[22.3.6.3] docs: record the deployed detector warm-up and the derived bench start delay`

---

## Task Group 3.7 — The demo clip in the repo and in the image (serves R12, R5)

> This group puts the committed clip into the image and measures what that costs. The clip itself is an input (§ Input), not a deliverable of any subtask here.
>
> C is scenario-injected and never in the footage; the decoy criterion and the bench's obligation are [§ C is synthetic](../ADA_ECU/media/ego-b-occluding-c.source.md#c-is-synthetic--what-the-criterion-actually-constrains).

### [x] `12.3.7.1` — The demo clip, its provenance and the binary tracking rules *(AI)*

**Objective:** the clip, its provenance record, and the rules that keep exactly one video file in git — `*.mp4 binary` / `*.onnx binary` in `.gitattributes`, `ADA_ECU/media/source/` in `.gitignore`.

**[The provenance record](../ADA_ECU/media/ego-b-occluding-c.source.md) is the authority, and no subtask may restate or contradict it:** the source and its URL, the Pexels License and the attribution string, the exact ffmpeg encode command, both SHA-256s, the content verdict per criterion, the § C is synthetic reasoning and its downstream obligations, the rejected candidates, and the accepted 10 s duration with looping as its remedy.

**Status:** done, commit `3d55d7b`. Candidate selection detail is [§ Rejected candidates](../ADA_ECU/media/ego-b-occluding-c.source.md#rejected-candidates).

### [x] `12.3.7.2` — Bake the committed clip into the ADA image *(AI)*

**Objective:** the clip reaches the container the only way it can — inside the image. A Container Node has no volume, no bind mount and no host path (research note §1 *(deprecated)*). The single objective is that one Dockerfile line; the size and digest measurement below is that line's acceptance evidence, not a second deliverable.

**Venue:** every build, `docker history` read and in-image run step happens in the `ada-ecu-image` lane. The dev host carries no Docker daemon and no binfmt emulation, so this subtask reads the lane's log rather than building locally.

**Scope — the image only. No video file is created, moved, re-encoded or re-committed.**

- Run `python ADA_ECU/tools/check_clip_spec.py ADA_ECU/media/ego-b-occluding-c.mp4` (from `12.2.9.1`) and require **exit 0**. This is the format gate over the committed artifact; the content rows are the provenance record's and are not re-judged here.
- `ADA_ECU/Dockerfile`: add **one line only — `COPY media/ /app/media/`** — as its own early layer. **This subtask is the sole owner of that line**; `5.3.6.1` adds `COPY models/` and `COPY detector/` afterwards and only verifies this one. Write it where those two will follow it:

  ```dockerfile
  # media and model: change rarely, pushed once, cached thereafter
  COPY media/  /app/media/    # <- this subtask
  COPY models/ /app/models/   # <- 5.3.6.1, later
  # code: changes every commit
  COPY detector/ /app/detector/   # <- 5.3.6.1, later
  ```

  Ordering is why the split matters: `media/` must sit above `models/` and `detector/` so its blob is pushed once and cached thereafter — the property `5.3.7.3` measures as KPI 7 (identical media-layer digest across two builds).

- Confirm `ADA_ECU/.dockerignore` does **not** exclude `media/`, and **does** exclude `media/source/`. Check the pattern list rather than assuming; `5.2.7.1` wrote it.
- No code change: `VIDEO_CLIP_PATH` defaults to `/app/media/ego-b-occluding-c.mp4` (`12.3.2.1`), and the path and the stride are env, never literals (CLAUDE.md principle 5).
- **Add the measurement steps to the `ada-ecu-image` lane and record their output** in `plans/doc/phase3-ada-detector-run.md`: `docker image inspect -f '{{.Size}}'` before and after, the media layer's own size from `docker history`, and the layer digest. That job lives in `phase2-ci.yml`, which is in this subtask's write scope for those steps alone — the lane is not duplicated and its build step is not edited. Expect the delta to be ≈ 5.0 MB — H.264 is already compressed, so the layer's gzip gains ~0 %.

**Acceptance (all required):**

- `check_clip_spec.py` exits 0 on the committed clip, output recorded.
- The image-size delta, media-layer size and layer digest recorded in the run doc, read from the lane's log.
- CI lane `ada-ecu-image` green: `docker buildx build --platform linux/arm64 --provenance=false --sbom=false` succeeds against the registry tag `registry.hackathon-2.carsky.io/m1-ada-ecu:latest`, and a run step inside the built arm64 image finds `/app/media/ego-b-occluding-c.mp4` at exactly 5 261 876 bytes.
- Unit tests unaffected and still green.
- **Exactly one video file remains in git history and no new one is added.**

**Dependencies:** after Phase 2 `5.2.7.1` (the Dockerfile must exist) + `12.2.9.1` (the preflight). Unblocks `5.3.6.1`, `5.3.7.3`. **Commit:** `[12.3.7.2] feat: bake the demo clip into the ADA image`

### [ ] `5.3.7.3` — Record the media-layer size, cache-stability and push KPIs *(AI — car-sky)*

**Objective:** close [m1-video-source-and-ivi-dashcam.md §7 KPIs 7 and 8](../requirements/m1-video-source-and-ivi-dashcam.md) — one number, read twice: what the clip costs to upload. The digest comparison and the push report are the two readings, not two deliverables.

**Who performs which half, within this one brief:** the two builds and the `docker history` comparison run in the `ada-ecu-image` lane, and the implementing agent reads the two runs' logs. The registry credential, the push and the registry's blob report are [[car-sky]]'s, per [carsky-deploy-preflight](../.claude/skills/carsky-deploy-preflight/SKILL.md).

**Scope — steps in order:**

1. Run `ada-ecu-image` twice with no change under `ADA_ECU/media/`.
2. Read `docker history` from both runs and assert the media layer digest is identical across them, and its size is ≤ 60 MB (KPI 7). A changed digest means the layer ordering is wrong, and `12.3.7.2`'s Dockerfile edit is the fix.
3. [[car-sky]]: record the first push of `registry.hackathon-2.carsky.io/m1-ada-ecu:latest` — the media blob's transfer time and the measured link rate.
4. [[car-sky]]: push again and confirm the registry reports that blob already present, 0 bytes transferred (KPI 8) — Zot skips blobs it already holds.
5. Record both readings in `plans/doc/phase3-ada-detector-run.md`. No code change; doc-only commit.

**Acceptance:** the two digests match and are recorded; the second push reports 0 bytes for the media blob; both numbers written to the run doc.

**Dependencies:** after `12.3.7.2` + `5.3.6.1`. **Commit:** `[5.3.7.3] docs: record the ADA media-layer size and push-cache KPIs`

---

## Execution order & parallelism

```
wheels     12.3.1.1                                   (runs before every other subtask - gates 12.3.2.5 and 5.3.6.1)
lanes      12.3.3.3                                   (guarded lane - lands before group 3.2's consumers)
modules    12.3.2.1 ──► 12.3.2.2 ∥ 12.3.2.3 ∥ 12.3.2.4 ∥ 12.3.2.8
                        12.3.2.5 (after 12.3.2.1 + 12.3.1.1)
                        12.3.2.6 (after 12.3.2.3 + 12.3.2.4)
                        12.3.2.7 (after 12.3.2.2 + 12.3.2.5 + 12.3.2.6 + 12.3.2.8)
artifacts  12.3.3.1 (after 12.3.1.1) ∥ 12.3.3.2 (after 12.3.2.2)
run lane   12.3.3.4 (after 12.3.1.1 + 12.3.2.7 + 12.3.3.1)
clip       12.3.7.2 (after phase-2 5.2.7.1 + 12.2.9.1)   - independent of the module lane
evidence   12.3.5.1 (after 12.3.2.6) ──► 12.3.5.4 (also needs 12.3.2.7 + 12.3.3.1)
           3.3.5.3 (after 12.3.2.7 + 12.3.3.1 + phase-2 13.2.8.2)
           12.3.4.3 (after 12.3.2.7 + 12.3.3.1 + 12.3.3.4) ──► 12.3.5.2 (also needs 12.3.5.1)
image      5.3.6.1 (after 12.3.1.1 + 12.3.2.7 + 12.3.3.1 + 12.3.7.2)
push/KPI   5.3.7.3 (after 12.3.7.2 + 5.3.6.1)
deployed   5.3.6.2 ∥ 22.3.6.3 (after 5.3.6.1 + 12.3.2.8 + phase-4 18.4.11.1 - log reads, not deploys)
```

**`12.3.3.1` gates every subtask that runs the real detector** — `12.3.3.4`, `12.3.4.3`, `12.3.5.2`, `12.3.5.4`, `3.3.5.3` and `5.3.6.1` all execute against `MODEL_PATH`, and that file reaches disk only when `12.3.3.1` commits it. The edges above are the dependency structure a parallel run must honour, not a suggestion the recommended order happens to satisfy.

**Recommended runtime order (single tree):** 12.3.1.1 → 12.3.3.3 → 12.3.2.1 → 12.3.2.2 → 12.3.2.3 → 12.3.2.4 → 12.3.2.8 → 12.3.3.2 → 12.3.3.1 → 12.3.2.5 → 12.3.2.6 → 12.3.2.7 → 12.3.3.4 → 12.3.7.2 → 12.3.5.1 → 12.3.5.4 → 3.3.5.3 → 12.3.4.3 → 12.3.5.2 → 5.3.6.1 → 5.3.7.3 → 5.3.6.2 → 22.3.6.3.

**Three `Human` steps and one external input gate this phase** (§ Execution labels): the branch creation and push, the workflow-run confirmations and artifact downloads, and the `yolo11n.pt` fetch `12.3.3.1` needs when the export host has no egress. The remaining external unknown is `12.3.1.1`'s wheel availability, which is taken first.

**Relative to Phase 4.** Fully parallel until `5.3.6.2`, which reads a log Phase 4's isolated Room produced. Phase 4 consumes the store, not the detector; the only shared files are `ADA_ECU/CMakeLists.txt` (C++ targets — this phase adds none) and `ADA_ECU/Dockerfile` (this phase adds three `COPY` lines, Phase 4 adds one for `capture.sh`). Sequence those edits, not the phases.

## Acceptance traceability

| Milestone Phase 3 box | Closed by |
|---|---|
| Detection log with per-frame objects and distance estimates (R12) | `12.3.5.2`, over the committed clip; modules `12.3.2.2`/`3`/`5`/`6`/`7`/`8` |
| Entries enter via the same R3 interface, `source = own_sensor`, mock retired | `3.3.5.3` · `12.3.2.6` (frozen binding) · Phase 2 `3.2.3.2` + `3.2.4.1` |
| **Zero detections labeled C** | `12.3.5.1` (the falsifiable check) · `12.3.5.4` (repeatable in CI) · D6's structural argument · the clip's content verdict in [the provenance record](../ADA_ECU/media/ego-b-occluding-c.source.md) |
| CPU-only, offline pace acceptable | `12.3.5.2` KPI 3 (CI runner) · `5.3.6.2` (deployed node) · `12.3.1.1` (CPU-only wheels) |
| *(no milestone box — § Open items item 12)* media layer ≤ 60 MB, digest stable; second push transfers 0 bytes | `5.3.7.3` |
| *(no milestone box)* the baked-in clip opens **on the deployed node** | `5.3.6.2` |
| *(no milestone box)* R20's detector half — sampled frames released at 1.0× wall time, K4 within ±2 % | `12.3.2.8` (the pacer) · `12.3.5.2` (the host measurement) · verified by Phase 4 `21.4.3.4` |
| *(no milestone box)* the detector warm-up `W` and the bench `start_delay_s` R22 rests on | `22.3.6.3`, consumed by Phase 1 `22.1.13.4` |
| *(phase task, no box)* R18 own-sensor evidence | `own_sensor_ingest` payloads from Phase 2 `18.2.2.3`, fed by this phase's real lines |

**Three of the four boxes close off-platform.** Only the deployed half of the CPU-pace box needs a Room, and it reads Phase 4's.

## Open items & flags (no Phase 3 subtask may silently close them)

| # | Item | Owner / closes at |
|---|---|---|
| 1 | **Resolved by `12.3.1.1`: `onnxruntime==1.28.0`, `opencv-python-headless==5.0.0.93`, `numpy==2.4.6` install as prebuilt cp311 aarch64 wheels on `python:3.11-slim`** — the pins in `detector/requirements.txt`. Original item: aarch64 wheel availability was unproven ([HLD decision D9](../documents/Design/ADA-ECU/ada-ecu-design-decisions.md#d9--deployment-shape)). Verified first by `12.3.1.1`. A red lane escalates — pin an older wheel, change the base image, or accept a QEMU source build with a raised timeout. Not an implementer's call | `12.3.1.1`, then [[project-architecture]] |
| 2 | **Planner-designated test/tool paths beyond the HLD's list**: `detector/tests/test_config.py`, `test_inference.py`, `test_main.py`; `ADA_ECU/tools/tests/test_check_zero_c.py`. Required by subtask discipline; HLD-consistent additions, not new design | [[project-architecture]] (ack) |
| 3 | **Resolved by `12.3.4.3`: retuned to `VEHICLE_WIDTH_M=2.6`, `CAMERA_HFOV_DEG=34.4`** ([run record](doc/phase3-ada-detector-run.md)) — the default constants started B inside the gate (22.3 m), the predicted coach bias. Original item: distance accuracy unvalidated ([HLD decision D6](../documents/Design/ADA-ECU/ada-ecu-design-decisions.md#d6--r12-detector-frame-source-seam-inference-distance-zero-c-evidence)). `12.3.4.3` is the retune, and it may only move `VEHICLE_WIDTH_M` / `CAMERA_HFOV_DEG`, never `GATE_ENTER_M` / `GATE_EXIT_M`. B is a coach, wider than the 1.8 m car default, so expect a systematic bias to correct | `12.3.4.3` |
| 4 | **Detector warm-up `W` (ONNX load + `VideoCapture` open) is unmeasured** — estimated 2–5 s, and it is the one measurement R22 cannot ship without: it sets the bench's `start_delay_s`, held to **−0.5 / +1.1 s** ([§6.6(g)](../requirements/m1-run-timing-and-event-triggering.md)). `12.3.5.2` produces the CI-runner figure; **`22.3.6.3` produces the deployed one, which is the value that ships**, and Phase 1 `22.1.13.4` writes it into `scenarios/default.yaml` | `12.3.5.2`, then `22.3.6.3` |
| 5 | **The clip is 10 s, and every long-run behaviour therefore depends on looping.** Accepted, with reasoning in [the provenance record](../ADA_ECU/media/ego-b-occluding-c.source.md). Two consequences no subtask may absorb silently: `FileFrameSource` must keep `frame_index` monotonic across loops (`12.3.2.2`), and the gap between the last emitted line of one clip pass and the first of the next must not exceed `TRACK_TIMEOUT_MS`, or ego's own B track expires between cycles. `12.3.5.2` measures that gap; if it is too long, the finding goes to [[project-architecture]] — it is not fixed by widening the timeout | `12.3.5.2` |
| 6 | **The detector's pacer is built because [HLD §4/§6/§12](../documents/Design/ADA-ECU/ada-ecu-hld.md#6-internal-components) and [D10](../documents/Design/ADA-ECU/ada-ecu-design-decisions.md#d10--clock-domains-and-stimulus-paced-against-clock_monotonic) designate it (`12.3.2.8`); the requirement number it serves is unratified.** [m1-run-timing-and-event-triggering.md §7](../requirements/m1-run-timing-and-event-triggering.md) defines R20 and §8(1) schedules R20/R21 behind this phase's acceptance. D10 states that pacing serves R20 on its own footing rather than the deferred IVI dashcam view, so building it pulls no deferred surface in; the dashcam view stays deferred ([milestone1_high_level_plan.md §6](../documents/Plan/milestone1_high_level_plan.md#6-deferred-to-later-milestones)) and no Phase 3 subtask may add clip-serving or an `exposedPorts` entry. **Trigger:** the user accepts or rejects R20, which fixes whether `12.3.2.8` and its K4 bound are traceable to a ratified requirement | **user** (accept/reject R20) |
| 7 | **The §3 clip-spec numbers are proposals**, and the duration row does not match the committed artifact. `12.2.9.2` sends the spec to FPT-Mentor for confirmation and states the deviation. A correction arriving late is absorbed by **raising the stride**, never by changing the model or the gate | user / `12.2.9.2` |
| 8 | **The `container-file` API is not the deploy path, and no subtask may make it one.** [m1-video-source-and-ivi-dashcam.md §5](../requirements/m1-video-source-and-ivi-dashcam.md) documents `POST /api/v1/deployments/:roomId/container-file/:nodeKey` as a real post-deploy file channel with no schema, no size limit and no example. It is sanctioned **only** as a rehearsal-time clip swap or log pull; **nothing deployed may differ from its image tag**, so no subtask depends on it and the clip reaches the node through `COPY media/` alone | recorded, no action |
| 9 | **No documented registry size ceiling.** [§10 item 1](../requirements/m1-video-source-and-ivi-dashcam.md) — a ~1.2 GB artifact is observed succeeding on the platform, so a ~30 MB image is unremarkable, but the number is unverified. `5.3.7.3`'s real `docker push` either holds or fails with the registry's error recorded | `5.3.7.3` |
| 10 | **The clip's provenance record attributes its ffmpeg encode to `12.3.7.2`**, and the encode landed under `12.3.7.1`; `12.3.7.2` is the image line alone. The plan is not the place to restate the encode, so the record's header line is what needs correcting | [[project-architecture]] |
| 11 | **The rejected-alternative rationale for plain-git binary tracking has no home.** Why Git LFS is not used for `models/yolo11n.onnx` and `media/ego-b-occluding-c.mp4` is design record, which belongs in `ADA_ECU/doc/` ([node-code-layout § Where a node's documents live](../.claude/rules/node-code-layout.md#where-a-nodes-documents-live)) rather than in this plan | [[project-architecture]] |
| 12 | **`5.3.7.3` closes no Phase 3 milestone box.** Its acceptance traces to [m1-video-source-and-ivi-dashcam.md §7](../requirements/m1-video-source-and-ivi-dashcam.md) KPIs 7–8 and is R5-traceable, but no box in [milestone1_high_level_plan.md §5](../documents/Plan/milestone1_high_level_plan.md#phase-3--object-detection-from-video-r12--runs--with-phase-4) covers it. Two resolutions: record it there as a fifth Phase 3 check, or move it to the phase that owns the image push | [[project-planner]] |

---

*Phase 3 = 7 task groups, 24 subtasks, every one `AI`; `5.3.6.2`, `5.3.7.3` and `22.3.6.3` go to [[car-sky]] and the rest to an implementation subagent. 21 are done (branch `feat/phase3-object-detection`); the three car-sky readings wait on Phase 4 `18.4.11.1`'s saved Room log and the registry push. Run evidence: [doc/phase3-ada-detector-run.md](doc/phase3-ada-detector-run.md).*
