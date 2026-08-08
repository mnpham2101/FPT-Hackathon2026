# Phase 3 — ADA detector run record (12.3.4.3 · 12.3.5.2)

> Run evidence for the R12 detection log, the distance-constant retune, and the KPI
> measurements of [phase3_tasks.md](../phase3_tasks.md) groups 3.4–3.5. The KPIs are
> [video-source-for-r12.md § Measurable checks](../../documents/KnowledgeBase/video-source-for-r12.md#measurable-checks-kpis);
> the retune rule is [ADA HLD D6](../../documents/Design/ADA-ECU/ada-ecu-design-decisions.md#d6--r12-detector-frame-source-seam-inference-distance-zero-c-evidence).

## Venue

The runs below were executed on the dev host (Windows-on-ARM64, x64 CPython 3.12.10
under emulation: onnxruntime 1.28.0, opencv 5.0.0, numpy 2.5.1) against the committed
clip `ADA_ECU/media/ego-b-occluding-c.mp4` and model `ADA_ECU/models/yolo11n.onnx`,
detector defaults from `detector/config.py` unless stated. This is **interim host
evidence**: the plan's venue for these subtasks is CI lane `ada-detector-run`
(12.3.3.4), which repeats both arms natively on every push of the phase branch, and
the deployed-node half of the CPU-pace criterion lands with the Phase 4/6 Room runs
(`5.3.6.2`, `22.3.6.3`). Timing figures from this host under-state a native runner;
detection content, series shape and the zero-C verdict are venue-independent.

## 12.3.4.3 — distance-constant retune

**Selection of B.** Single unpaced pass, 50 sampled frames (stride 4). Two
full-length (50-frame) tracks exist; adjacent-lane and oncoming traffic appear as
short-lived tracks, which is expected and is not C. B is the full-length track with
the by-far largest boxes (nearest range): `own:4`, present in all 50 sampled frames,
closing monotonically after the first two seconds of box jitter.

**Before (defaults `VEHICLE_WIDTH_M=1.8`, `CAMERA_HFOV_DEG=60`).** B's estimated
range runs 22.3 → 4.0 m: the series starts *inside* `GATE_ENTER_M` (30 m) and never
crosses it downward — the required once-per-loop gate crossing fails, so a retune is
mandatory, and the direction matches D6's prediction (B is a coach, wider than the
1.8 m car default, so range under-reads).

**After (`VEHICLE_WIDTH_M=2.6`, `CAMERA_HFOV_DEG=34.4`).** Scale factor 2.69 chosen
to place the series onto the provenance record's ~60 → ~10 m: width 2.6 m is a
physical coach width; 34.4° is the effective HFOV of the cropped, re-encoded footage
(both are D6's two sanctioned knobs; the R13 gate was not touched). Result over one
pass:

```
60 59 51 47 58 58 53 50 48 45 46 44 45 43 43 41 40 41 39 38 37 36 36 34 33
32 31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 15 14 13 12 11 11 11
```

- Start 60.1 m, end 10.8 m; exactly **one** downward crossing of 30 m per pass
  (frame 28 of 50, ≈ 5.6 s into the 10 s clip at 1.0×).
- Monotonic through the approach after the initial jitter window (one > 0.5 m
  up-step in frames 1–8; none after).
- Mean confidence 0.82, minimum 0.39.

The two values are committed as the `detector/config.py` defaults. **Hand-off to
project-architecture:** the HLD §6 *Env — detector* row `VEHICLE_WIDTH_M ·
CAMERA_HFOV_DEG` and [node-ada-ecu.md](../../requirements/car-sky-guide/node-ada-ecu.md)
need the new defaults `2.6 · 34.4`; the measured series above and this run record are
the evidence. Neither file is edited from this phase (HLD ownership rule).

**Findings flagged, not absorbed:**

- At the retuned scale B's mean lateral offset is **+2.29 m** (right of frame
  centre), so 12.3.4.3's numeric selection sub-criterion "lateral within ±2 m for
  ≥ 90 % of frames" holds only at the pre-retune scale (88 %). Selection by largest
  box area is unambiguous; the ±2 m bound wants re-stating against the retuned
  scale.
- The detector intermittently returns a near-full-frame-width false box (estimated
  range 4.3 m retuned, confidence 0.36–0.52, up to 3 consecutive sampled frames —
  ids `own:2`, `own:9` et al.). With `CONFIRM_HITS=3` such a burst can transiently
  reach `tracked`. Raising `CONF_THRESHOLD` above 0.52 would also cut real B frames
  (min 0.39), so the constant stays; Phase 4's `d_AB` selection should prefer the
  dominant/nearest *persistent* track rather than any tracked own entry.

## 12.3.5.2 — detection log and KPI record (host interim)

**Paced looped run** (defaults: `DETECTOR_LOOP=true`, `DETECTOR_REALTIME_PACING=true`,
stride 4), 60 s wall: 297 sampled frames emitted 1211 R3 lines.

| Measure | Value (host) | Note |
|---|---|---|
| KPI 2 — decode | 50/50 declared sampled frames per pass, 0 decode errors, across 6 loops | also proved per-pass by `check_clip_spec.py` (200/200 decoded) |
| KPI 3 — unpaced inference rate | **2.65 Hz (377 ms/frame) — below the 5 Hz floor on this host** | emulated x64-on-ARM64 host; not the criterion's venue. YOLO11n CPU reference is 56 ms at 640, so the native CI runner arm (`ada-detector-run`, unpaced) is expected to clear 5 Hz; the deployed figure is `5.3.6.2`. If the runner also falls short, the remedy is raising `DETECTOR_FRAME_STRIDE`, never the model |
| KPI 4 — detection coverage | ≈ 100 % (297 emitting frames vs ≈ 296 released in the window) | every sampled frame carried ≥ 1 `class=vehicle`, `source=own_sensor` entry with a distance estimate |
| KPI 5 — zero-C | `check_zero_c.py` exit 0: `detection_lines=204 own_sensor_lines=204` (single-pass log), no rule fired | rule 3 runs with the `[EVT]` stream once Phase 4's Room log exists |
| Paced-rate check (HLD §12 K4) | **5.004 Hz** vs target `20/4 = 5.000` — within 0.1 % (bound ±2 %) | read off the run's own emit stamps |
| Warm-up `W` (spawn → first R3 line) | 0.83 s warm-cache; 2.9–4.6 s cold-cache single-pass runs | host figure only; the value R22's `start_delay_s` ships from is the deployed one (`22.3.6.3`) |
| Loop re-open gap | max inter-frame arrival gap 0.33 s | ≪ `TRACK_TIMEOUT_MS` 1000, so ego's own B track cannot expire between clip passes; B's id is re-minted each pass (the wrap breaks IoU continuity), each loop reading as a fresh approach |

**Representative log excerpt** (retuned single pass, frame ≈ 5.6 s — B crossing the
gate; one line per detection, schema-valid against the frozen R3 schema):

```json
{"id":"own:4","class":"vehicle","source":"own_sensor","position":{"x":30.180369663056084,"y":2.2994638481284544},"distance":30.180369663056084,"speed":3.904966320771539,"confidence":0.9108918905258179,"state":"not_tracked","timestamps":{"measured":1785851514719,"received":1785851514764,"lastUpdated":1785851514764}}
```

**Exit behaviour note.** On this host the 60 s run ends by `TerminateProcess`
(Windows has no SIGTERM delivery), reported as returncode 1 by the driver; on POSIX
the detector exits 0 on SIGTERM, which `detector/tests/test_main.py` asserts and the
CI lanes rely on.

## Blocked measurements (recorded here when their inputs exist)

| Subtask | Waits on |
|---|---|
| `5.3.6.2` — deployed inference rate + clip-open proof | Phase 4 `18.4.11.1` saved `ada.log` from the isolated ADA Room |
| `22.3.6.3` — deployed warm-up `W` → bench `start_delay_s` | same log; unblocks Phase 1 `22.1.13.4` |
| `5.3.7.3` — media-layer digest stability + push KPIs 7/8 | two `ada-ecu-image` lane runs plus car-sky's registry push readings |
