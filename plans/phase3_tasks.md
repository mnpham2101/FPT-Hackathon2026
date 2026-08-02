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
- **The clip** — `12.2.9.3` in [phase2_tasks.md](phase2_tasks.md). **This is a human deliverable and it gates group 3.4, group 3.5's R12 evidence, and the phase's first acceptance box.** Groups 3.1–3.3 and 3.6 do not need it and start immediately.

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
- **Escalation, not absorption:** if the clip has not arrived by the day group 3.4 would start, the plan does not quietly substitute the fixture — `12.3.4.1` is marked blocked and the slip is reported to the user against the 2026-08-08 deadline.

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

## Task Group 3.4 — Clip intake and calibration (serves R12) — **blocked on the human task**

> Every subtask here waits on `12.2.9.3` in [phase2_tasks.md](phase2_tasks.md). If the clip has not arrived when group 3.3 finishes, this group is marked blocked and the slip is reported — see § Clip contingency.

### [ ] `12.3.4.1` — Validate the delivered clip *(agent — blocked on `12.2.9.3`)*

**Objective:** a pass/fail verdict on the delivered file before it enters the repo.

**Scope:** run `python ADA_ECU/tools/check_clip_spec.py <delivered file>`; record the verdict (every attribute, actual vs expected) in `plans/doc/phase3-ada-detector-run.md`. If a **format** attribute fails, re-encode in one `ffmpeg` command to the [§3 spec](../ADA_ECU/doc/research_notes/video-source-for-r12.md#3-video-input-spec-to-build-phase-3-against) and record the exact command; if a **content** row fails (B not occluding at 10–40 m in ≥ 90% of frames, or **C visible in any frame**), the clip is rejected back to the user — **no re-encode fixes content, and a clip containing C invalidates R19 outright**.

**Acceptance:** preflight exit 0 on the final file, with the report recorded. Doc-only commit.

**Dependencies:** after `12.2.9.3` + `12.2.9.1`. **Commit:** `[12.3.4.1] docs: record the delivered clip preflight verdict`

### [ ] `12.3.4.2` — Commit the clip and wire it into the image *(agent — blocked on `12.3.4.1`)*

**Objective:** the clip reaches the container the only way it can — inside the image ([research note §1](../ADA_ECU/doc/research_notes/video-source-for-r12.md#1-platform-finding--carsky-serves-no-camera-content): a Container Node has no volume, no bind mount, no host path).

**Scope:** commit the validated file at `ADA_ECU/media/ego-b-occluding-c.mp4` with a `.gitattributes` binary marker; add `COPY media/ /app/media/` to `ADA_ECU/Dockerfile`; confirm `.dockerignore` does not exclude `media/`. No code change — `VIDEO_CLIP_PATH` already defaults to `/app/media/ego-b-occluding-c.mp4` (`12.3.2.1`).

**Acceptance:** `ada-ecu-image` lane green; the built image contains the file at `/app/media/ego-b-occluding-c.mp4` at the expected size (verified by the lane's in-image run step).

**Dependencies:** after `12.3.4.1`; needs the § Open items item 2 decision. **Commit:** `[12.3.4.2] feat: bake the demo clip into the ADA image`

### [ ] `12.3.4.3` — Retune the distance constants against the real clip *(agent — blocked on `12.3.4.2`)*

**Objective:** close [HLD §11 item 3](../ADA_ECU/doc/phase2-4-ada-ecu-hld.md#11-open-items-and-flags) — `VEHICLE_WIDTH_M` and `CAMERA_HFOV_DEG` are proposals with no calibration target until the clip exists.

**Scope:**

- Run the detector over the clip; extract the estimated range series for B; compare against the clip's intended geometry (B at roughly 10–40 m, [§3 content row](../ADA_ECU/doc/research_notes/video-source-for-r12.md#3-video-input-spec-to-build-phase-3-against)).
- Required property is **monotonic, consistently-biased range**, not absolute accuracy: B approaching must yield a decreasing series that crosses `GATE_ENTER_M` once.
- If the series disagrees, retune **only** `VEHICLE_WIDTH_M` / `CAMERA_HFOV_DEG` — the new values are recorded in [HLD §6](../ADA_ECU/doc/phase2-4-ada-ecu-hld.md#6-configuration--no-hardcoded-tunables) as the defaults and in `node-ada-ecu.md`. **Never retune the R13 gate** (D3/D6): the gate is a requirement value, the camera constants are estimates.
- Record the before/after series summary in `plans/doc/phase3-ada-detector-run.md`.

**Acceptance:** the estimated-range series for B is monotonic through the approach and crosses the gate once; the two constants' final values are committed in the config defaults and the node guide.

**Dependencies:** after `12.3.4.2` + `12.3.2.7`. **Commit:** `[12.3.4.3] fix: retune the pinhole distance constants against the demo clip`

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

### [ ] `12.3.5.2` — Detector smoke run over the clip: the R12 detection log *(agent — blocked on `12.3.4.2`)*

**Objective:** the R12 acceptance artifact — a detection log over the provided clip with per-frame objects and distance estimates, at an acceptable offline pace.

**Scope:**

- Run `detector/main.py` over the full clip on CPU; capture stdout to `plans/doc/phase3-ada-detector-run.md`'s referenced log excerpt (the full log is not committed; a representative excerpt plus the summary is).
- Record the [research note KPIs](../ADA_ECU/doc/research_notes/video-source-for-r12.md#measurable-checks-kpis): KPI 2 — ≥ 99% of declared frames read with zero decode errors; KPI 3 — **effective inference rate ≥ 5 Hz, i.e. wall-clock ≤ 200 ms per sampled frame**, measured over the whole clip; KPI 4 — ≥ 1 `class = vehicle`, `source = own_sensor` entry with a distance estimate for ≥ 90% of sampled frames; KPI 5 — `check_zero_c.py` exit 0 over the whole log.
- If KPI 3 fails, the remedy is raising `DETECTOR_FRAME_STRIDE`, never changing the model ([research note §5](../ADA_ECU/doc/research_notes/video-source-for-r12.md#5-requirement-mapping-and-flags)) — record the stride used.
- **Also record the detector warm-up time** — ONNX model load plus `VideoCapture` open, from process start to the first emitted R3 line. [m1-run-timing-and-event-triggering.md §9 open item 4](../requirements/m1-run-timing-and-event-triggering.md) has it estimated at 2–5 s and **unmeasured**, and §3.3 shows why it matters: against a 12.0 s bench lead-in (C at 60 m closing 2.5 m/s to the 30 m `gate_enter`), the ADA side needs warm-up plus `confirm_hits` at 5 Hz (0.6 s) ≈ 5.6 s, leaving **≈ 6.4 s of slack** — the alignment tolerance the whole timing design rests on. **This run is where that number comes from.** One measurement, recorded in the run doc; the remedy if it exceeds the slack is the bench's `start_delay_s`, not a change here (R20, § Open items item 7).

**Acceptance:** all four KPIs plus the measured warm-up recorded with their values; this closes the R12 detection-log box and the CPU-only/offline-pace box at host level (`5.3.6.2` repeats KPI 3 on the deployed node).

**Dependencies:** after `12.3.4.2` + `12.3.4.3` + `12.3.5.1`. **Commit:** `[12.3.5.2] docs: record the R12 detection log and KPI measurements`

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

**Scope:** `ADA_ECU/Dockerfile` — add `COPY detector/ /app/detector/`, `RUN pip install --no-cache-dir -r /app/detector/requirements.txt` (build stage or runtime stage per the single-base design), `COPY models/ /app/models/`, and confirm `COPY media/` from `12.3.4.2`. `detector/tests/` and `requirements-dev.txt` stay out via `.dockerignore`. Image layout ends as `/app/ada_ecu`, `/app/entrypoint.sh`, `/app/detector/`, `/app/models/yolo11n.onnx`, `/app/media/ego-b-occluding-c.mp4`.

**Acceptance:** `ada-ecu-image` lane green; the lane's in-image run step starts `detector/main.py --synthetic` inside the pulled arm64 image and observes R3 JSONL on stdout — proving the wheels resolved for aarch64 in the real image, which is what [HLD §11 item 6](../ADA_ECU/doc/phase2-4-ada-ecu-hld.md#11-open-items-and-flags) asks for.

**Dependencies:** after `12.3.1.1` + `12.3.2.7` + `12.3.3.1` (+ `12.3.4.2` when the clip exists). **Commit:** `[5.3.6.1] feat: add the detector, model and clip to the ADA image`

### [ ] `5.3.6.2` — USER-MANUAL: deploy the ADA node and measure the deployed inference rate *(user, Nydus UI)*

**Objective:** the ADA node runs on CarSky and meets KPI 3 on the real 2-vCPU node, not on a laptop.

**Scope:**

- Node config per [node-ada-ecu.md § Blueprint node config](../requirements/car-sky-guide/node-ada-ecu.md) as updated by `5.2.9.4`: image `registry.hackathon-2.carsky.io/m1-ada-ecu:latest`, `command: ["./entrypoint.sh"]`, `capabilities: ["NET_RAW"]`, the §6 env set. New Deployment → Deployment Viewer shows the ADA node **Running**, restart 0; mind the 2-deployment quota.
- From the node's View Log: confirm `detector_spawn` and a steady `own_sensor_ingest` stream; measure the effective sampled-frame rate from the event timestamps (**≥ 5 Hz / ≤ 200 ms per sampled frame**); if it fails, raise `DETECTOR_FRAME_STRIDE` in the node config and re-measure — config only, no rebuild.

**Acceptance:** Running evidence plus the measured deployed rate recorded in `plans/doc/phase3-ada-detector-run.md`; evidence commit by the orchestrating session after the user confirms.

**Dependencies:** after `5.3.6.1`. **Commit:** `[5.3.6.2] docs: record the ADA node deploy and deployed inference rate`

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

CLIP LANE (blocked on the human task 12.2.9.3 - never blocks the lanes above)
           12.3.4.1 ──► 12.3.4.2 ──► 12.3.4.3 ──► 12.3.5.2
deploy     5.3.6.2 (after 5.3.6.1; best run after 12.3.4.2 so the real clip is in the image)
```

**Recommended runtime order (single tree):** 12.3.1.1 → 12.3.3.3 → 12.3.2.1 → 12.3.2.2 → 12.3.2.3 → 12.3.2.4 → 12.3.3.2 → 12.3.3.1 → 12.3.2.5 → 12.3.2.6 → 12.3.2.7 → 12.3.5.1 → 12.3.5.4 → 3.3.5.3 → 5.3.6.1 → *(clip lane when unblocked)* 12.3.4.1 → 12.3.4.2 → 12.3.4.3 → 12.3.5.2 → 5.3.6.2.

**Relative to Phase 4.** Fully parallel. Phase 4 consumes the store, not the detector; the only shared file is `ADA_ECU/CMakeLists.txt` (C++ targets — this phase adds none) and `ADA_ECU/Dockerfile` (this phase adds COPY lines, Phase 4 adds `capture.sh`). Sequence those two edits, not the phases.

## Acceptance traceability

| Milestone Phase 3 box | Closed by |
|---|---|
| Detection log with per-frame objects and distance estimates (R12) | 12.3.5.2 — **needs the clip**; modules 12.3.2.2/3/5/6/7 |
| Entries enter via the same R3 interface, `source = own_sensor`, mock retired | 3.3.5.3 · 12.3.2.6 (frozen binding) · Phase 2 `3.2.3.2` + `3.2.4.1` |
| **Zero detections labeled C** | 12.3.5.1 (the falsifiable check) · 12.3.5.4 (repeatable in CI) · D6 structural argument · clip content row (`12.3.4.1`) |
| CPU-only, offline pace acceptable | 12.3.5.2 KPI 3 (host) · 5.3.6.2 (deployed node) · 12.3.1.1 (CPU-only wheels) |
| *(phase task, no box)* R18 own-sensor evidence | `own_sensor_ingest` payloads from Phase 2 `18.2.2.3`, fed by this phase's real lines |

## Open items & flags (no Phase 3 subtask may silently close them)

| # | Item | Owner / closes at |
|---|---|---|
| 1 | **`onnxruntime` / `opencv-python-headless` aarch64 wheel availability is unproven** ([HLD §11 item 6](../ADA_ECU/doc/phase2-4-ada-ecu-hld.md#11-open-items-and-flags)). De-risked first by `12.3.1.1`. A red lane escalates to [[project-architecture]] — pin an older wheel, change the base image, or accept a QEMU source build with a raised timeout. Not an implementer's call | `12.3.1.1`, then [[project-architecture]] |
| 2 | **Repo size — two committed binaries** in the build context: `models/yolo11n.onnx` (~10 MB, `12.3.3.1`) and `media/ego-b-occluding-c.mp4` (≤ 60 MB, `12.3.4.2`). Both must be in the image because a Container Node has no volume; the alternative costs offline reproducibility. Recommendation: commit both. **Needed before `12.3.3.1`** | **user** |
| 3 | **Planner-designated test/tool paths beyond the HLD's list**: `detector/tests/test_config.py`, `test_inference.py`, `test_main.py`; `ADA_ECU/tools/tests/test_check_zero_c.py`. Required by subtask discipline; flagged to [[project-architecture]] as HLD-consistent additions | [[project-architecture]] (ack) |
| 4 | **The clip remains the phase's gating external input** — `12.2.9.3` in [phase2_tasks.md](phase2_tasks.md). § Clip contingency states what is still delivered without it and exactly what is forfeited. **Escalate the slip; do not substitute the synthetic fixture for R12 evidence** | **user** |
| 5 | **The §3 clip-spec numbers are proposals** until FPT-Mentor confirms them ([research note §5](../ADA_ECU/doc/research_notes/video-source-for-r12.md#5-requirement-mapping-and-flags)). A delivered 1080p/30 fps clip is accepted by **raising the stride**, never by changing the model or the gate | user / `12.3.4.1` |
| 6 | **Distance accuracy is unvalidated until the clip exists** ([HLD §11 item 3](../ADA_ECU/doc/phase2-4-ada-ecu-hld.md#11-open-items-and-flags)) — `12.3.4.3` is the retune, and it may only move `VEHICLE_WIDTH_M` / `CAMERA_HFOV_DEG`, never `GATE_ENTER_M` / `GATE_EXIT_M` | `12.3.4.3` |
| 7 | **R20's detector half is not planned in this phase.** [m1-run-timing-and-event-triggering.md §7](../requirements/m1-run-timing-and-event-triggering.md) makes real-time pacing (`DETECTOR_REALTIME_PACING`, `DETECTOR_CLIP_FPS`, `DETECTOR_START_DELAY_S`, §6.1) a requirement, and §2(d) names the free-running detector the **dominant timing error term** — but §8(1) schedules R20/R21 **behind** this phase's acceptance, not in front of it, and §8(3) notes it only becomes mandatory if the deferred IVI dashcam view is accepted. `12.3.2.1` and `12.3.2.2` are annotated so the addition stays a table row and one sleep; the instrument that would measure it is `21.4.8.2` in [phase4_tasks.md](phase4_tasks.md) group 4.8. **No subtask here may add the keys or the pacing on its own** | **user** (accept/reject R20 per §8(1)) |
| 8 | **Detector warm-up (ONNX load + `VideoCapture` open) is unmeasured** — estimated 2–5 s against the ≈ 6.4 s slack of §3.3, and it is the term that consumes that slack ([§9 open item 4](../requirements/m1-run-timing-and-event-triggering.md)). `12.3.5.2` is where the number is produced; `5.3.6.2` repeats it on the 2-vCPU node, where it will be worse | `12.3.5.2`, then `5.3.6.2` |

---

*Created 2026-08-02 by project-planner from [phase2-4-ada-ecu-hld.md D6](../ADA_ECU/doc/phase2-4-ada-ecu-hld.md#d6--r12-detector-frame-source-seam-inference-distance-zero-c-evidence), [video-source-for-r12.md](../ADA_ECU/doc/research_notes/video-source-for-r12.md) and [milestone1.md § Phase 3](milestone1.md#phase-3--object-detection-from-video-r12--runs--with-phase-4). 6 task groups, 20 subtasks: 19 agent-implemented (4 of them clip-blocked), 1 user-manual. Planned from zero.*
