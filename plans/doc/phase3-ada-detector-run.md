# Phase 3 — ADA detector run record (12.3.4.3 · 12.3.5.2)

> Run evidence for the R12 detection log, the distance-constant retune, and the KPI
> measurements of [phase3_tasks.md](../phase3_tasks.md) groups 3.4–3.5. The KPIs are
> [video-source-for-r12.md § Measurable checks](../../ADA_ECU/doc/research_notes/video-source-for-r12.md#measurable-checks-kpis);
> the retune rule is [ADA HLD D6](../../ADA_ECU/doc/ada-ecu-design-decisions.md#d6--r12-detector-frame-source-seam-inference-distance-zero-c-evidence).

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

