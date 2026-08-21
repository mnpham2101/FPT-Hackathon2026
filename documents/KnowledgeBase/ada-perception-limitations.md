# ADA-ECU's perception limitations, and where each could go next

ADA-ECU's own-sensor perception — "what does A see of B" — is the half of cooperative awareness this project builds itself, rather than relaying over V2X. It works for the M1 demo, but every simplification that makes the demo achievable is also a limit on what the system can actually be trusted to do. This note lists the four limitations, grounded in the current code and design decisions, and what each would need to become production-credible.

## 1. Distance-to-next-vehicle from video is inaccurate

**Current state.** ADA estimates range with a known-width pinhole formula — `distance = vehicle_width_m * focal_px / bbox_width_px` — implemented as a pure function in [`ADA_ECU/detector/distance.py`](../../ADA_ECU/detector/distance.py). Both inputs it depends on are assumed constants, not measurements: `VEHICLE_WIDTH_M` and `CAMERA_HFOV_DEG`. The design decision record is explicit about what this buys and what it doesn't — "absolute accuracy rests on two constants that a user-supplied clip cannot calibrate," and the method is chosen to deliver "a monotonic, consistently biased range," not an accurate one ([ada-ecu-design-decisions.md D6](../Design/MODULE-DESIGN/ADA-ECU/ada-ecu-design-decisions.md)). There is no camera calibration step, no ground-plane homography, and no correction for whatever vehicle actually appears in frame — a car and a truck get the same `VEHICLE_WIDTH_M`.

**What could be done.**
- **Calibrate the camera** — an intrinsics/extrinsics step (checkerboard or ground-plane calibration) removes the single biggest source of systematic bias; right now `CAMERA_HFOV_DEG` is "an explicit assumption... fixed when the camera spec exists," per the project's own future-features register.
- **Class-conditioned width** instead of one fixed `VEHICLE_WIDTH_M` — the detector already classifies objects, so per-class width priors (car vs. truck vs. motorbike) are a small change with a real accuracy payoff.
- **Move off known-width geometry entirely** toward a learned monocular depth model, or — better — toward range that doesn't depend on apparent object size at all: this is exactly what limitation 3 (sensor fusion) would replace.
- **Temporal smoothing** — the current estimate is single-frame; filtering range across the existing track lifecycle (the store already tracks objects over time) would damp the noise a single detection's bounding box introduces.

## 2. Machine learning is not yet applied to the live camera feed

**Current state.** The detector does run a real trained model — YOLO11n over ONNX Runtime's CPU provider ([`ADA_ECU/detector/inference.py`](../../ADA_ECU/detector/inference.py)) — but only against a committed clip file on disk, decoded through `cv2.VideoCapture`, never a live camera. This isn't an oversight so much as an explicitly out-of-scope milestone boundary: "M1 has no radar and no live camera bring-up" ([ada-ecu-hld.md](../Design/MODULE-DESIGN/ADA-ECU/ada-ecu-hld.md)).

**What could be done.** The seam for this already exists in the code, not just on paper: frame acquisition sits behind a `FrameSource` protocol, and the module's own header comment says "a future live source (camera, stream) is one new `FrameSource` implementation" ([`ADA_ECU/detector/frame_source.py:1-8`](../../ADA_ECU/detector/frame_source.py)). So the wiring is cheap; what's genuinely hard is already scoped and quantified in the project's own future-features register under "Live object detection at speed":
- **Detection range must reach ≥130–150 m dry / ≥190 m wet** at highway speed (120 km/h) — derived from braking distance (79 m dry / 139 m wet) plus driver reaction time (1.0–1.5 s) plus a ~300 ms broadcast budget margin.
- **The binding constraint is input resolution, not frame rate** — at the current 640 px inference size, a 1.8 m car at 100 m is only ~10–12 px wide, below the ~20–30 px reliability floor nano-detectors need. Reaching real detection range needs ≥1280–1920 px inference at ≥10 Hz.
- **CPU inference fails outright at that resolution** (≈224 ms/frame @ 1280 px, ≈505 ms @ 1920 px) — live operation needs GPU-class acceleration (TensorRT, ROCm, or OpenVINO are the open candidates already named), which M1 deliberately carries zero dependency on today.

