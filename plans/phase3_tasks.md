# Phase 3 — Object Detection from Video (R12): Full Task Breakdown

> **Authority & context:**
> - **Phase content:** [milestone1.md § Phase 3](milestone1.md#phase-3--object-detection-from-video-r12--runs--with-phase-4) — its four acceptance checkboxes are the phase output.
> - **Design:** [phase2-4-ada-ecu-hld.md](../ADA_ECU/doc/phase2-4-ada-ecu-hld.md) (commit `093f6d6`) — **D6** is this phase's design (frame-source seam, inference, distance, association, emission, zero-C evidence, model provenance); §4 folder map for every path; §6 env table for every constant.
> - **Video source:** [video-source-for-r12.md](../ADA_ECU/doc/research_notes/video-source-for-r12.md) (commit `e4d64e7`) — §3 the clip spec and its KPIs, §4 the user deliverable, §6 the decision.
> - **Requirements:** [m1-cooperative-awareness.md §2](../requirements/m1-cooperative-awareness.md) R3, R5, R12, R18 and §3(g) — referenced by number, never restated.
> - **Run timing:** [m1-run-timing-and-event-triggering.md](../requirements/m1-run-timing-and-event-triggering.md) — §6.2's detector timestamp ruling (`12.3.2.6`), §3.3's warm-up budget (`12.3.5.2`), and R20's detector half, which §8(1) schedules **behind** this phase's acceptance and § Open items item 7 leaves unplanned.
> - **Phase 2 baseline (do not re-plan):** [phase2_tasks.md § Output](phase2_tasks.md#phase-2-overview) — the C++ core, the store, the R13 machine, the CRA seam, `src/observer/detector_reader` with its `DETECTOR_CMD` process contract, the image and the `ada-ecu-image` lane, `tools/check_clip_spec.py`.
> - **Rules:** [task-planning-conventions.md](../.claude/rules/task-planning-conventions.md); [node-code-layout.md](../.claude/rules/node-code-layout.md).
>
> **Task ID legend:** `X.3.Z.W` — X = requirement served · 3 = this phase · Z = task group · W = subtask position. IDs are stable; never renumber.
>
> **Runs in parallel with Phase 4.** The two never call each other — they meet only at the R3 store: this phase writes `own_sensor` entries through the JSONL subprocess boundary, Phase 4 reads the store.

## Phase 3 overview

**Objective.** Replace Phase 2's JSONL fixture with real perception: a YOLO11n ONNX detector reads the provided clip, finds **B — the visible occluder**, estimates its range, and streams R3 JSONL on stdout into the same store through the same interface. Zero entries labelled C, proven rather than asserted.

**Input (must exist before start):**

- Phase 2 complete: `src/observer/detector_reader` (the `DETECTOR_CMD` + stdout-JSONL contract), `src/parser/r3_parser`, the store, `ada_ecu`, the ADA image and its CI lane.
- Phase 0's frozen `ADA_ECU/detector/contracts/tracked_object.py` binding and `detector/requirements-dev.txt`, plus `ADA_ECU/detector/tests/test_r3_roundtrip.py`.
- `ADA_ECU/tools/check_clip_spec.py` from `12.2.9.1`.
- **The clip** — **no longer a blocking human deliverable as of 2026-08-02.** [m1-video-source-and-ivi-dashcam.md](../requirements/m1-video-source-and-ivi-dashcam.md) §5/§9 confirms the delivery mechanism (one `COPY media/` layer, `VIDEO_CLIP_PATH`, no volume, no `video` pin), and the user's 2026-08-02 direction makes **sourcing and post-producing the clip agent work** — new **task group 3.7** below. `12.2.9.3` in [phase2_tasks.md](phase2_tasks.md) stays open as the *preferred* source if the user already holds footage; group 3.7 is the path taken when they do not. Either one feeds `12.3.7.2`. Groups 3.1–3.3 and 3.6 need no clip and start immediately.

**Output (phase acceptance = the four milestone boxes):**

- [ ] Detection log over the provided clip with per-frame objects and distance estimates (R12) — closed by `12.3.5.2`. **Blocked on the clip**; the synthetic fixture cannot close it (§ Clip contingency).
- [ ] Entries enter the store via the same R3 interface as relayed entries, `source = own_sensor` — mock no longer required — closed by `3.3.5.3`.
- [ ] **Zero detections labeled C** — checked on the detection log — closed by `12.3.5.1` (`tools/check_zero_c.py`) + `12.3.5.4` (the CI lane that makes it repeatable) + the structural argument in [HLD D6](../ADA_ECU/doc/phase2-4-ada-ecu-hld.md#d6--r12-detector-frame-source-seam-inference-distance-zero-c-evidence).
- [ ] Runs CPU-only on the provided clip; offline pace acceptable — closed by `12.3.5.2` (measured effective inference rate ≥ 5 Hz, i.e. ≤ 200 ms per sampled frame) and `5.3.6.2` (the same measurement on the deployed node — [research note KPI 3](../ADA_ECU/doc/research_notes/video-source-for-r12.md#measurable-checks-kpis)).

**Suggested branch (suggestion only — creation is the user's call):** `feat/phase3-ada-detector`. One branch for the whole phase; subtasks commit onto it. It branches from Phase 2's branch (or from `main` once Phase 2 merges) — it needs `detector_reader` and the image, nothing from Phase 4.

### Execution split legend, subagent spec, subtask discipline

Identical to [phase2_tasks.md § Execution split legend](phase2_tasks.md#execution-split-legend) and § Subtask discipline — not restated. Two additions specific to this phase:

- **Detector dependencies may not install on the dev host.** `onnxruntime` and `opencv-python-headless` have no guaranteed wheel for Windows-on-ARM. Every detector test must therefore run on CI (`ada-detector-tests`, `12.3.3.3`) and degrade to `pytest.importorskip` locally — the same skip-locally / run-on-CI pattern Phase 1 used for `test_encoder_golden.py`. A subtask whose tests only *skip* locally is not done until its CI lane is green.
- **Detector modules read env only through `detector/config.py`** (HLD §6). No `os.environ` outside that file.

### Per-node build commands (cited in acceptance below)

| Area | Build + test command | Verified |
|---|---|---|
| `ADA_ECU/detector/` | `pip install -r ADA_ECU/detector/requirements.txt -r ADA_ECU/detector/requirements-dev.txt && python -m pytest ADA_ECU/detector/tests` | CI `ada-detector-tests` (`12.3.3.3`); local with skips |
| `ADA_ECU/` (C++ core) | `cmake -S ADA_ECU -B ADA_ECU/build && cmake --build ADA_ECU/build -j $(nproc) && ctest --test-dir ADA_ECU/build --output-on-failure` | CI `ada-core-build` |
| `ADA_ECU/tools/` | `python -m py_compile ADA_ECU/tools/<script>.py` | local + CI |
| ADA image | `docker buildx build --platform linux/arm64 --provenance=false --sbom=false -t m1-ada-ecu:latest ADA_ECU/` | CI `ada-ecu-image` |

### CI ruling for this phase

New lanes go in a new `.github/workflows/phase3-ci.yml` — *a lane belongs to the phase that created it* ([phase1-ci.yml](../.github/workflows/phase1-ci.yml) header). Three jobs: `ada-detector-wheels` (`12.3.1.1`), `ada-detector-tests` (`12.3.3.3`), `ada-zero-c` (`12.3.5.4`). `ada-core-build` and `python-tests` in phase0-ci.yml stay untouched; `ada-ecu-image` in phase2-ci.yml is reused, not duplicated.

### Clip contingency — what Phase 3 develops against if no clip arrives

Stated once, referenced by every clip-gated subtask.

- **Development is not blocked.** `SyntheticFrameSource` (D6) and `tools/make_sample_video.py` drive every module, every unit test and every CI lane. Groups 3.1, 3.2, 3.3 and 3.6 complete with no clip in the repo.
- **Evidence is blocked.** `tools/make_sample_video.py` writes flat grey rectangles labelled "B" — a **decoder-and-contract smoke fixture**. A pretrained COCO detector will not classify a labelled rectangle as `car`, so a synthetic run produces **zero detections**, which is not a detection log with per-frame objects and distance estimates.
- **Forfeited without a real clip:** R12's first acceptance box; the distance-constant validation ([HLD §11 item 3](../ADA_ECU/doc/phase2-4-ada-ecu-hld.md#11-open-items-and-flags)); the ≥ 5 Hz KPI under real decode + inference load; and R19's zero-C claim as *measured* evidence rather than an argument from construction.
- **Escalation, not absorption:** the clip is now agent-sourced (group 3.7), so "no clip" means "no acceptably-licensed footage satisfying the content rows was found", not "the user has not sent one". In that case the plan does not quietly substitute the fixture — `12.3.7.1` reports the three closest candidates and their failing rows to the user against the 2026-08-08 deadline, and `12.3.4.1` is marked blocked.

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

> The Python subprocess. Paths from [HLD §4](../ADA_ECU/doc/phase2-4-ada-ecu-hld.md#4-folder-structure-map--file-location-designations); every module is importable and unit-testable on its own, and none of them opens a socket or reads env directly.

### [ ] `12.3.2.1` — `detector/config.py` + `detector/requirements.txt` *(agent)*

**Objective:** the detector's **only** env reader, plus its runtime dependency manifest.

**Scope:**

- `detector/config.py`: a frozen dataclass loaded from env with the [HLD §6](../ADA_ECU/doc/phase2-4-ada-ecu-hld.md#6-configuration--no-hardcoded-tunables) detector half and defaults — `VIDEO_CLIP_PATH` (`/app/media/ego-b-occluding-c.mp4`) · `DETECTOR_FRAME_STRIDE` (4) · `DETECTOR_LOOP` (true) · `MODEL_PATH` (`/app/models/yolo11n.onnx`) · `CONF_THRESHOLD` (0.35) · `IOU_THRESHOLD` (0.45) · `TRACK_IOU_MIN` (0.3) · `VEHICLE_WIDTH_M` (1.8) · `CAMERA_HFOV_DEG` (60). Injectable env mapping so tests never mutate `os.environ`. Validation: positive stride, thresholds in [0, 1], positive width, HFOV in (0, 180) — invalid value raises `ValueError` naming the variable, and `main.py` exits non-zero on it.
- `detector/requirements.txt`: `onnxruntime`, `opencv-python-headless`, `numpy`, each **pinned** to the versions proven by `12.3.1.1`. `requirements-dev.txt` (Phase 0) gains a leading `-r requirements.txt` so a dev install carries the runtime set.
- **Not in this subtask, but the table will grow:** [m1-run-timing-and-event-triggering.md §6.1](../requirements/m1-run-timing-and-event-triggering.md) adds `DETECTOR_REALTIME_PACING` (default `true`), `DETECTOR_CLIP_FPS` (default from `CAP_PROP_FPS`) and `DETECTOR_START_DELAY_S` (default 0.0) as R20's detector half. **They are not planned in this phase** (§ Open items item 7) — do not add them speculatively, and do not treat `DETECTOR_FRAME_STRIDE` as a rate: it is a decimation stride, and "5 Hz effective" is an assumed CPU throughput, not an enforced one (report §2(d)). Design the loader so the three keys are a table addition, not a restructure.
- Test `detector/tests/test_config.py`: defaults when unset; each override; each rejection case.

**Acceptance:** pytest green on CI `ada-detector-tests`; no `os.environ` access anywhere else in `detector/`.

**Dependencies:** after `12.3.1.1` (pin versions to what installed). **Commit:** `[12.3.2.1] feat: add the detector config loader and runtime requirements`

### [ ] `12.3.2.2` — Frame-source seam `detector/frame_source.py` *(agent)*

**Objective:** the mandatory D6 seam — frame acquisition behind an interface, so a future CarSky `video` pin is one new implementation and touches nothing downstream ([research note §2 (a′)](../ADA_ECU/doc/research_notes/video-source-for-r12.md#why-a-is-rejected-for-m1)).

**Scope:**

- `Frame(index, timestamp_ms, image, width, height)` dataclass; `FrameSource` protocol with `iter_frames() -> Iterator[Frame]`.
- `FileFrameSource`: OpenCV `VideoCapture` on `VIDEO_CLIP_PATH`; yields every `DETECTOR_FRAME_STRIDE`-th frame; `timestamp_ms = frame_index / fps * 1000` (valid because the spec fixes constant frame rate); at EOF, re-opens from frame 0 when `DETECTOR_LOOP` is true, otherwise stops; a file that cannot be opened raises with the path named.
- `SyntheticFrameSource`: generates N frames of a configurable size with no clip on disk — the CI path and the no-clip development path.
- Test `detector/tests/test_frame_source.py`: stride selects the expected indices; loop restarts the index sequence; EOF without loop terminates; the synthetic source yields the declared count and shape; a missing file raises.
- **Real-time pacing is where it will land, and it is not in this subtask.** [m1-run-timing-and-event-triggering.md §2(d)](../requirements/m1-run-timing-and-event-triggering.md) names the free-running detector the dominant error term — a 60 s clip consumed in whatever wall time the CPU takes. R20's fix is a `CLOCK_MONOTONIC`-deadline sleep between yielded frames, gated by `DETECTOR_REALTIME_PACING`; ~2 h *inside* this file, not planned here (§ Open items item 7). Keep `iter_frames()` a generator with no assumption that the consumer is faster than real time, so the pacing sleep is one insertion rather than a redesign.

**Acceptance:** pytest green on CI; no OpenCV call anywhere else in `detector/` except this file.

**Dependencies:** after `12.3.2.1`. **Commit:** `[12.3.2.2] feat: add the detector frame-source seam`

### [ ] `12.3.2.3` — Distance estimation `detector/distance.py` *(agent — parallel with 12.3.2.2)*

**Objective:** the pinhole known-width range and lateral offset (D6) — pure maths, no model, no frames.

**Scope:** `f_px = (frame_w / 2) / tan(CAMERA_HFOV_DEG / 2)` · `d = VEHICLE_WIDTH_M × f_px / bbox_w_px` · `y = (bbox_u_center − frame_w / 2) × d / f_px`. Inputs are parameters, not env reads. Zero or negative `bbox_w_px` returns `None` rather than dividing. Docstring states the rationale in one line and links D6 — the alternative estimators (ground-plane bbox-bottom, monocular depth) were rejected there and must not be re-litigated in code comments. Test `detector/tests/test_distance.py`: hand-computed values for at least three (bbox width, frame width, HFOV) triples; monotonicity — a wider bbox is always nearer; lateral sign convention (right of centre is +y); the degenerate-width guard.

**Acceptance:** pytest green locally **and** on CI (this module has no heavy dependency, so it must pass locally too).

**Dependencies:** after `12.3.2.1`. **Commit:** `[12.3.2.3] feat: add pinhole known-width distance estimation`

### [ ] `12.3.2.4` — Frame-to-frame association `detector/tracker.py` *(agent — parallel)*

**Objective:** stable `own:<n>` ids across sampled frames by greedy IoU matching (D6) — no tracking library.

**Scope:** greedy IoU match of the current frame's boxes against the previous **sampled** frame's; an id is held while IoU ≥ `TRACK_IOU_MIN`, otherwise a new id is minted from a monotonic counter; ids are `own:<n>` — the detector can never mint a `v2x:` id (the structural half of the zero-C argument, D6). No time-based prediction; one dominant occluder is the design target. Test `detector/tests/test_tracker.py`: an id survives a small box drift across frames; a box jump below `TRACK_IOU_MIN` yields a new id; two boxes get distinct ids and do not swap; ids are never reused.

**Acceptance:** pytest green locally and on CI.

**Dependencies:** after `12.3.2.1`. **Commit:** `[12.3.2.4] feat: add greedy IoU frame-to-frame association`

### [ ] `12.3.2.5` — Inference `detector/inference.py` *(agent)*

**Objective:** the YOLO11n ONNX Runtime CPU session behind a swappable `Detector` protocol (D6, report §3(g)).

**Scope:** `Detector` protocol with `detect(image) -> list[Detection(bbox_xywh, score, coco_class)]`; `OnnxDetector` implementing it — ONNX Runtime **CPU** provider on `MODEL_PATH`, letterbox to 640×640 with the scale/pad recorded so boxes map back to source pixels, score floor `CONF_THRESHOLD`, NMS at `IOU_THRESHOLD`; COCO classes `car | truck | bus | motorcycle` collapse to the R3 `class: "vehicle"`, everything else dropped. A `FakeDetector` returning scripted boxes lives in the test file, not in `detector/`. Test `detector/tests/test_inference.py` (planner-designated path, § Open items item 3): letterbox round-trip — a box in letterboxed coordinates maps back to the expected source pixels; NMS suppresses an overlapping duplicate; the class map collapses the four vehicle classes and drops a person; the ONNX session test is `pytest.importorskip`-guarded and additionally skipped when `models/yolo11n.onnx` is absent.

**Acceptance:** pytest green on CI `ada-detector-tests` with the session test **executed** (the lane installs the wheels and the model is committed by `12.3.3.1`).

**Dependencies:** after `12.3.2.1` + `12.3.1.1`; the session test unskips once `12.3.3.1` lands. **Commit:** `[12.3.2.5] feat: add the YOLO11n ONNX inference stage`

### [ ] `12.3.2.6` — Emission `detector/emit.py` *(agent)*

**Objective:** one R3 JSONL line per detection per sampled frame, through the **frozen** Phase 0 binding (D6).

**Scope:** build `TrackedObject` via `detector/contracts/tracked_object.py` — `source: own_sensor` · `id: own:<n>` from the tracker · `class: vehicle` · `position {x: distance, y: lateral}` · `distance` · `speed` from Δdistance/Δt between consecutive sampled frames for the same id (**0 on a track's first frame**) · `confidence` from the detection score · `state: not_tracked` (the store is the sole `state` writer, D3) · `timestamps {measured, received, lastUpdated}` per the ruling below. One compact JSON object per line to stdout, flushed per line. Test `detector/tests/test_emit_contract.py`: emitted lines **validate against the synced `ADA_ECU/contracts/r3-tracked-object.schema.json`** (loaded from disk); id and source conventions exact; first-frame speed is 0 and the second frame's speed matches the hand-computed Δd/Δt; no `v2x_relayed` line is producible.

**Timestamp ruling — [m1-run-timing-and-event-triggering.md §6.2](../requirements/m1-run-timing-and-event-triggering.md), not "all three from the frame timestamp".** For `own_sensor`: `measured` = the detector's stamp **at frame capture** (the clip-time stamp `12.3.2.2` computes as `frame_index / fps * 1000`) · `received` = the detector's **emit time**, `CLOCK_REALTIME` at the moment the line is written · `lastUpdated` = written by the store, not by the detector. The two must be distinguishable: **K4 of §6.4 is exactly the ratio of their advances**, so collapsing them to one value destroys the only evidence that the detector runs at 1.0× wall time. Add a test case asserting `measured` and `received` advance independently across sampled frames.

**Acceptance:** pytest green locally and on CI; the schema is loaded, never restated.

**Dependencies:** after `12.3.2.3` + `12.3.2.4`. **Commit:** `[12.3.2.6] feat: emit R3 JSONL from the detector`

### [ ] `12.3.2.7` — Entrypoint `detector/main.py` *(agent)*

**Objective:** the ego side of the D2 process contract — argv, wiring, exit codes. Controller only, no rules.

**Scope:** load `config` → build the frame source (file by default, `--synthetic N` for CI) → `OnnxDetector` → per frame: detect → distance → track → emit; log startup and fatal errors to **stderr** so stdout carries only R3 JSONL (the reader parses every stdout line); documented exit codes (0 clean EOF, non-zero for missing clip, missing model, invalid config); SIGTERM exits cleanly and promptly. Test `detector/tests/test_main.py` (planner-designated path): a short synthetic run with a fake detector emits N valid JSONL lines on stdout and nothing else; a missing clip exits non-zero with the path named; SIGTERM terminates within a bounded time.

**Acceptance:** pytest green on CI; **stdout contains only JSONL** — asserted, because a stray print corrupts the process contract.

**Dependencies:** after `12.3.2.2` + `12.3.2.5` + `12.3.2.6`. **Commit:** `[12.3.2.7] feat: add the detector entrypoint`

---

## Task Group 3.3 — Model, CI fixture, detector lane (serves R12)

### [ ] `12.3.3.1` — `tools/export_yolo11n.py` + committed `models/yolo11n.onnx` *(agent — **needs the § Open items item 2 decision**)*

**Objective:** the one-off Ultralytics → ONNX export and its committed artifact (D6 model provenance).

**Scope:** `ADA_ECU/tools/export_yolo11n.py` performs the export (AGPL-3.0 accepted, report §4) with the input size and opset recorded in the script header; the resulting `ADA_ECU/models/yolo11n.onnx` (~10 MB) is committed with a `.gitattributes` binary marker. The script **never runs in CI or in the image** — committing the artifact is what keeps the image build offline-reproducible and keeps Ultralytics out of the runtime dependency set. The script's Ultralytics import is not added to `requirements.txt`.

**Acceptance:** the ONNX file loads in an ONNX Runtime CPU session and reports the expected input/output shapes (asserted by `12.3.2.5`'s session test, which unskips here); `python -m py_compile` on the exporter passes.

**Dependencies:** after `12.3.1.1`; blocked on the repo-size decision (§ Open items item 2). **Commit:** `[12.3.3.1] feat: export and commit the YOLO11n ONNX model`

### [ ] `12.3.3.2` — CI video fixture `tools/make_sample_video.py` *(agent — parallel)*

**Objective:** the decoder smoke fixture, and **only** that ([research note §2 (c)](../ADA_ECU/doc/research_notes/video-source-for-r12.md#why-c-is-a-fallback-not-the-source)).

**Scope:** `ADA_ECU/tools/make_sample_video.py` writes a short MP4 to a path given on the command line — deterministic content, configurable size/fps/frame count, no committed output. The module docstring states in one line that it is a CI fixture and **must never be the demo source**, and links the research note. Generated files are `.gitignore`d.

**Acceptance:** `python -m py_compile` passes; the generated file opens through `FileFrameSource` and yields the declared frame count (asserted in `detector/tests/test_frame_source.py`, extended here).

**Dependencies:** after `12.3.2.2`. **Commit:** `[12.3.3.2] feat: add the CI sample-video fixture generator`

### [ ] `12.3.3.3` — Lane `ada-detector-tests` *(agent)*

**Objective:** the Linux verification lane for group 3.2 — the only place the detector suite runs unskipped.

**Scope:** job in `phase3-ci.yml`: checkout; `pip install -r ADA_ECU/detector/requirements.txt -r ADA_ECU/detector/requirements-dev.txt` with pip cache; `python -m pytest ADA_ECU/detector/tests -q`; **fail the job if any test skipped for a missing dependency** (a skip-count assertion) — a silently-skipping suite is the failure mode this lane exists to prevent. Job guarded skip-with-notice while `ADA_ECU/detector/requirements.txt` is absent.

**Acceptance:** workflow YAML valid; run-blocks `bash -n` clean; lane green with 0 dependency-skips once group 3.2 lands.

**Dependencies:** after `12.3.1.1` (same file); lands before its consumers. **Commit:** `[12.3.3.3] chore: add the ADA detector test CI lane`

---

## Task Group 3.4 — Clip intake and calibration (serves R12) — **unblocked by group 3.7**

> Written when the clip was a human deliverable. As of 2026-08-02 its input arrives from **group 3.7** (agent-sourced and post-produced) or from `12.2.9.3` (user-supplied), whichever lands first. `12.3.4.2` is **superseded by `12.3.7.2`** — see its entry.

### [ ] `12.3.4.1` — Validate the delivered clip *(agent — after `12.3.7.1`, or after `12.2.9.3` if the user supplies footage)*

**Objective:** a pass/fail verdict on the delivered file before it enters the repo.

**Scope:** run `python ADA_ECU/tools/check_clip_spec.py <delivered file>`; record the verdict (every attribute, actual vs expected) in `plans/doc/phase3-ada-detector-run.md`. If a **format** attribute fails, re-encode in one `ffmpeg` command to the [§3 spec](../ADA_ECU/doc/research_notes/video-source-for-r12.md#3-video-input-spec-to-build-phase-3-against) and record the exact command; if a **content** row fails (B not occluding at 10–40 m in ≥ 90% of frames, or **C visible in any frame**), the clip is rejected back to the user — **no re-encode fixes content, and a clip containing C invalidates R19 outright**.

**Acceptance:** preflight exit 0 on the final file, with the report recorded. Doc-only commit.

**Dependencies:** after `12.2.9.1`, and after `12.3.7.1` (agent-sourced candidate) or `12.2.9.3` (user-supplied file). **Commit:** `[12.3.4.1] docs: record the delivered clip preflight verdict`

### `12.3.4.2` — ~~Commit the clip and wire it into the image~~ — **SUPERSEDED by `12.3.7.2` (2026-08-02)**

> **Do not implement.** Its whole scope — the committed file at `ADA_ECU/media/ego-b-occluding-c.mp4`, the `.gitattributes` binary marker, `COPY media/ /app/media/`, the `.dockerignore` check — is carried by `12.3.7.2`, which additionally owns the ffmpeg post-production that produces the file and the image-size/layer evidence required by [m1-video-source-and-ivi-dashcam.md §5](../requirements/m1-video-source-and-ivi-dashcam.md). Splitting "encode it" from "land it" would have produced a repo state with an off-spec binary committed and then replaced — exactly what §5 warns against ("git keeps every blob forever… commit exactly one final file"). The ID is retired in place, never renumbered or reused; every dependency that named `12.3.4.2` now reads `12.3.7.2`.

**Original scope, retained for traceability *(agent — blocked on `12.3.4.1`)*:**

**Objective:** the clip reaches the container the only way it can — inside the image ([research note §1](../ADA_ECU/doc/research_notes/video-source-for-r12.md#1-platform-finding--carsky-serves-no-camera-content): a Container Node has no volume, no bind mount, no host path).

**Scope:** commit the validated file at `ADA_ECU/media/ego-b-occluding-c.mp4` with a `.gitattributes` binary marker; add `COPY media/ /app/media/` to `ADA_ECU/Dockerfile`; confirm `.dockerignore` does not exclude `media/`. No code change — `VIDEO_CLIP_PATH` already defaults to `/app/media/ego-b-occluding-c.mp4` (`12.3.2.1`).

**Acceptance:** `ada-ecu-image` lane green; the built image contains the file at `/app/media/ego-b-occluding-c.mp4` at the expected size (verified by the lane's in-image run step).

**Dependencies:** after `12.3.4.1`; needs the § Open items item 2 decision. **Commit:** ~~`[12.3.4.2] feat: bake the demo clip into the ADA image`~~ — **no commit is made under this ID.**

### [ ] `12.3.4.3` — Retune the distance constants against the real clip *(agent — blocked on `12.3.7.2`)*

**Objective:** close [HLD §11 item 3](../ADA_ECU/doc/phase2-4-ada-ecu-hld.md#11-open-items-and-flags) — `VEHICLE_WIDTH_M` and `CAMERA_HFOV_DEG` are proposals with no calibration target until the clip exists.

**Scope:**

- Run the detector over the clip; extract the estimated range series for B; compare against the clip's intended geometry (B at roughly 10–40 m, [§3 content row](../ADA_ECU/doc/research_notes/video-source-for-r12.md#3-video-input-spec-to-build-phase-3-against)).
- Required property is **monotonic, consistently-biased range**, not absolute accuracy: B approaching must yield a decreasing series that crosses `GATE_ENTER_M` once.
- If the series disagrees, retune **only** `VEHICLE_WIDTH_M` / `CAMERA_HFOV_DEG` — the new values are recorded in [HLD §6](../ADA_ECU/doc/phase2-4-ada-ecu-hld.md#6-configuration--no-hardcoded-tunables) as the defaults and in `node-ada-ecu.md`. **Never retune the R13 gate** (D3/D6): the gate is a requirement value, the camera constants are estimates.
- Record the before/after series summary in `plans/doc/phase3-ada-detector-run.md`.

**Acceptance:** the estimated-range series for B is monotonic through the approach and crosses the gate once; the two constants' final values are committed in the config defaults and the node guide.

**Dependencies:** after `12.3.7.2` + `12.3.2.7`. **Commit:** `[12.3.4.3] fix: retune the pinhole distance constants against the demo clip`

---

## Task Group 3.5 — Zero-C evidence and store integration (serves R12, R3, R18)

### [ ] `12.3.5.1` — Zero-C check `tools/check_zero_c.py` *(agent — not blocked on the clip)*

**Objective:** make the R19 zero-C claim **falsifiable** (D6) — the check exists so a pass means something, not because a failure is expected.

**Scope:**

- `ADA_ECU/tools/check_zero_c.py`, Python 3 stdlib; input = a detection log (R3 JSONL) and optionally the ADA `[EVT]` stream; fails the run when **any** of:
  1. an own-sensor entry claims `source: v2x_relayed`;
  2. an own-sensor entry carries a `v2x:` id namespace;
  3. an own-sensor track sits within `ZERO_C_RADIUS_M` (default 5, from env — no literal) of the relayed C position at the same timestamp.
- Non-zero exit naming the offending line and which rule fired; exit 0 with the counts examined, so a vacuous pass is visible.
- Test `ADA_ECU/tools/tests/test_check_zero_c.py` (planner-designated path): a clean log exits 0; one planted violation per rule exits 1 naming that rule; an empty log exits non-zero (nothing examined is not a pass).

**Acceptance:** `python -m py_compile` passes; the test passes locally and on CI.

**Dependencies:** after `12.3.2.6` (line shape). **Commit:** `[12.3.5.1] feat: add the R12 zero-C evidence check`

### [ ] `12.3.5.2` — Detector smoke run over the clip: the R12 detection log *(agent — blocked on `12.3.7.2`)*

**Objective:** the R12 acceptance artifact — a detection log over the provided clip with per-frame objects and distance estimates, at an acceptable offline pace.

**Scope:**

- Run `detector/main.py` over the full clip on CPU; capture stdout to `plans/doc/phase3-ada-detector-run.md`'s referenced log excerpt (the full log is not committed; a representative excerpt plus the summary is).
- Record the [research note KPIs](../ADA_ECU/doc/research_notes/video-source-for-r12.md#measurable-checks-kpis): KPI 2 — ≥ 99% of declared frames read with zero decode errors; KPI 3 — **effective inference rate ≥ 5 Hz, i.e. wall-clock ≤ 200 ms per sampled frame**, measured over the whole clip; KPI 4 — ≥ 1 `class = vehicle`, `source = own_sensor` entry with a distance estimate for ≥ 90% of sampled frames; KPI 5 — `check_zero_c.py` exit 0 over the whole log.
- If KPI 3 fails, the remedy is raising `DETECTOR_FRAME_STRIDE`, never changing the model ([research note §5](../ADA_ECU/doc/research_notes/video-source-for-r12.md#5-requirement-mapping-and-flags)) — record the stride used.
- **Also record the detector warm-up time** — ONNX model load plus `VideoCapture` open, from process start to the first emitted R3 line. [m1-run-timing-and-event-triggering.md §9 open item 4](../requirements/m1-run-timing-and-event-triggering.md) has it estimated at 2–5 s and **unmeasured**, and §3.3 shows why it matters: against a 12.0 s bench lead-in (C at 60 m closing 2.5 m/s to the 30 m `gate_enter`), the ADA side needs warm-up plus `confirm_hits` at 5 Hz (0.6 s) ≈ 5.6 s, leaving **≈ 6.4 s of slack** — the alignment tolerance the whole timing design rests on. **This run is where that number comes from.** One measurement, recorded in the run doc; the remedy if it exceeds the slack is the bench's `start_delay_s`, not a change here (R20, § Open items item 7).

**Acceptance:** all four KPIs plus the measured warm-up recorded with their values; this closes the R12 detection-log box and the CPU-only/offline-pace box at host level (`5.3.6.2` repeats KPI 3 on the deployed node).

**Dependencies:** after `12.3.7.2` + `12.3.4.3` + `12.3.5.1`. **Commit:** `[12.3.5.2] docs: record the R12 detection log and KPI measurements`

### [ ] `3.3.5.3` — Retire the mock: real detector into the real store *(agent)*

**Objective:** the second Phase 3 box — own-sensor entries enter the store through the **same** R3 interface as relayed entries, with the fixture no longer required.

**Scope:**

- No new module. Change the default `DETECTOR_CMD` path in use from the Phase 2 fixture (`cat …/own_sensor_mock.jsonl`) to `python3 /app/detector/main.py` — already the [HLD §6](../ADA_ECU/doc/phase2-4-ada-ecu-hld.md#6-configuration--no-hardcoded-tunables) default, so this is a verification subtask plus whatever wiring defect it uncovers.
- Extend `13.2.8.2`'s loopback lane arm (or add a sibling arm) driving `ada_ecu` with the **real** detector over a synthetic frame source, asserting `own_sensor_ingest` events and at least one `track_transition` for an `own:<n>` id — proving the JSONL crosses the process boundary, parses through `r3_parser`, and lands in the store via the same `upsert` as `r2_parser`.
- The Phase 2 fixture stays committed as a test artifact — it is what `--expect-no-tracks` and the deterministic admission cases run against.

**Acceptance:** the lane arm is green and observes `own:<n>` tracks reaching `tracked` with `source = own_sensor`; the ADA build + ctest stay green.

**Dependencies:** after `12.3.2.7` + Phase 2 `13.2.8.2`. **Commit:** `[3.3.5.3] test: drive the store from the real detector subprocess`

### [ ] `12.3.5.4` — Lane `ada-zero-c` *(agent)*

**Objective:** make the zero-C check repeatable rather than a one-off run — it feeds R19, so it must not be a manual step.

**Scope:** job in `phase3-ci.yml`: install the detector requirements; run `detector/main.py --synthetic` (or over the committed clip when present in the checkout) capturing stdout; run `python ADA_ECU/tools/check_zero_c.py` over it and fail the job on non-zero; assert a non-zero examined-line count so the lane cannot pass on an empty log.

**Acceptance:** lane green on the pushed branch with a non-zero examined count.

**Dependencies:** after `12.3.5.1` + `12.3.2.7` + `12.3.3.3` (same file). **Commit:** `[12.3.5.4] chore: add the zero-C evidence CI lane`

---

## Task Group 3.6 — Image and deployment (serves R5)

### [ ] `5.3.6.1` — Extend the ADA image with the detector, model and clip *(agent, push by CI or [[car-sky]])*

**Objective:** the deployable image carries both processes (D9 image layout).

**Scope:** `ADA_ECU/Dockerfile` — add `COPY detector/ /app/detector/`, `RUN pip install --no-cache-dir -r /app/detector/requirements.txt` (build stage or runtime stage per the single-base design), `COPY models/ /app/models/`, and confirm `COPY media/` from `12.3.7.2`. **Layer order is load-bearing** ([m1-video-source-and-ivi-dashcam.md §5](../requirements/m1-video-source-and-ivi-dashcam.md)): `COPY media/` then `COPY models/` — both rarely-changing, so their blobs are pushed once and cached — and `COPY detector/` last, because it changes every commit. `detector/tests/` and `requirements-dev.txt` stay out via `.dockerignore`. Image layout ends as `/app/ada_ecu`, `/app/entrypoint.sh`, `/app/detector/`, `/app/models/yolo11n.onnx`, `/app/media/ego-b-occluding-c.mp4`.

**Acceptance:** `ada-ecu-image` lane green; the lane's in-image run step starts `detector/main.py --synthetic` inside the pulled arm64 image and observes R3 JSONL on stdout — proving the wheels resolved for aarch64 in the real image, which is what [HLD §11 item 6](../ADA_ECU/doc/phase2-4-ada-ecu-hld.md#11-open-items-and-flags) asks for.

**Dependencies:** after `12.3.1.1` + `12.3.2.7` + `12.3.3.1` (+ `12.3.7.2` when the clip exists). **Commit:** `[5.3.6.1] feat: add the detector, model and clip to the ADA image`

### [ ] `5.3.6.2` — USER-MANUAL: deploy the ADA node and measure the deployed inference rate *(user, Nydus UI)*

**Objective:** the ADA node runs on CarSky and meets KPI 3 on the real 2-vCPU node, not on a laptop.

**Scope:**

- Node config per [node-ada-ecu.md § Blueprint node config](../requirements/car-sky-guide/node-ada-ecu.md) as updated by `5.2.9.4`: image `registry.hackathon-2.carsky.io/m1-ada-ecu:latest`, `command: ["./entrypoint.sh"]`, `capabilities: ["NET_RAW"]`, the §6 env set. New Deployment → Deployment Viewer shows the ADA node **Running**, restart 0; mind the 2-deployment quota.
- From the node's View Log: confirm `detector_spawn` and a steady `own_sensor_ingest` stream; measure the effective sampled-frame rate from the event timestamps (**≥ 5 Hz / ≤ 200 ms per sampled frame**); if it fails, raise `DETECTOR_FRAME_STRIDE` in the node config and re-measure — config only, no rebuild.
- **Also closes [m1-video-source-and-ivi-dashcam.md §7 KPI 9](../requirements/m1-video-source-and-ivi-dashcam.md):** `detector_spawn` followed by `own_sensor_ingest` on the deployed node is the proof that the **baked-in clip opened on the real node**, not just on a laptop — the one thing no local run can establish about the `COPY media/` delivery path. Record it as such, naming the clip path the node reports.

**Acceptance:** Running evidence plus the measured deployed rate recorded in `plans/doc/phase3-ada-detector-run.md`; evidence commit by the orchestrating session after the user confirms. Deployment status and the log evidence are gathered by [[car-sky]] per [carsky-acceptance-evidence](../.claude/skills/carsky-acceptance-evidence/SKILL.md); the user performs the Nydus UI steps.

**Dependencies:** after `5.3.6.1`. **Commit:** `[5.3.6.2] docs: record the ADA node deploy and deployed inference rate`

---

## Task Group 3.7 — Demo clip: sourcing, licence, post-production, delivery (serves R12, R5)

> **Added 2026-08-02** on the user's direction, against [m1-video-source-and-ivi-dashcam.md](../requirements/m1-video-source-and-ivi-dashcam.md) §1(4), §5 and §9. It converts the clip from a blocking human deliverable into agent work: find footage under a licence the project may use, cut and encode it to the [§3 spec](../ADA_ECU/doc/research_notes/video-source-for-r12.md#3-video-input-spec-to-build-phase-3-against), and land it in the repo and the image the only way a Container Node can receive a file — a `COPY` layer.
>
> **The two binding content rows are not negotiable and no editing step can create them:** vehicle **B** visible and occluding the ego lane at roughly **10–40 m** in **≥ 90%** of frames, and vehicle **C never visible in any frame**. The second is what R19's "zero direct C detections" asserts; a clip that violates it invalidates the definition of done regardless of how good the detection log looks.

### [ ] `12.3.7.1` — Source, licence-check and download the demo clip *(agent — online search; **first in this group**)*

**Objective:** obtain **one** candidate ego-POV dashcam recording that satisfies the content rows, under a licence that permits redistribution and modification, and record its provenance. This subtask does **not** cut, encode, or commit video — that is `12.3.7.2`.

**Where to search — acceptable sources only.** The project is open-source-only ([solution-selection-criteria.md](../.claude/rules/solution-selection-criteria.md)), and the clip is redistributed twice: committed to this repo and pushed inside an OCI image. A licence that permits neither is disqualifying, however good the footage.

| Tier | Source | Licence to expect |
|---|---|---|
| 1 (prefer) | [Wikimedia Commons](https://commons.wikimedia.org/) video category search; [Openverse](https://openverse.org/) | CC0 / public domain / CC BY / CC BY-SA |
| 1 | [Internet Archive](https://archive.org/) items explicitly marked public domain or CC | PD / CC |
| 2 | Pexels, Pixabay, Mixkit, Coverr, Videvo — **per-clip licence, read it, do not assume the site-wide one** | Site "free to use" licences permitting commercial use and modification |
| 3 | Openly-licensed driving datasets — comma2k19 (MIT), or any dataset whose stated licence permits redistribution | MIT / BSD / CC BY |
| **Disqualified** | YouTube/Vimeo downloads under the standard platform licence; any file with no stated licence; anything paid, watermarked, or "editorial use only"; KITTI / BDD100K and other **CC BY-NC** datasets | — |

**On non-commercial (NC) licences:** an NC clip is *not* auto-rejected but is **not the agent's call** — it restricts redistribution of every image the project pushes. If the only viable candidate is NC, stop and escalate to the user with the licence text quoted (§ Open items item 10).

**Content acceptance the footage must satisfy** — checked before it is accepted, on the segment intended for use:

1. **Forward-facing ego dashcam view**, camera roughly fixed to the vehicle, same-heading near-collinear convoy (plan §2 composition assumption). Daylight, dry, clear — a rain-streaked or night clip costs detector confidence for nothing.
2. **Vehicle B**: exactly one vehicle directly ahead in the ego lane, visible and occluding, apparent range roughly **10–40 m**, present in **≥ 90%** of the segment's frames. A segment where B is at 100 m is useless — the R13 gate is 30 m.
3. **Vehicle C never visible**: operationally, **B is the frontmost visible vehicle in the ego lane for the entire segment** — no third vehicle ahead of B in that lane, and nothing visible past or through B in the ego lane at any moment. Vehicles in adjacent lanes and oncoming traffic are *not* C and are acceptable; a vehicle appearing ahead of B in the ego lane, even for one frame, disqualifies the segment (re-cut around it, or reject the clip).
4. **Approach signal, strongly preferred:** B's apparent range decreases across the segment, so `12.3.4.3` has a monotonic series to calibrate against and the R13 gate is crossed once.
5. **Source resolution ≥ 1280×720** so `12.3.7.2` downscales rather than upscales; ≥ 20 fps source; usable segment ≥ 60 s continuous.
6. No burned-in overlay obscuring the lane ahead (a corner speed/time HUD is fine); no heavy compression blocking on the vehicle ahead.

**How to check rows 2–4 — not by watching, by sampling.** `ffmpeg -i <raw> -vf fps=1 -q:v 3 <scratch>/frame_%04d.jpg`, then inspect **every** extracted frame of the intended segment. Row 3's claim is a claim about *all* frames, so a spot check does not establish it; sample at ≥ 1 fps and state the sampling rate in the record. Choose segment boundaries so the claim holds across them.

**Scope — what this subtask writes:**

- Download the chosen file to `ADA_ECU/media/source/` and add `ADA_ECU/media/source/` to `.gitignore`. **The raw download is never committed** — [§5](../requirements/m1-video-source-and-ivi-dashcam.md) is explicit that git keeps every blob forever, so exactly one final encoded file enters history and the raw stays local.
- Write the provenance sidecar `ADA_ECU/media/ego-b-occluding-c.source.md` carrying: source URL · title and author · licence name, SPDX-or-URL, and the exact attribution string the licence requires · date accessed · SHA-256 of the raw download · the raw file's `ffprobe` summary (container, codec, resolution, fps, duration, size) · the chosen in/out timestamps for the segment · the content verdict for rows 1–6 with the frame-sampling rate used and the frame indices checked for row 3.
- If any licence obliges attribution in the distributed artifact, the sidecar states where that attribution is carried (this file plus `ADA_ECU/README` if one exists) — an attribution that lives only in a chat message is not compliance.

**Escalation, not substitution:** if no candidate satisfies all of rows 1–6 under a tier-1/2/3 licence after searching the named sources, **do not lower a content row and do not fall back to `tools/make_sample_video.py`** (§ Clip contingency — the synthetic fixture forfeits R12 evidence). Report the three closest candidates with the row each fails, and hand the decision to the user.

**Acceptance:** the sidecar exists and is complete, every field filled with a real value; the licence is tier 1–3 and quoted; the content verdict states rows 1–6 pass on a named segment with the sampling evidence; the raw file is present locally at `ADA_ECU/media/source/` and is **not** staged for commit; `.gitignore` covers it. No video file enters the commit.

**Dependencies:** none within this phase — startable day one, in parallel with every module lane. **Commit:** `[12.3.7.1] docs: record the demo clip source, licence and content verdict`

### [ ] `12.3.7.2` — Post-produce the clip and land it in the repo and the image *(agent — after `12.3.7.1`; **needs the § Open items item 2 decision**)*

**Objective:** turn the raw candidate into the one deliverable artifact and put it where the container can read it. The brief deliberately covers **both** things the clip needs before it is usable, because they are one indivisible outcome — "an off-spec file committed, then replaced" is the exact history [§5](../requirements/m1-video-source-and-ivi-dashcam.md) warns against, so the encode and the placement land in one commit. This subtask **supersedes `12.3.4.2`**.

**Half A — post-processing / editing.** Tool: **ffmpeg** (LGPL/GPL, open-source, Linux — the only encoder this project uses). Target is the [§3 spec](../ADA_ECU/doc/research_notes/video-source-for-r12.md#3-video-input-spec-to-build-phase-3-against): MP4 (ISO BMFF) with `faststart` · H.264 High, `yuv420p` 8-bit progressive · **1280×720** · **20 fps constant** · 60–120 s (target 60 s) · ~4 Mbit/s → ~30 MB, **hard ceiling 60 MB** · **no audio track**.

```
ffmpeg -ss <in> -to <out> -i ADA_ECU/media/source/<raw> \
  -vf "crop=<w>:<h>:<x>:<y>,scale=1280:720:flags=lanczos,fps=20" \
  -c:v libx264 -profile:v high -pix_fmt yuv420p \
  -b:v 4M -maxrate 4.5M -bufsize 8M \
  -x264-params keyint=40:min-keyint=40:scenecut=0 \
  -movflags +faststart -an \
  ADA_ECU/media/ego-b-occluding-c.mp4
```

- `-ss/-to` trim to the segment `12.3.7.1` chose. `crop` first, **only** if the source aspect is not 16:9 — crop to 16:9 then scale, never letterbox (black bars are frames the detector wastes inference on) and **never upscale**.
- `fps=20` with a fixed keyint gives the constant frame rate `12.3.2.2` depends on: `timestamp_ms = frame_index / fps * 1000` is valid only under CFR, and the fixed GOP keeps the `DETECTOR_LOOP` re-open at frame 0 clean.
- `-an` drops audio (§3: an audio track only costs bytes). `+faststart` puts the moov atom first.
- Record the **exact command used**, verbatim, in `plans/doc/phase3-ada-detector-run.md` — a reproducible encode is what lets the clip be re-cut later without re-deriving these flags.

**Verify the content again, on the encoded file — do not inherit `12.3.7.1`'s verdict.** A trim point is a place where a frame that was excluded can reappear; re-run the same `fps=1` frame extraction against `ADA_ECU/media/ego-b-occluding-c.mp4` and re-confirm **B occluding at 10–40 m in ≥ 90% of frames** and **no vehicle ahead of B in the ego lane in any frame**. Then run `python ADA_ECU/tools/check_clip_spec.py ADA_ECU/media/ego-b-occluding-c.mp4` (from `12.2.9.1`) and require exit 0 on the format rows.

**Half B — placing the artifact into the repo and the image.**

- Final path **`ADA_ECU/media/ego-b-occluding-c.mp4`** — the path `VIDEO_CLIP_PATH` already defaults to inside the container as `/app/media/ego-b-occluding-c.mp4` (`12.3.2.1`, [HLD §6](../ADA_ECU/doc/phase2-4-ada-ecu-hld.md#6-configuration--no-hardcoded-tunables)). No code change: the path and the stride are env, never literals (CLAUDE.md principle 5).
- `.gitattributes`: `ADA_ECU/media/*.mp4 binary -diff -merge` so the blob is never diffed or merge-mangled.
- `ADA_ECU/Dockerfile`: `COPY media/  /app/media/` **as its own early layer**, ahead of `COPY models/` and well ahead of `COPY detector/`, per [§5](../requirements/m1-video-source-and-ivi-dashcam.md):

  ```dockerfile
  # media and model: change rarely, pushed once, cached thereafter
  COPY media/  /app/media/
  COPY models/ /app/models/
  # code: changes every commit
  COPY detector/ /app/detector/
  ```

- Confirm `ADA_ECU/.dockerignore` does **not** exclude `media/` (it excludes `doc/`, `tests/`, `tools/`, `requirements-dev.txt` — check the pattern list rather than assuming).
- **Measure and record the image-size delta** in `plans/doc/phase3-ada-detector-run.md`: `docker image inspect -f '{{.Size}}'` before and after, the media layer's own size from `docker history`, and the layer digest. Expect the delta to be ≈ the file size — H.264 is already compressed, so the layer's gzip gains ~0%.
- **Git tracking — the recommendation, and the flag.** A ~30 MB binary goes in **plain git**, committed exactly once. It joins `models/yolo11n.onnx` (~10 MB) and the repo's existing committed media (`presentation/assets/*.jpg|png`, `requirements/*.png`); ~40 MB of write-once binaries is within normal git limits and keeps the image build offline-reproducible on a node with no volume. **Git LFS is rejected** — it buys history hygiene at the cost of a remote-storage dependency and a new failure mode on every clone and CI checkout, days from the 2026-08-08 deadline (criterion C2). The one real cost is permanence: a re-encode is a full second copy, never a delta, so **iterate the encode locally and commit one final file**. This is § Open items item 2, whose owner is the **user** — the implementing agent obtains that confirmation before committing the binary and does not decide it.

**Acceptance (all required):**

- `check_clip_spec.py` exits 0 on `ADA_ECU/media/ego-b-occluding-c.mp4`; `ffprobe` reports MP4/H.264 High/`yuv420p`/1280×720/20 fps CFR/no audio stream/≤ 60 MB.
- The post-edit content re-verification is recorded with its sampling rate: B occluding ≥ 90% of frames at 10–40 m, **zero frames containing a vehicle ahead of B in the ego lane**.
- The exact ffmpeg command and the image-size delta, media-layer size and layer digest are recorded in `plans/doc/phase3-ada-detector-run.md`.
- Build passes: `docker buildx build --platform linux/arm64 --provenance=false --sbom=false -t m1-ada-ecu:latest ADA_ECU/` succeeds and CI lane `ada-ecu-image` is green; a run step inside the built arm64 image finds `/app/media/ego-b-occluding-c.mp4` at the expected byte size.
- Unit tests unaffected and still green (`python -m pytest ADA_ECU/detector/tests` on CI `ada-detector-tests`; `ctest --test-dir ADA_ECU/build`) — this subtask changes no code, and proving it changed no behaviour is part of being done.
- The raw source file is still uncommitted; exactly one video file is added to git history.

**Dependencies:** after `12.3.7.1`; needs the § Open items item 2 user decision. Unblocks `12.3.4.3`, `12.3.5.2`, `5.3.6.1`, `5.3.7.3`. **Commit:** `[12.3.7.2] feat: post-produce the demo clip and bake it into the ADA image`

### [ ] `5.3.7.3` — Record the media-layer size, cache-stability and push KPIs *(spawn [[car-sky]] — registry push)*

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
artifacts  12.3.3.1 (after 12.3.1.1; needs the repo-size decision) ∥ 12.3.3.2 (after 12.3.2.2)
evidence   12.3.5.1 (after 12.3.2.6) ──► 12.3.5.4 (also needs 12.3.2.7)
           3.3.5.3 (after 12.3.2.7 + phase-2 13.2.8.2)
image      5.3.6.1 (after 12.3.1.1 + 12.3.2.7 + 12.3.3.1)

CLIP LANE (agent-owned from 2026-08-02 - runs fully parallel with every lane above)
           12.3.7.1 (day one, no dependency) ──► 12.3.4.1 ──► 12.3.7.2 ──► 12.3.4.3 ──► 12.3.5.2
                                                   12.3.4.2 SUPERSEDED by 12.3.7.2 - not implemented
push/KPI   5.3.7.3 (after 12.3.7.2 + 5.3.6.1)
deploy     5.3.6.2 (after 5.3.6.1; run after 12.3.7.2 so the real clip is in the deployed image)
```

**Recommended runtime order (single tree):** 12.3.7.1 *(start it first — it is the only lane with an external unknown)* → 12.3.1.1 → 12.3.3.3 → 12.3.2.1 → 12.3.2.2 → 12.3.2.3 → 12.3.2.4 → 12.3.3.2 → 12.3.3.1 → 12.3.2.5 → 12.3.2.6 → 12.3.2.7 → 12.3.5.1 → 12.3.5.4 → 3.3.5.3 → 12.3.4.1 → 12.3.7.2 → 5.3.6.1 → 12.3.4.3 → 12.3.5.2 → 5.3.7.3 → 5.3.6.2.

**The clip lane is now parallel, not blocking.** `12.3.7.1` has no dependency inside the phase and no dependency on any other person, so it starts on day one alongside `12.3.1.1` and the module lane. Only the four clip-consuming subtasks (`12.3.4.1`, `12.3.7.2`, `12.3.4.3`, `12.3.5.2`) sit behind it, and none of the 15 module/CI/image subtasks do.

**Relative to Phase 4.** Fully parallel. Phase 4 consumes the store, not the detector; the only shared file is `ADA_ECU/CMakeLists.txt` (C++ targets — this phase adds none) and `ADA_ECU/Dockerfile` (this phase adds COPY lines, Phase 4 adds `capture.sh`). Sequence those two edits, not the phases.

## Acceptance traceability

| Milestone Phase 3 box | Closed by |
|---|---|
| Detection log with per-frame objects and distance estimates (R12) | 12.3.5.2 — **needs the clip**, now produced by 12.3.7.1 + 12.3.7.2; modules 12.3.2.2/3/5/6/7 |
| Entries enter via the same R3 interface, `source = own_sensor`, mock retired | 3.3.5.3 · 12.3.2.6 (frozen binding) · Phase 2 `3.2.3.2` + `3.2.4.1` |
| **Zero detections labeled C** | 12.3.5.1 (the falsifiable check) · 12.3.5.4 (repeatable in CI) · D6 structural argument · the clip content rows, established at 12.3.7.1 and **re-established on the encoded file** at 12.3.7.2 · recorded at 12.3.4.1 |
| CPU-only, offline pace acceptable | 12.3.5.2 KPI 3 (host) · 5.3.6.2 (deployed node) · 12.3.1.1 (CPU-only wheels) |
| *(no milestone box)* [Update §7](../requirements/m1-video-source-and-ivi-dashcam.md) KPI 7 media layer ≤ 60 MB, digest stable · KPI 8 second push transfers 0 bytes | 5.3.7.3 |
| *(no milestone box)* [Update §7](../requirements/m1-video-source-and-ivi-dashcam.md) KPI 9 the baked-in clip opens **on the deployed node** | 5.3.6.2 |
| *(phase task, no box)* R18 own-sensor evidence | `own_sensor_ingest` payloads from Phase 2 `18.2.2.3`, fed by this phase's real lines |

## Open items & flags (no Phase 3 subtask may silently close them)

| # | Item | Owner / closes at |
|---|---|---|
| 1 | **`onnxruntime` / `opencv-python-headless` aarch64 wheel availability is unproven** ([HLD §11 item 6](../ADA_ECU/doc/phase2-4-ada-ecu-hld.md#11-open-items-and-flags)). De-risked first by `12.3.1.1`. A red lane escalates to [[project-architecture]] — pin an older wheel, change the base image, or accept a QEMU source build with a raised timeout. Not an implementer's call | `12.3.1.1`, then [[project-architecture]] |
| 2 | **Repo size — two committed binaries** in the build context: `models/yolo11n.onnx` (~10 MB, `12.3.3.1`) and `media/ego-b-occluding-c.mp4` (~30 MB, ≤ 60 MB, `12.3.7.2`). Both must be in the image because a Container Node has no volume; the alternative costs offline reproducibility. **Recommendation: commit both to plain git, once each; Git LFS rejected** ([update §5](../requirements/m1-video-source-and-ivi-dashcam.md) — a remote-storage dependency days from the deadline, criterion C2). **Needed before `12.3.3.1` and before `12.3.7.2`** | **user** |
| 3 | **Planner-designated test/tool paths beyond the HLD's list**: `detector/tests/test_config.py`, `test_inference.py`, `test_main.py`; `ADA_ECU/tools/tests/test_check_zero_c.py`. Required by subtask discipline; flagged to [[project-architecture]] as HLD-consistent additions | [[project-architecture]] (ack) |
| 4 | ~~The clip remains the phase's gating external input~~ — **resolved 2026-08-02.** Sourcing and post-production are agent work in **group 3.7**; `12.2.9.3` stays open as the preferred source if the user already holds footage, but no longer gates the phase. § Clip contingency still governs the case where **no acceptably-licensed footage can be found** — escalate per `12.3.7.1`; **never substitute the synthetic fixture for R12 evidence** | `12.3.7.1` |
| 5 | **The §3 clip-spec numbers are proposals** until FPT-Mentor confirms them ([research note §5](../ADA_ECU/doc/research_notes/video-source-for-r12.md#5-requirement-mapping-and-flags)). A delivered 1080p/30 fps clip is accepted by **raising the stride**, never by changing the model or the gate | user / `12.3.4.1` |
| 6 | **Distance accuracy is unvalidated until the clip exists** ([HLD §11 item 3](../ADA_ECU/doc/phase2-4-ada-ecu-hld.md#11-open-items-and-flags)) — `12.3.4.3` is the retune, and it may only move `VEHICLE_WIDTH_M` / `CAMERA_HFOV_DEG`, never `GATE_ENTER_M` / `GATE_EXIT_M` | `12.3.4.3` |
| 7 | **R20's detector half is not planned in this phase.** [m1-run-timing-and-event-triggering.md §7](../requirements/m1-run-timing-and-event-triggering.md) makes real-time pacing (`DETECTOR_REALTIME_PACING`, `DETECTOR_CLIP_FPS`, `DETECTOR_START_DELAY_S`, §6.1) a requirement, and §2(d) names the free-running detector the **dominant timing error term** — but §8(1) schedules R20/R21 **behind** this phase's acceptance, not in front of it, and §8(3) notes it only becomes mandatory if the deferred IVI dashcam view is accepted. `12.3.2.1` and `12.3.2.2` are annotated so the addition stays a table row and one sleep; the instrument that would measure it is `21.4.8.2` in [phase4_tasks.md](phase4_tasks.md) group 4.8. **No subtask here may add the keys or the pacing on its own** | **user** (accept/reject R20 per §8(1)) |
| 8 | **Detector warm-up (ONNX load + `VideoCapture` open) is unmeasured** — estimated 2–5 s against the ≈ 6.4 s slack of §3.3, and it is the term that consumes that slack ([§9 open item 4](../requirements/m1-run-timing-and-event-triggering.md)). `12.3.5.2` is where the number is produced; `5.3.6.2` repeats it on the 2-vCPU node, where it will be worse | `12.3.5.2`, then `5.3.6.2` |
| 9 | **Clip licence** — the sidecar `ADA_ECU/media/ego-b-occluding-c.source.md` is the compliance record for a file redistributed in this repo **and** inside every pushed OCI image. If the licence obliges attribution, the sidecar must name where that attribution is carried in the distributed artifact | `12.3.7.1` |
| 10 | **Non-commercial (CC BY-NC) footage is not the agent's call.** It permits redistribution but restricts it in a way that touches every image push. If the only viable candidate is NC-licensed, `12.3.7.1` stops and escalates with the licence quoted rather than accepting it | **user** |
| 11 | **The IVI dashcam view stays deferred** — [update §8](../requirements/m1-video-source-and-ivi-dashcam.md) and the user's 2026-08-02 direction. Its one consequence for *this* phase: [update §4](../requirements/m1-video-source-and-ivi-dashcam.md) makes real-time detector pacing **mandatory only if the view is built**, so with the view excluded, R20's detector half (item 7) stays optional and unplanned. No Phase 3 subtask may add clip-serving, an `exposedPorts` entry, or pacing on that basis | closed by the deferral |

---

*Created 2026-08-02 by project-planner from [phase2-4-ada-ecu-hld.md D6](../ADA_ECU/doc/phase2-4-ada-ecu-hld.md#d6--r12-detector-frame-source-seam-inference-distance-zero-c-evidence), [video-source-for-r12.md](../ADA_ECU/doc/research_notes/video-source-for-r12.md) and [milestone1.md § Phase 3](milestone1.md#phase-3--object-detection-from-video-r12--runs--with-phase-4). 6 task groups, 20 subtasks: 19 agent-implemented (4 of them clip-blocked), 1 user-manual. Planned from zero.*

*Amended 2026-08-02 by project-planner against [m1-video-source-and-ivi-dashcam.md](../requirements/m1-video-source-and-ivi-dashcam.md) (commit `a7808c8`) and the user's direction of that date: **task group 3.7 added** (`12.3.7.1`, `12.3.7.2`, `5.3.7.3`), `12.3.4.2` **superseded in place** by `12.3.7.2`, the clip lane converted from human-blocked to agent-owned and parallel, and open items 9–11 opened. No existing ID renumbered. Consolidated ADA+IVI view: [ada-ivi-plan.md](ada-ivi-plan.md). Now 7 task groups, 23 subtasks: 21 agent-implemented, 1 [[car-sky]], 1 user-manual, 1 superseded.*
