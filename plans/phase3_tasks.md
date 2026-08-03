# Phase 3 — Object Detection from Video (R12): Full Task Breakdown

> **Authority & context:**
> - **Phase content:** [milestone1.md § Phase 3](milestone1.md#phase-3--object-detection-from-video-r12--runs--with-phase-4) — its four acceptance checkboxes are the phase output.
> - **Design:** [ada-ecu-hld.md](../ADA_ECU/doc/ada-ecu-hld.md) — §4 folder structure for every path, §6 env tables for every constant; **[D6](../ADA_ECU/doc/ada-ecu-design-decisions.md#d6--r12-detector-frame-source-seam-inference-distance-zero-c-evidence)** is this phase's design (frame-source seam, inference, distance, association, emission, zero-C evidence, model provenance) and **[D10](../ADA_ECU/doc/ada-ecu-design-decisions.md#d10--clock-domains-and-stimulus-paced-against-clock_monotonic)** adds the detector's real-time pacing.
> - **Video source:** [video-source-for-r12.md](../ADA_ECU/doc/research_notes/video-source-for-r12.md) §3 the clip spec and its KPIs; and **the clip's own record**, [ADA_ECU/media/ego-b-occluding-c.source.md](../ADA_ECU/media/ego-b-occluding-c.source.md) — provenance, licence, encode command, content verdict, and the accepted duration deviation.
> - **Requirements:** [m1-cooperative-awareness.md §2](../requirements/m1-cooperative-awareness.md) R3, R5, R12, R18 and §3(g) — referenced by number, never restated.
> - **Run timing:** [m1-run-timing-and-event-triggering.md](../requirements/m1-run-timing-and-event-triggering.md) — §6.2's detector timestamp ruling (`12.3.2.6`) and §3.3's warm-up budget (`12.3.5.2`).
> - **Phase 2 baseline (do not re-plan):** [phase2_tasks.md § Output](phase2_tasks.md#phase-2-overview) — the C++ core, the store, the R13 machine, the CRA seam, `src/observer/detector_reader` with its `DETECTOR_CMD` process contract, the image and the `ada-ecu-image` lane, `tools/check_clip_spec.py`.
> - **Rules:** [task-planning-conventions.md](../.claude/rules/task-planning-conventions.md); [node-code-layout.md](../.claude/rules/node-code-layout.md).
>
> **Task ID legend:** `X.3.Z.W` — X = requirement served · 3 = this phase · Z = task group · W = subtask position. IDs are stable; never renumber, never reuse a retired one.
>
> **Runs in parallel with Phase 4.** The two never call each other — they meet only at the R3 store: this phase writes `own_sensor` entries through the JSONL subprocess boundary, Phase 4 reads the store.

## Phase 3 overview

**Objective.** Replace Phase 2's JSONL fixture with real perception: a YOLO11n ONNX detector reads the committed clip, finds **B — the visible occluder**, estimates its range, and streams R3 JSONL on stdout into the same store through the same interface. Zero entries labelled C, proven rather than asserted.

**Input (must exist before start).** All present on `main` except the Phase 2 items:

- Phase 2 complete: `src/observer/detector_reader` (the `DETECTOR_CMD` + stdout-JSONL contract), `src/parser/r3_parser`, the store, `ada_ecu`, the ADA image and its CI lane, `ADA_ECU/tools/check_clip_spec.py`.
- Phase 0's frozen `ADA_ECU/detector/contracts/tracked_object.py` binding, `detector/requirements-dev.txt`, and `ADA_ECU/detector/tests/test_r3_roundtrip.py`.
- **The clip is in the repo.** `ADA_ECU/media/ego-b-occluding-c.mp4` — 1280×720, 20 fps CFR, H.264 High / yuv420p, 200 frames / 10.0 s, 5 261 876 bytes, no audio — with its provenance sidecar beside it. Licence: Pexels License, commercial use and modification permitted, attribution given anyway. **No subtask in this phase sources, cuts or re-encodes video.**

**Output (phase acceptance = the four milestone boxes):**

- [ ] Detection log over the provided clip with per-frame objects and distance estimates (R12) — closed by `12.3.5.2`.
- [ ] Entries enter the store via the same R3 interface as relayed entries, `source = own_sensor` — mock no longer required — closed by `3.3.5.3`.
- [ ] **Zero detections labeled C** — closed by `12.3.5.1` (`tools/check_zero_c.py`) + `12.3.5.4` (the CI lane that makes it repeatable) + the structural argument in [HLD D6](../ADA_ECU/doc/ada-ecu-design-decisions.md#d6--r12-detector-frame-source-seam-inference-distance-zero-c-evidence) + the clip's own content verdict.
- [ ] Runs CPU-only on the provided clip; offline pace acceptable — closed by `12.3.5.2` (measured effective inference rate ≥ 5 Hz, i.e. ≤ 200 ms per sampled frame) on the dev host, and `5.3.6.2` (the same measurement on the deployed node — [research note KPI 3](../ADA_ECU/doc/research_notes/video-source-for-r12.md#measurable-checks-kpis)).

**The clip is 10 s and that is by design.** B is the lead vehicle only across a 10 s window of the source; a longer run is obtained by **looping** (`DETECTOR_LOOP=true`, default), each loop reading as a fresh approach cycle with B re-appearing at ~60 m and closing again. Reasoning and the source's own geometry: [sidecar § The remaining deviation](../ADA_ECU/media/ego-b-occluding-c.source.md). Every duration-sensitive check in this phase and in Phase 4 is worded against a **looped** run, not a single pass.

**Suggested branch (suggestion only — creation is the user's call):** `feat/phase3-ada-detector`. One branch for the whole phase; subtasks commit onto it. It branches from Phase 2's branch (or from `main` once Phase 2 merges) — it needs `detector_reader` and the image, nothing from Phase 4.

### Execution labels

Identical to [phase2_tasks.md § Execution labels](phase2_tasks.md#execution-labels) — *agent* · *car-sky* · *Human*. **Phase 3 is 19 *agent* subtasks and 2 *car-sky* subtasks, with no *Human* row at all**: the clip it runs against is a committed input rather than a deliverable, and the deployed measurement reads off Phase 4's isolated Room rather than booking a Room of its own.

Two constraints specific to this phase:

- **Detector dependencies may not install on the dev host.** `onnxruntime` and `opencv-python-headless` have no guaranteed wheel for Windows-on-ARM. Every detector test must therefore run on CI (`ada-detector-tests`, `12.3.3.3`) and degrade to `pytest.importorskip` locally — the same skip-locally / run-on-CI pattern Phase 1 used for `test_encoder_golden.py`. A subtask whose tests only *skip* locally is not done until its CI lane is green.
- **Detector modules read env only through `detector/config.py`** (HLD §6). No `os.environ` outside that file.

### Subtask discipline

Identical to [phase2_tasks.md § Subtask discipline](phase2_tasks.md#subtask-discipline-applies-to-every-subtask-below) — not restated. **Nothing in this file is started** except `12.3.7.1`, which is done.

### Per-node build commands (cited in acceptance below)

| Area | Build + test command | Verified |
|---|---|---|
| `ADA_ECU/detector/` | `pip install -r ADA_ECU/detector/requirements.txt -r ADA_ECU/detector/requirements-dev.txt && python -m pytest ADA_ECU/detector/tests` | CI `ada-detector-tests` (`12.3.3.3`); local with skips |
| `ADA_ECU/` (C++ core) | `cmake -S ADA_ECU -B ADA_ECU/build && cmake --build ADA_ECU/build -j $(nproc) && ctest --test-dir ADA_ECU/build --output-on-failure` | CI `ada-core-build` |
| `ADA_ECU/tools/` | `python -m py_compile ADA_ECU/tools/<script>.py` | local + CI |
| ADA image | `docker buildx build --platform linux/arm64 --provenance=false --sbom=false -t m1-ada-ecu:latest ADA_ECU/` | CI `ada-ecu-image` |

### CI ruling for this phase

New lanes go in a new `.github/workflows/phase3-ci.yml` — *a lane belongs to the phase that created it* ([phase1-ci.yml](../.github/workflows/phase1-ci.yml) header). Three jobs: `ada-detector-wheels` (`12.3.1.1`), `ada-detector-tests` (`12.3.3.3`), `ada-zero-c` (`12.3.5.4`). `ada-core-build` and `python-tests` in phase0-ci.yml stay untouched; `ada-ecu-image` in phase2-ci.yml is reused, not duplicated.

---

## Task Group 3.1 — arm64 dependency de-risking (serves R12; HLD §11 item 6)

> The one unproven platform assumption in this phase, taken first. If `onnxruntime` has no aarch64 wheel for `python:3.11-slim`, the image build falls back to a source build inside QEMU — a multi-hour, possibly failing step that must not be discovered at Phase 3's end.

### [ ] `12.3.1.1` — `phase3-ci.yml` + lane `ada-detector-wheels` *(agent — **sequential-first**)*

**Objective:** prove, before any detector code exists, that `onnxruntime`, `opencv-python-headless` and `numpy` install as prebuilt wheels for `linux/arm64` on `python:3.11-slim`.

**Scope:**

- Create `.github/workflows/phase3-ci.yml` with the same `on:`/`concurrency:` block as [phase1-ci.yml](../.github/workflows/phase1-ci.yml) and a header comment naming what the file carries.
- Job `ada-detector-wheels`: qemu setup; `docker run --platform linux/arm64 python:3.11-slim` executing `pip install --only-binary=:all: onnxruntime opencv-python-headless numpy` followed by an import smoke (`import onnxruntime, cv2, numpy; print(versions)`).
- `--only-binary=:all:` is deliberate: the lane must **fail loudly** on a missing wheel rather than silently start a source build. On failure the lane prints the escalation note — options are pinning an older `onnxruntime` with an aarch64 wheel, switching the base to a distro whose apt carries the package, or accepting a source build with a raised timeout — and the choice goes to [[project-architecture]], not to the implementing subagent.
- `.github/workflows/` is explicitly in this subtask's write scope.

**Acceptance:** lane green on the pushed branch with the three versions printed; or lane red with the failing package named and the item escalated (§ Open items item 1). A red lane here is a **result**, not a failure of the subtask — but it blocks `12.3.2.5` and `5.3.6.1` until resolved.

**Dependencies:** none — first in the phase. **Commit:** `[12.3.1.1] chore: add the arm64 detector-wheel CI lane`

---

## Task Group 3.2 — Detector modules (serves R12; HLD D6)

> The Python subprocess. Paths from [HLD §4](../ADA_ECU/doc/ada-ecu-hld.md#4-folder-structure); every module is importable and unit-testable on its own, and none of them opens a socket or reads env directly.

### [ ] `12.3.2.1` — `detector/config.py` + `detector/requirements.txt` *(agent)*

**Objective:** the detector's **only** env reader, plus its runtime dependency manifest.

**Scope:**

- `detector/config.py`: a frozen dataclass loaded from env with the [HLD §6](../ADA_ECU/doc/ada-ecu-hld.md#6-internal-components) detector half and defaults — `VIDEO_CLIP_PATH` (`/app/media/ego-b-occluding-c.mp4`) · `DETECTOR_FRAME_STRIDE` (4) · `DETECTOR_LOOP` (true) · `MODEL_PATH` (`/app/models/yolo11n.onnx`) · `CONF_THRESHOLD` (0.35) · `IOU_THRESHOLD` (0.45) · `TRACK_IOU_MIN` (0.3) · `VEHICLE_WIDTH_M` (1.8) · `CAMERA_HFOV_DEG` (60). Injectable env mapping so tests never mutate `os.environ`. Validation: positive stride, thresholds in [0, 1], positive width, HFOV in (0, 180) — invalid value raises `ValueError` naming the variable, and `main.py` exits non-zero on it.
- `detector/requirements.txt`: `onnxruntime`, `opencv-python-headless`, `numpy`, each **pinned** to the versions proven by `12.3.1.1`. `requirements-dev.txt` (Phase 0) gains a leading `-r requirements.txt` so a dev install carries the runtime set.
- **Do not add real-time-pacing keys.** `DETECTOR_REALTIME_PACING`, `DETECTOR_CLIP_FPS` and `DETECTOR_START_DELAY_S` belong to R20, which is unratified and unplanned (§ Open items item 6). `DETECTOR_FRAME_STRIDE` is a decimation stride, **not** a rate, and "5 Hz effective" is an assumed CPU throughput, not an enforced one (report §2(d)). Design the loader so those three keys would be a table addition rather than a restructure — and add none of them.
- Test `detector/tests/test_config.py`: defaults when unset; each override; each rejection case.

**Acceptance:** pytest green on CI `ada-detector-tests`; no `os.environ` access anywhere else in `detector/`.

**Dependencies:** after `12.3.1.1` (pin versions to what installed). **Commit:** `[12.3.2.1] feat: add the detector config loader and runtime requirements`

### [ ] `12.3.2.2` — Frame-source seam `detector/frame_source.py` *(agent)*

**Objective:** the mandatory D6 seam — frame acquisition behind an interface, so a future CarSky `video` pin is one new implementation and touches nothing downstream ([research note §2 (a′)](../ADA_ECU/doc/research_notes/video-source-for-r12.md#why-a-is-rejected-for-m1)).

**Scope:**

- `Frame(index, timestamp_ms, image, width, height)` dataclass; `FrameSource` protocol with `iter_frames() -> Iterator[Frame]`.
- `FileFrameSource`: OpenCV `VideoCapture` on `VIDEO_CLIP_PATH`; yields every `DETECTOR_FRAME_STRIDE`-th frame; `timestamp_ms = frame_index / fps * 1000` (valid because the clip is 20 fps CFR); at EOF, re-opens from frame 0 when `DETECTOR_LOOP` is true, otherwise stops; a file that cannot be opened raises with the path named.
- **`frame_index` accumulates across loops and does not reset to 0.** The clip is 10 s / 200 frames and looping is the normal case, so a resetting index would make `timestamp_ms` sawtooth and destroy the monotonic clip-time stamp `12.3.2.6` emits as `measured`. Expose the within-clip index separately if a caller wants it.
- `SyntheticFrameSource`: generates N frames of a configurable size with no clip on disk — the CI path.
- Test `detector/tests/test_frame_source.py`: stride selects the expected indices; loop restarts the *file* while the yielded index keeps increasing; EOF without loop terminates; the synthetic source yields the declared count and shape; a missing file raises.
- **Real-time pacing is not in this subtask and is not planned** (§ Open items item 6). Keep `iter_frames()` a generator with no assumption that the consumer is faster than real time, so a pacing sleep would be one insertion rather than a redesign — and insert none.

**Acceptance:** pytest green on CI; no OpenCV call anywhere else in `detector/` except this file.

**Dependencies:** after `12.3.2.1`. **Commit:** `[12.3.2.2] feat: add the detector frame-source seam`

### [ ] `12.3.2.3` — Distance estimation `detector/distance.py` *(agent — parallel with 12.3.2.2)*

**Objective:** the pinhole known-width range and lateral offset (D6) — pure maths, no model, no frames.

**Scope:** `f_px = (frame_w / 2) / tan(CAMERA_HFOV_DEG / 2)` · `d = VEHICLE_WIDTH_M × f_px / bbox_w_px` · `y = (bbox_u_center − frame_w / 2) × d / f_px`. Inputs are parameters, not env reads. Zero or negative `bbox_w_px` returns `None` rather than dividing. Docstring states the rationale in one line and links D6 — the alternative estimators (ground-plane bbox-bottom, monocular depth) were rejected there and must not be re-litigated in code comments. Test `detector/tests/test_distance.py`: hand-computed values for at least three (bbox width, frame width, HFOV) triples; monotonicity — a wider bbox is always nearer; lateral sign convention (right of centre is +y); the degenerate-width guard.

**Acceptance:** pytest green locally **and** on CI (this module has no heavy dependency, so it must pass locally too).

**Dependencies:** after `12.3.2.1`. **Commit:** `[12.3.2.3] feat: add pinhole known-width distance estimation`

### [ ] `12.3.2.4` — Frame-to-frame association `detector/tracker.py` *(agent — parallel)*

**Objective:** stable `own:<n>` ids across sampled frames by greedy IoU matching (D6) — no tracking library.

**Scope:** greedy IoU match of the current frame's boxes against the previous **sampled** frame's; an id is held while IoU ≥ `TRACK_IOU_MIN`, otherwise a new id is minted from a monotonic counter; ids are `own:<n>` — the detector can never mint a `v2x:` id (the structural half of the zero-C argument, D6). No time-based prediction; one dominant occluder is the design target. **Expect several concurrent tracks, not one** — the clip carries adjacent-lane and oncoming traffic, which is expected and is not C ([sidecar § C is synthetic](../ADA_ECU/media/ego-b-occluding-c.source.md#c-is-synthetic--what-the-criterion-actually-constrains)). Test `detector/tests/test_tracker.py`: an id survives a small box drift across frames; a box jump below `TRACK_IOU_MIN` yields a new id; two boxes get distinct ids and do not swap; ids are never reused.

**Acceptance:** pytest green locally and on CI.

**Dependencies:** after `12.3.2.1`. **Commit:** `[12.3.2.4] feat: add greedy IoU frame-to-frame association`

### [ ] `12.3.2.5` — Inference `detector/inference.py` *(agent)*

**Objective:** the YOLO11n ONNX Runtime CPU session behind a swappable `Detector` protocol (D6, report §3(g)).

**Scope:** `Detector` protocol with `detect(image) -> list[Detection(bbox_xywh, score, coco_class)]`; `OnnxDetector` implementing it — ONNX Runtime **CPU** provider on `MODEL_PATH`, letterbox to 640×640 with the scale/pad recorded so boxes map back to source pixels, score floor `CONF_THRESHOLD`, NMS at `IOU_THRESHOLD`; COCO classes `car | truck | bus | motorcycle` collapse to the R3 `class: "vehicle"`, everything else dropped. A `FakeDetector` returning scripted boxes lives in the test file, not in `detector/`. Test `detector/tests/test_inference.py` (planner-designated path, § Open items item 2): letterbox round-trip — a box in letterboxed coordinates maps back to the expected source pixels; NMS suppresses an overlapping duplicate; the class map collapses the four vehicle classes and drops a person; the ONNX session test is `pytest.importorskip`-guarded and additionally skipped when `models/yolo11n.onnx` is absent.

**Acceptance:** pytest green on CI `ada-detector-tests` with the session test **executed** (the lane installs the wheels and the model is committed by `12.3.3.1`).

**Dependencies:** after `12.3.2.1` + `12.3.1.1`; the session test unskips once `12.3.3.1` lands. **Commit:** `[12.3.2.5] feat: add the YOLO11n ONNX inference stage`

### [ ] `12.3.2.6` — Emission `detector/emit.py` *(agent)*

**Objective:** one R3 JSONL line per detection per sampled frame, through the **frozen** Phase 0 binding (D6).

**Scope:** build `TrackedObject` via `detector/contracts/tracked_object.py` — `source: own_sensor` · `id: own:<n>` from the tracker · `class: vehicle` · `position {x: distance, y: lateral}` · `distance` · `speed` from Δdistance/Δt between consecutive sampled frames for the same id (**0 on a track's first frame**) · `confidence` from the detection score · `state: not_tracked` (the store is the sole `state` writer, D3) · `timestamps` per the ruling below. One compact JSON object per line to stdout, flushed per line. Test `detector/tests/test_emit_contract.py`: emitted lines **validate against the synced `ADA_ECU/contracts/r3-tracked-object.schema.json`** (loaded from disk); id and source conventions exact; first-frame speed is 0 and the second frame's speed matches the hand-computed Δd/Δt; no `v2x_relayed` line is producible.

**Timestamp ruling — [m1-run-timing-and-event-triggering.md §6.2](../requirements/m1-run-timing-and-event-triggering.md), not "all three from the frame timestamp".** For `own_sensor`: `measured` = the clip-time stamp `12.3.2.2` computes as `frame_index / fps * 1000`, monotonic across loops · `received` = the detector's **emit time**, `CLOCK_REALTIME` at the moment the line is written · `lastUpdated` = written by the store, not by the detector. The two must be distinguishable: the ratio of their advances is the only evidence that the detector runs at 1.0× wall time, so collapsing them to one value destroys it. Add a test case asserting `measured` and `received` advance independently across sampled frames.

**Acceptance:** pytest green locally and on CI; the schema is loaded, never restated.

**Dependencies:** after `12.3.2.3` + `12.3.2.4`. **Commit:** `[12.3.2.6] feat: emit R3 JSONL from the detector`

### [ ] `12.3.2.7` — Entrypoint `detector/main.py` *(agent)*

**Objective:** the ego side of the D2 process contract — argv, wiring, exit codes. Controller only, no rules.

**Scope:** load `config` → build the frame source (file by default, `--synthetic N` for CI) → `OnnxDetector` → per frame: detect → distance → track → emit; log startup and fatal errors to **stderr** so stdout carries only R3 JSONL (the reader parses every stdout line); documented exit codes (0 clean EOF, non-zero for missing clip, missing model, invalid config); SIGTERM exits cleanly and promptly. Test `detector/tests/test_main.py` (planner-designated path): a short synthetic run with a fake detector emits N valid JSONL lines on stdout and nothing else; a missing clip exits non-zero with the path named; SIGTERM terminates within a bounded time.

**Acceptance:** pytest green on CI; **stdout contains only JSONL** — asserted, because a stray print corrupts the process contract.

**Dependencies:** after `12.3.2.2` + `12.3.2.5` + `12.3.2.6`. **Commit:** `[12.3.2.7] feat: add the detector entrypoint`

---

## Task Group 3.3 — Model, CI fixture, detector lane (serves R12)

### [ ] `12.3.3.1` — `tools/export_yolo11n.py` + committed `models/yolo11n.onnx` *(agent)*

**Objective:** the one-off Ultralytics → ONNX export and its committed artifact (D6 model provenance).

**Scope:** `ADA_ECU/tools/export_yolo11n.py` performs the export (AGPL-3.0 accepted, report §4) with the input size and opset recorded in the script header; the resulting `ADA_ECU/models/yolo11n.onnx` (~10 MB) is committed. The repo's `.gitattributes` already carries `*.onnx binary`. The script **never runs in CI or in the image** — committing the artifact is what keeps the image build offline-reproducible and keeps Ultralytics out of the runtime dependency set. The script's Ultralytics import is not added to `requirements.txt`.

**Binary tracking is settled, not open**: plain git, committed once, no Git LFS — the same rule `media/ego-b-occluding-c.mp4` follows. ~15 MB of write-once binaries is within normal git limits, and LFS would add a remote-storage dependency and a new clone/CI failure mode days from the deadline.

**Acceptance:** the ONNX file loads in an ONNX Runtime CPU session and reports the expected input/output shapes (asserted by `12.3.2.5`'s session test, which unskips here); `python -m py_compile` on the exporter passes.

**Dependencies:** after `12.3.1.1`. **Commit:** `[12.3.3.1] feat: export and commit the YOLO11n ONNX model`

### [ ] `12.3.3.2` — CI video fixture `tools/make_sample_video.py` *(agent — parallel)*

**Objective:** the decoder smoke fixture, and **only** that ([research note §2 (c)](../ADA_ECU/doc/research_notes/video-source-for-r12.md#why-c-is-a-fallback-not-the-source)).

**Scope:** `ADA_ECU/tools/make_sample_video.py` writes a short MP4 to a path given on the command line — deterministic content, configurable size/fps/frame count, no committed output. The module docstring states in one line that it is a CI fixture and **must never be the demo source** — it writes flat grey rectangles, which a pretrained COCO detector will not classify as `car`, so a run against it produces zero detections and no R12 evidence. Generated files are `.gitignore`d.

**Acceptance:** `python -m py_compile` passes; the generated file opens through `FileFrameSource` and yields the declared frame count (asserted in `detector/tests/test_frame_source.py`, extended here).

**Dependencies:** after `12.3.2.2`. **Commit:** `[12.3.3.2] feat: add the CI sample-video fixture generator`

### [ ] `12.3.3.3` — Lane `ada-detector-tests` *(agent)*

**Objective:** the Linux verification lane for group 3.2 — the only place the detector suite runs unskipped.

**Scope:** job in `phase3-ci.yml`: checkout; `pip install -r ADA_ECU/detector/requirements.txt -r ADA_ECU/detector/requirements-dev.txt` with pip cache; `python -m pytest ADA_ECU/detector/tests -q`; **fail the job if any test skipped for a missing dependency** (a skip-count assertion) — a silently-skipping suite is the failure mode this lane exists to prevent. Job guarded skip-with-notice while `ADA_ECU/detector/requirements.txt` is absent.

**Acceptance:** workflow YAML valid; run-blocks `bash -n` clean; lane green with 0 dependency-skips once group 3.2 lands.

**Dependencies:** after `12.3.1.1` (same file); lands before its consumers. **Commit:** `[12.3.3.3] chore: add the ADA detector test CI lane`

---

## Task Group 3.4 — Distance calibration against the real clip (serves R12)

> The clip's content is judged in [the sidecar's § Content verdict](../ADA_ECU/media/ego-b-occluding-c.source.md) and is not re-judged here; its format gate and its image placement are `12.3.7.2`'s. This group owns the distance constants alone.

### [ ] `12.3.4.3` — Retune the distance constants against the clip *(agent)*

**Objective:** close [HLD decision D6](../ADA_ECU/doc/ada-ecu-design-decisions.md#d6--r12-detector-frame-source-seam-inference-distance-zero-c-evidence) — `VEHICLE_WIDTH_M` and `CAMERA_HFOV_DEG` are proposals with no calibration target until the detector runs on the real clip.

**Scope:**

- Run the detector over `ADA_ECU/media/ego-b-occluding-c.mp4`; extract the estimated range series for B; compare against the clip's recorded geometry — [the sidecar](../ADA_ECU/media/ego-b-occluding-c.source.md) states B is a white coach closing from roughly 60 m to roughly 10 m across the 10 s, present in every sampled frame.
- **B is the largest, most central, closest track** — not the only one. Adjacent-lane and oncoming vehicles are expected in the series; select B by that criterion and say in the record how it was selected.
- Required property is **monotonic, consistently-biased range**, not absolute accuracy: the approach must yield a decreasing series that crosses `GATE_ENTER_M` (30 m) exactly once per loop.
- If the series disagrees, retune **only** `VEHICLE_WIDTH_M` / `CAMERA_HFOV_DEG` — the new values are recorded in [HLD §6](../ADA_ECU/doc/ada-ecu-hld.md#6-internal-components) as the defaults and in `node-ada-ecu.md`. **Never retune the R13 gate** (D3/D6): the gate is a requirement value, the camera constants are estimates. The coach is wider than the 1.8 m car default, so a systematic under-read of range is the expected first finding.
- Record the before/after series summary in `plans/doc/phase3-ada-detector-run.md`.

**Acceptance:** the estimated-range series for B is monotonic through the approach and crosses the gate once per loop; the two constants' final values are committed in the config defaults and the node guide.

**Dependencies:** after `12.3.2.7`. **Commit:** `[12.3.4.3] fix: retune the pinhole distance constants against the demo clip`

---

## Task Group 3.5 — Zero-C evidence and store integration (serves R12, R3, R18)

### [ ] `12.3.5.1` — Zero-C check `tools/check_zero_c.py` *(agent)*

**Objective:** make the R19 zero-C claim **falsifiable** (D6) — the check exists so a pass means something, not because a failure is expected.

**Scope:**

- `ADA_ECU/tools/check_zero_c.py`, Python 3 stdlib; input = a detection log (R3 JSONL) and optionally the ADA `[EVT]` stream; fails the run when **any** of:
  1. an own-sensor entry claims `source: v2x_relayed`;
  2. an own-sensor entry carries a `v2x:` id namespace;
  3. an own-sensor track sits within `ZERO_C_RADIUS_M` (default 5, from env — no literal) of the relayed C position at the same timestamp.
- **Rule 3 is the spatial check and it is the only content-sensitive one. Do not add a rule asserting "the detector found nothing but B"** — adjacent-lane and oncoming vehicles are expected in this clip and such a rule would fail on correct footage ([sidecar § What this obliges downstream](../ADA_ECU/media/ego-b-occluding-c.source.md#what-this-obliges-downstream)).
- Non-zero exit naming the offending line and which rule fired; exit 0 with the counts examined, so a vacuous pass is visible.
- Test `ADA_ECU/tools/tests/test_check_zero_c.py` (planner-designated path): a clean log exits 0; one planted violation per rule exits 1 naming that rule; an empty log exits non-zero (nothing examined is not a pass).

**Acceptance:** `python -m py_compile` passes; the test passes locally and on CI.

**Dependencies:** after `12.3.2.6` (line shape). **Commit:** `[12.3.5.1] feat: add the R12 zero-C evidence check`

### [ ] `12.3.5.2` — Detector run over the clip: the R12 detection log *(agent)*

**Objective:** the R12 acceptance artifact — a detection log over the provided clip with per-frame objects and distance estimates, at an acceptable offline pace.

**Scope:**

- Run `detector/main.py` over the clip on CPU with `DETECTOR_LOOP=true` for **at least 60 s of wall time** — six loops of the 10 s clip — so the rate measurement has a window and the loop path is exercised in the same run. Capture stdout; commit a representative excerpt plus the summary into `plans/doc/phase3-ada-detector-run.md`, not the full log.
- Record the [research note KPIs](../ADA_ECU/doc/research_notes/video-source-for-r12.md#measurable-checks-kpis): KPI 2 — ≥ 99% of declared frames read with zero decode errors, across loops; KPI 3 — **effective inference rate ≥ 5 Hz, i.e. wall-clock ≤ 200 ms per sampled frame**, measured over the whole run; KPI 4 — ≥ 1 `class = vehicle`, `source = own_sensor` entry with a distance estimate for ≥ 90% of sampled frames; KPI 5 — `check_zero_c.py` exit 0 over the whole log.
- If KPI 3 fails, the remedy is raising `DETECTOR_FRAME_STRIDE`, never changing the model ([research note §5](../ADA_ECU/doc/research_notes/video-source-for-r12.md#5-requirement-mapping-and-flags)) — record the stride used.
- **Also record the detector warm-up time** — ONNX model load plus `VideoCapture` open, from process start to the first emitted R3 line. [m1-run-timing-and-event-triggering.md §9 open item 4](../requirements/m1-run-timing-and-event-triggering.md) has it estimated at 2–5 s and **unmeasured**, and §3.3 shows why it matters: against a 12.0 s bench lead-in, the ADA side needs warm-up plus `confirm_hits` at 5 Hz (0.6 s), leaving roughly 6.4 s of slack — the alignment tolerance the timing design rests on. **This run is where that number comes from.** One measurement, recorded in the run doc; the remedy if it exceeds the slack is the bench's start delay, not a change here (§ Open items item 6).
- **Also record the loop seam:** the wall-clock gap between the last line of one loop and the first of the next, and whether B's track id is re-minted or carried. A long re-open stall is what would expire ego's own B track between cycles, which is `13.4.11.3`'s own-sensor half in Phase 4.

**Acceptance:** all four KPIs, the measured warm-up and the loop-seam gap recorded with their values; this closes the R12 detection-log box and the CPU-only/offline-pace box at host level (`5.3.6.2` repeats KPI 3 on the deployed node).

**Dependencies:** after `12.3.2.7` + `12.3.4.3` + `12.3.5.1`. **Commit:** `[12.3.5.2] docs: record the R12 detection log and KPI measurements`

### [ ] `3.3.5.3` — Retire the mock: real detector into the real store *(agent)*

**Objective:** the second Phase 3 box — own-sensor entries enter the store through the **same** R3 interface as relayed entries, with the fixture no longer required.

**Scope:**

- No new module. Change the default `DETECTOR_CMD` path in use from the Phase 2 fixture (`cat …/own_sensor_mock.jsonl`) to `python3 /app/detector/main.py` — already the [HLD §6](../ADA_ECU/doc/ada-ecu-hld.md#6-internal-components) default, so this is a verification subtask plus whatever wiring defect it uncovers.
- Add a sibling arm to `13.2.8.2`'s loopback lane driving `ada_ecu` with the **real** detector over a synthetic frame source, asserting `own_sensor_ingest` events and at least one `track_transition` for an `own:<n>` id — proving the JSONL crosses the process boundary, parses through `r3_parser`, and lands in the store via the same `upsert` as `r2_parser`.
- The Phase 2 fixture stays committed as a test artifact — it is what `--expect-no-tracks` and the deterministic admission cases run against.

**Acceptance:** the lane arm is green and observes `own:<n>` tracks reaching `tracked` with `source = own_sensor`; the ADA build + ctest stay green.

**Dependencies:** after `12.3.2.7` + Phase 2 `13.2.8.2`. **Commit:** `[3.3.5.3] test: drive the store from the real detector subprocess`

### [ ] `12.3.5.4` — Lane `ada-zero-c` *(agent)*

**Objective:** make the zero-C check repeatable rather than a one-off run — it feeds R19, so it must not be a manual step.

**Scope:** job in `phase3-ci.yml`: install the detector requirements; run `detector/main.py` over the **committed clip** (present in every checkout) capturing stdout, falling back to `--synthetic` only when the clip is absent; run `python ADA_ECU/tools/check_zero_c.py` over it and fail the job on non-zero; assert a non-zero examined-line count so the lane cannot pass on an empty log.

**Acceptance:** lane green on the pushed branch with a non-zero examined count.

**Dependencies:** after `12.3.5.1` + `12.3.2.7` + `12.3.3.3` (same file). **Commit:** `[12.3.5.4] chore: add the zero-C evidence CI lane`

---

## Task Group 3.6 — Image and deployed measurement (serves R5)

### [ ] `5.3.6.1` — Extend the ADA image with the detector and model *(agent)*

**Objective:** the deployable image carries both processes (D9 image layout).

**Scope:** `ADA_ECU/Dockerfile` — add **exactly two** COPY lines, `COPY models/ /app/models/` and `COPY detector/ /app/detector/`, plus `RUN pip install --no-cache-dir -r /app/detector/requirements.txt`. **`COPY media/` is not this subtask's to write** — `12.3.7.2` owns that line and lands before this one; verify it is present and sits above the two added here, and do not duplicate or reorder it. **Layer order is load-bearing:** `COPY media/` then `COPY models/` — both rarely-changing, so their blobs are pushed once and cached — and `COPY detector/` last, because it changes every commit. `detector/tests/` and `requirements-dev.txt` stay out via `.dockerignore`. Image layout ends as `/app/ada_ecu`, `/app/entrypoint.sh`, `/app/detector/`, `/app/models/yolo11n.onnx`, `/app/media/ego-b-occluding-c.mp4`.

**Acceptance:** `ada-ecu-image` lane green; the lane's in-image run step starts `detector/main.py --synthetic` inside the pulled arm64 image and observes R3 JSONL on stdout — proving the wheels resolved for aarch64 in the real image, which is what [HLD decision D9](../ADA_ECU/doc/ada-ecu-design-decisions.md#d9--deployment-shape) asks for.

**Dependencies:** after `12.3.1.1` + `12.3.2.7` + `12.3.3.1` + `12.3.7.2` (which must have written `COPY media/` first). **Commit:** `[5.3.6.1] feat: add the detector and model to the ADA image`

### [ ] `5.3.6.2` — Measure the deployed inference rate off the isolated ADA Room *(car-sky)*

**Objective:** KPI 3 on the real 2-vCPU node rather than on a laptop — and the proof that the **baked-in clip opens on the deployed node**, which no local run can establish about the `COPY media/` delivery path.

**This subtask books no Room and performs no deploy.** It reads [phase4_tasks.md](phase4_tasks.md) group 4.11's already-saved `ada.log` from the isolated ADA Room — bridge + V2X bench mock + ADA + IVI sink mock. The detector needs no neighbour at all for this measurement, the ADA node's config is identical in every Room, and the account allows only two concurrent deployments; booking a second slot for a log read that another group has already taken would waste one. If group 4.11 has not run yet, this subtask waits for it rather than deploying.

**Scope — reading, entirely:**

- From `18.4.11.1`'s saved ADA node log: confirm `detector_spawn`, then a steady `own_sensor_ingest` stream. Measure the effective sampled-frame rate from the event timestamps — **≥ 5 Hz / ≤ 200 ms per sampled frame**.
- Record the clip path the node reports opening. `detector_spawn` followed by `own_sensor_ingest` on the deployed node is the proof that the baked-in clip opened there ([m1-video-source-and-ivi-dashcam.md §7 KPI 9](../requirements/m1-video-source-and-ivi-dashcam.md)).
- Also record the deployed warm-up time, which will be worse than `12.3.5.2`'s host figure, and the loop-seam gap.
- **If the rate falls short**, the remedy is raising `DETECTOR_FRAME_STRIDE` in the ADA node config and re-reading on the next Room — config only, no rebuild. Report the number whichever way it lands; a rate below KPI 3 is not only a Phase 3 miss, it is the Phase 4 failure mode where ego's own B track expires between updates and the composed geometry disappears ([deploy-ada-ecu-walkthrough.md §8.1 item 11](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#81-confirm-before-relying-on-these)).

**Acceptance:** the measured deployed rate, warm-up, loop-seam gap and reported clip path recorded in `plans/doc/phase3-ada-detector-run.md`, citing the Room and log the reading came from.

**Dependencies:** after `5.3.6.1` (so the deployed image carries the detector) + Phase 4 `18.4.11.1` (the log to read). **Commit:** `[5.3.6.2] docs: record the deployed ADA inference rate and clip-open evidence`

---

## Task Group 3.7 — The demo clip in the repo and in the image (serves R12, R5)

> This group puts the committed clip into the image and measures what that costs. The clip itself is an input (§ Input), not a deliverable of any subtask here.
>
> **C is synthetic and is never in the footage.** C exists only as a position the bench asserts over V2X, so "C never visible in any frame" holds by construction. What the clip must avoid is a *decoy* — a vehicle held in the ego lane beyond B, long enough to be admitted as an `own_sensor` track — and the sidecar records that it carries none. Adjacent-lane and oncoming traffic are not decoys and are present. The obligation this puts on the bench scenario is to **place C in the ego lane beyond B at every instant of the run**, which is `Scenario_Player/scenarios/*.yaml`'s to satisfy, not this phase's.

### [x] `12.3.7.1` — The demo clip, its provenance and the binary tracking rules *(agent)*

**Objective:** the clip, its provenance sidecar, and the rules that keep exactly one video file in git — `*.mp4 binary` / `*.onnx binary` in `.gitattributes`, `ADA_ECU/media/source/` in `.gitignore`.

**[The sidecar](../ADA_ECU/media/ego-b-occluding-c.source.md) is the authority, and no subtask may restate or contradict it:** the source and its URL, the Pexels License and the attribution string, the exact ffmpeg encode command, both SHA-256s, the content verdict per criterion, the § C is synthetic reasoning and its downstream obligations, the rejected candidates, and the accepted 10 s duration with looping as its remedy.

**Status:** done, commit `3d55d7b` — 22 candidates frame-inspected, one accepted; raw download gitignored and not committed; exactly one video file in git history.

### [ ] `12.3.7.2` — Bake the committed clip into the ADA image *(agent)*

**Objective:** the clip reaches the container the only way it can — inside the image. A Container Node has no volume, no bind mount and no host path ([research note §1](../ADA_ECU/doc/research_notes/video-source-for-r12.md#1-platform-finding--carsky-serves-no-camera-content)).

**Scope — the image only. No video file is created, moved, re-encoded or re-committed.**

- Run `python ADA_ECU/tools/check_clip_spec.py ADA_ECU/media/ego-b-occluding-c.mp4` (from `12.2.9.1`) and require **exit 0**. This is the format gate over the committed artifact; the content rows are the sidecar's and are not re-judged here.
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
- No code change: `VIDEO_CLIP_PATH` already defaults to `/app/media/ego-b-occluding-c.mp4` (`12.3.2.1`), and the path and the stride are env, never literals (CLAUDE.md principle 5).
- **Measure and record the image-size delta** in `plans/doc/phase3-ada-detector-run.md`: `docker image inspect -f '{{.Size}}'` before and after, the media layer's own size from `docker history`, and the layer digest. Expect the delta to be ≈ 5.0 MB — H.264 is already compressed, so the layer's gzip gains ~0%.

**Acceptance (all required):**

- `check_clip_spec.py` exits 0 on the committed clip, output recorded.
- The image-size delta, media-layer size and layer digest recorded in the run doc.
- `docker buildx build --platform linux/arm64 --provenance=false --sbom=false -t m1-ada-ecu:latest ADA_ECU/` succeeds and CI lane `ada-ecu-image` is green; a run step inside the built arm64 image finds `/app/media/ego-b-occluding-c.mp4` at exactly 5 261 876 bytes.
- Unit tests unaffected and still green — this subtask changes no code, and proving it changed no behaviour is part of being done.
- **Exactly one video file remains in git history and no new one is added.**

**Dependencies:** after Phase 2 `5.2.7.1` (the Dockerfile must exist) + `12.2.9.1` (the preflight). Unblocks `5.3.6.1`, `5.3.7.3`. **Commit:** `[12.3.7.2] feat: bake the demo clip into the ADA image`

### [ ] `5.3.7.3` — Record the media-layer size, cache-stability and push KPIs *(car-sky)*

**Objective:** close [m1-video-source-and-ivi-dashcam.md §7 KPIs 7 and 8](../requirements/m1-video-source-and-ivi-dashcam.md) — prove the clip costs one upload rather than one per push, which is the whole reason the `COPY media/` layer is ordered early.

**Scope:**

- Build the ADA image twice without touching `ADA_ECU/media/`; assert from `docker history` that the **media layer digest is identical** across the two builds and its size is **≤ 60 MB** (KPI 7). A changed digest means the layer ordering is wrong and `12.3.7.2`'s Dockerfile edit is the fix.
- `docker push registry.hackathon-2.carsky.io/m1-ada-ecu:latest` — time the first push and record the media blob's transfer time and the measured link rate. Push again and confirm the registry reports that blob **already present, 0 bytes transferred** (KPI 8) — Zot skips blobs it already holds.
- Record both in `plans/doc/phase3-ada-detector-run.md`. No code change; doc-only commit.

**Acceptance:** the two digests match and are recorded; the second push reports 0 bytes for the media blob; both numbers written to the run doc. Registry credential, tag and push are [[car-sky]]'s per [carsky-deploy-preflight](../.claude/skills/carsky-deploy-preflight/SKILL.md).

**Dependencies:** after `12.3.7.2` + `5.3.6.1`. **Commit:** `[5.3.7.3] docs: record the ADA media-layer size and push-cache KPIs`

---

## Execution order & parallelism

```
de-risk    12.3.1.1                                   (SEQUENTIAL-FIRST - gates 12.3.2.5 and 5.3.6.1)
CI-first   12.3.3.3                                   (guarded lane - lands before group 3.2's consumers)
modules    12.3.2.1 ──► 12.3.2.2 ∥ 12.3.2.3 ∥ 12.3.2.4
                        12.3.2.5 (after 12.3.2.1 + 12.3.1.1)
                        12.3.2.6 (after 12.3.2.3 + 12.3.2.4)
                        12.3.2.7 (after 12.3.2.2 + 12.3.2.5 + 12.3.2.6)
artifacts  12.3.3.1 (after 12.3.1.1) ∥ 12.3.3.2 (after 12.3.2.2)
clip       12.3.7.2 (after phase-2 5.2.7.1 + 12.2.9.1)   - independent of the module lane
evidence   12.3.5.1 (after 12.3.2.6) ──► 12.3.5.4 (also needs 12.3.2.7)
           3.3.5.3 (after 12.3.2.7 + phase-2 13.2.8.2)
           12.3.4.3 (after 12.3.2.7) ──► 12.3.5.2 (also needs 12.3.5.1)
image      5.3.6.1 (after 12.3.1.1 + 12.3.2.7 + 12.3.3.1 + 12.3.7.2)
push/KPI   5.3.7.3 (after 12.3.7.2 + 5.3.6.1)
deployed   5.3.6.2 (after 5.3.6.1 + phase-4 18.4.11.1 - a log read, not a deploy)
```

**Recommended runtime order (single tree):** 12.3.1.1 → 12.3.3.3 → 12.3.2.1 → 12.3.2.2 → 12.3.2.3 → 12.3.2.4 → 12.3.3.2 → 12.3.3.1 → 12.3.2.5 → 12.3.2.6 → 12.3.2.7 → 12.3.7.2 → 12.3.5.1 → 12.3.5.4 → 3.3.5.3 → 12.3.4.3 → 12.3.5.2 → 5.3.6.1 → 5.3.7.3 → 5.3.6.2.

**Nothing in this phase waits on a person or on an external input.** The single external unknown that remains is `12.3.1.1`'s wheel availability, which is taken first.

**Relative to Phase 4.** Fully parallel until `5.3.6.2`, which reads a log Phase 4's isolated Room produced. Phase 4 consumes the store, not the detector; the only shared files are `ADA_ECU/CMakeLists.txt` (C++ targets — this phase adds none) and `ADA_ECU/Dockerfile` (this phase adds three `COPY` lines, Phase 4 adds one for `capture.sh`). Sequence those edits, not the phases.

## Acceptance traceability

| Milestone Phase 3 box | Closed by |
|---|---|
| Detection log with per-frame objects and distance estimates (R12) | `12.3.5.2`, over the committed clip; modules `12.3.2.2`/`3`/`5`/`6`/`7` |
| Entries enter via the same R3 interface, `source = own_sensor`, mock retired | `3.3.5.3` · `12.3.2.6` (frozen binding) · Phase 2 `3.2.3.2` + `3.2.4.1` |
| **Zero detections labeled C** | `12.3.5.1` (the falsifiable check) · `12.3.5.4` (repeatable in CI) · D6's structural argument · the clip's content verdict in [the sidecar](../ADA_ECU/media/ego-b-occluding-c.source.md) |
| CPU-only, offline pace acceptable | `12.3.5.2` KPI 3 (host) · `5.3.6.2` (deployed node) · `12.3.1.1` (CPU-only wheels) |
| *(no milestone box)* media layer ≤ 60 MB, digest stable; second push transfers 0 bytes | `5.3.7.3` |
| *(no milestone box)* the baked-in clip opens **on the deployed node** | `5.3.6.2` |
| *(phase task, no box)* R18 own-sensor evidence | `own_sensor_ingest` payloads from Phase 2 `18.2.2.3`, fed by this phase's real lines |

**Three of the four boxes close off-platform.** Only the deployed half of the CPU-pace box needs a Room, and it reads Phase 4's.

## Open items & flags (no Phase 3 subtask may silently close them)

| # | Item | Owner / closes at |
|---|---|---|
| 1 | **`onnxruntime` / `opencv-python-headless` aarch64 wheel availability is unproven** ([HLD decision D9](../ADA_ECU/doc/ada-ecu-design-decisions.md#d9--deployment-shape)). De-risked first by `12.3.1.1`. A red lane escalates — pin an older wheel, change the base image, or accept a QEMU source build with a raised timeout. Not an implementer's call | `12.3.1.1`, then [[project-architecture]] |
| 2 | **Planner-designated test/tool paths beyond the HLD's list**: `detector/tests/test_config.py`, `test_inference.py`, `test_main.py`; `ADA_ECU/tools/tests/test_check_zero_c.py`. Required by subtask discipline; HLD-consistent additions, not new design | [[project-architecture]] (ack) |
| 3 | **Distance accuracy is unvalidated until the detector runs on the clip** ([HLD decision D6](../ADA_ECU/doc/ada-ecu-design-decisions.md#d6--r12-detector-frame-source-seam-inference-distance-zero-c-evidence)). `12.3.4.3` is the retune, and it may only move `VEHICLE_WIDTH_M` / `CAMERA_HFOV_DEG`, never `GATE_ENTER_M` / `GATE_EXIT_M`. B is a coach, wider than the 1.8 m car default, so expect a systematic bias to correct | `12.3.4.3` |
| 4 | **Detector warm-up (ONNX load + `VideoCapture` open) is unmeasured** — estimated 2–5 s against the ≈ 6.4 s slack of [§3.3](../requirements/m1-run-timing-and-event-triggering.md), and it is the term that consumes that slack. `12.3.5.2` produces the number on the host; `5.3.6.2` repeats it on the 2-vCPU node, where it will be worse | `12.3.5.2`, then `5.3.6.2` |
| 5 | **The clip is 10 s, and every long-run behaviour therefore depends on looping.** Accepted, with reasoning in [the sidecar](../ADA_ECU/media/ego-b-occluding-c.source.md). Two consequences no subtask may absorb silently: `FileFrameSource` must keep `frame_index` monotonic across loops (`12.3.2.2`), and the loop-seam stall must not exceed `TRACK_TIMEOUT_MS` or ego's own B track expires between cycles. `12.3.5.2` measures the seam; if it is too long, the finding goes to [[project-architecture]] — it is not fixed by widening the timeout | `12.3.5.2` |
| 6 | **R20's detector half is not planned and no subtask may add it.** [m1-run-timing-and-event-triggering.md §7](../requirements/m1-run-timing-and-event-triggering.md) proposes real-time pacing (`DETECTOR_REALTIME_PACING`, `DETECTOR_CLIP_FPS`, `DETECTOR_START_DELAY_S`) and §2(d) names the free-running detector the dominant timing error term — but §8(1) schedules R20/R21 **behind** this phase's acceptance, §8(3) makes it mandatory only if the deferred IVI dashcam view is accepted, and **the user has not accepted R20**. The dashcam view stays deferred ([milestone1.md §6](milestone1.md#6-deferred-to-later-milestones)), so no Phase 3 subtask may add clip-serving, an `exposedPorts` entry, or pacing | **user** (accept/reject R20) |
| 7 | **The §3 clip-spec numbers are proposals**, and the duration row does not match the committed artifact. `12.2.9.2` sends the spec to FPT-Mentor for confirmation and states the deviation. A correction arriving late is absorbed by **raising the stride**, never by changing the model or the gate | user / `12.2.9.2` |
| 8 | **The `container-file` API is not the deploy path, and no subtask may make it one.** [m1-video-source-and-ivi-dashcam.md §5](../requirements/m1-video-source-and-ivi-dashcam.md) documents `POST /api/v1/deployments/:roomId/container-file/:nodeKey` as a real post-deploy file channel with no schema, no size limit and no example. It is sanctioned **only** as a rehearsal-time clip swap or log pull; **nothing deployed may differ from its image tag**, so no subtask depends on it and the clip reaches the node through `COPY media/` alone | recorded, no action |
| 9 | **No documented registry size ceiling.** [§10 item 1](../requirements/m1-video-source-and-ivi-dashcam.md) — a ~1.2 GB artifact is observed succeeding on the platform, so a ~30 MB image is unremarkable, but the number is unverified. `5.3.7.3`'s real `docker push` is where it either holds or fails loudly | `5.3.7.3` |

---

*Phase 3 = 7 task groups, 21 subtasks — 19 *agent*, 2 *car-sky*, 0 *Human*. One subtask done (`12.3.7.1`); the rest not started. Retired IDs, never reused: `12.3.4.1`, `12.3.4.2`. Decomposed from [ada-ecu-design-decisions.md D6](../ADA_ECU/doc/ada-ecu-design-decisions.md#d6--r12-detector-frame-source-seam-inference-distance-zero-c-evidence), [video-source-for-r12.md](../ADA_ECU/doc/research_notes/video-source-for-r12.md), [the clip sidecar](../ADA_ECU/media/ego-b-occluding-c.source.md) and [milestone1.md § Phase 3](milestone1.md#phase-3--object-detection-from-video-r12--runs--with-phase-4).*