In short: swapping in a live `FrameSource` is a small change; making live detection actually reliable at real vehicle speed is a hardware and resolution problem, already sized by the project itself.

## 3. The calculation needs input from other sensors, such as lidar, instead of relying on computer vision alone

**Current state.** Radar/lidar are explicitly out of scope for M1 — "Radar/lidar hardware is Cortex-M/sensor territory excluded by §1 (F12)," with the bench injecting a LOS-filtered object list directly in its place ([m1-ada-dual-language-study.md](../../requirements/deprecated/requirement-analysis/m1-ada-dual-language-study.md)). Unlike the other three limitations, this one has **no entry anywhere in the future-features register** — it's a real gap in the roadmap, not a deferred, already-planned item.

**What could be done.**
- **Radar before lidar** is the more practical next step: it's cheaper, all-weather-capable (it keeps working in rain, fog and darkness where a camera degrades), and gives velocity directly via Doppler rather than needing frame-to-frame differencing.
- **Lidar** would remove the pinhole-estimation problem in limitation 1 outright — centimeter-level ranging with no dependence on apparent object size or a calibrated FOV constant — at real hardware cost and integration complexity.
- **The architecture already has the seam this needs**, even without the sensor: tracked objects already carry a `Source` (`own_sensor` / `v2x_relayed`); a radar or lidar listener could plug in as a third source into the same track store, alongside the fusion plugins that already consume it (`chained_collision.cpp`) — the diagram slot for a "Radar listener" already exists in the project's own architecture notes, just unrealized.
- **A fusion strategy is a separate design decision** once a second sensor exists — even simple gating (trust radar range, trust camera classification) beats either sensor alone, before investing in a learned fusion model.

## 4. There are other risk categories: unseen obstructions at corners, slippery roads, and foggy conditions

**Current state.** This bullet bundles three items with different status:
- **Corner/intersection blind spots** — already registered future work: "intersection hazards, curve blind spots" are named as future realizations of the same Collision Risk Assessment abstraction M1 already built (R14), meant to "plug in... without reworking existing code."
- **Slippery roads** — also already registered, but as a different message family entirely: "Slippery roads, falling rocks, road holes... carried by DENM (event position + cause code)," not CPM. This is a hazard-notification problem, not a perception problem — ADA doesn't need to *detect* a slippery road itself, it needs to *receive and act on* a DENM.
- **Fog** — not mentioned anywhere in the project's requirements, design decisions, or future-features register. This is the one genuine, undocumented gap of the three.

**What could be done.**
- **Corners** need geometry work, not just a new warning type: today's risk model (`chained_collision`, D5) assumes a straight-line convoy (A behind B behind C); an intersection scenario needs a genuinely different geometric relationship between ego, occluder and the hidden vehicle, and likely a roadside unit (RSU) as the relay source rather than only vehicle-to-vehicle.
- **Slippery roads** are mostly a message-handling problem: extending the R9 dispatch (already described as extensible — "further families enter as a new codec module plus one dispatch entry") to decode DENM alongside CPM, then routing its cause code into the existing warning-emission path.
- **Fog** is the one item needing real new design work. Two independent angles, not mutually exclusive: detect degraded visibility and derate confidence in vision-only detections accordingly, or lean on limitation 3's sensor fusion — radar's biggest practical advantage over camera is exactly that it keeps working in fog.

## Related

- The V2X-side counterpart to this note: [v2x-cpm-limitations.md](v2x-cpm-limitations.md)
- The decisions these limitations sit on top of: [ada-ecu-design-decisions.md](../Design/MODULE-DESIGN/ADA-ECU/ada-ecu-design-decisions.md) D6
