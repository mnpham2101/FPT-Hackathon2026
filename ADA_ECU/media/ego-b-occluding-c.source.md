# Demo Clip Provenance & Attribution

Provenance record for `ego-b-occluding-c.mp4`, the ego-camera clip R12's detector reads. Written for subtask `12.3.7.1` ([phase3_tasks.md](../../plans/phase3_tasks.md)); the encode it describes is `12.3.7.2`.

## Attribution — reproduce this in the report and the presentation

> Dashcam footage: *"Dash Cam View of a Moving Vehicle in a Highway"* by **Mario Angel**, via Pexels — <https://www.pexels.com/video/dash-cam-view-of-a-moving-vehicle-in-a-highway-5915075/>

## Source

| | |
|---|---|
| Title | Dash Cam View of a Moving Vehicle in a Highway |
| Author | Mario Angel |
| Source | Pexels, video ID `5915075` |
| URL | <https://www.pexels.com/video/dash-cam-view-of-a-moving-vehicle-in-a-highway-5915075/> |
| Licence | [Pexels License](https://www.pexels.com/license/) — free use including commercial, modification permitted, attribution not required |
| Retrieved | 2026-08-02 |
| Raw file | `source/pexels-5915075-mario-angel-autobahn.mp4` — **gitignored**, not committed |
| Raw properties | 1920×1080, 27.54 s, H.264 MP4, 20 357 763 bytes |
| Raw SHA-256 | `fc85ff614e3165acc305afb06fe9929340816a3156231ec6aa4295806619dff1` |

The Pexels License does not require attribution. The credit above is given anyway because the clip is redistributed inside every pushed `ada-ecu` image and in a repository, and because the competition report and deck credit their sources.

## The encoded clip

| | |
|---|---|
| Path | `ego-b-occluding-c.mp4` |
| Segment | `t = 6.0 s … 16.0 s` of the raw file |
| Format | 1280×720, 20 fps CFR, H.264 High / yuv420p, ~4 Mbit/s, `+faststart`, no audio |
| Frames | 200 |
| Size | 5 261 876 bytes (~5.0 MB) |
| SHA-256 | `fbe877df1dcd78c2d9df38e08be075c0a0037622f3217ca2ad2383bab4a62346` |

```bash
ffmpeg -ss 6 -t 10 -i source/pexels-5915075-mario-angel-autobahn.mp4 \
  -vf "scale=1280:720,fps=20" -c:v libx264 -profile:v high -pix_fmt yuv420p \
  -b:v 4M -g 40 -movflags +faststart -an ego-b-occluding-c.mp4
```

No crop is applied — the raw frame is already 16:9. The windscreen sticker at bottom-right and the dashboard edge along the bottom are in the source and are left in; they are static and do not overlap the road ahead.

## Content verdict

Verified by extracting all 20 frames at 2 fps across the encoded clip and inspecting each one.

| Criterion | Verdict |
|---|---|
| Forward-facing ego dashcam view | **Pass** — windscreen view over the dashboard, daylight, clear and sharp |
| A vehicle B ahead in the ego lane | **Pass** — a white coach, present in 20 of 20 sampled frames |
| B occluding at ~10–40 m | **Pass** — a continuous approach, B closing from roughly 60 m to roughly 10 m; the closing geometry suits a collision-risk demo better than a fixed-distance follow |
| B is the frontmost visible vehicle in the ego lane | **Pass** — the road beyond B in the ego lane is occluded by B in every sampled frame |
| No vehicle C ever visible | **Pass** — and by construction, not by luck. See § C is synthetic |
| Duration ≥ 60 s | **Fail** — 10 s. See below |

## C is synthetic — what the criterion actually constrains

**There is no vehicle C in this footage, and there could not have been one.** C does not exist in the physical world the clip records. It exists only as a position asserted by the bench's V2X message ([Scenario_Player](../../Scenario_Player/), R11), relayed through the V2X ECU as an R2 object and admitted by the ADA core tagged `v2x_relayed`. The bench invents C; the camera never had the chance to see it.

This reframes the criterion. "C never visible in any frame" is not a property to hunt for in footage — it is guaranteed the moment C is a scenario parameter rather than a filmed object. What the footage must actually avoid is a **decoy**: a vehicle that A's own detector could see *in the ego lane beyond B*, at the range the bench claims C occupies, which would produce an `own_sensor` track colliding with the `v2x_relayed` one and destroy the non-line-of-sight claim.

Three consequences follow, and they are what an implementer needs:

- **Adjacent-lane and oncoming traffic are irrelevant.** They are detected as ordinary `own_sensor` objects at their own bearings and ranges. They are not C, cannot be confused with C, and their presence is realistic — a demo on an empty road would be less convincing, not more.
- **A brief or partial glimpse of something far ahead does not disqualify the clip.** The claim R19 makes is about a *track*, not a pixel: no `own_sensor` track is admitted at C's relayed position. A momentary partial sighting neither survives the R13 admission state machine (`not_tracked → tentative → tracked` needs consecutive hits) nor lands at C's asserted range. Only a vehicle held in the ego lane beyond B, long enough to be admitted, breaks the demo.
- **The occlusion is authored, not discovered.** Because C's position is ours to choose, the bench scenario must place it where B genuinely blocks it. That is a constraint on the scenario config, not on the footage — recorded below.

### What this obliges downstream

| Where | Obligation |
|---|---|
| `Scenario_Player/scenarios/*.yaml` | Place C in the **ego lane, beyond B**, at a range consistent with B's measured distance in this clip — B closes from ~60 m to ~10 m, so C must sit beyond B's range at every instant of the run, and the two must never cross. Emit C only once B is plausibly tracked, so the relay arrives into an established scene |
| `ADA_ECU/tools/check_zero_c.py` (`12.3.5.1`) | Already correct as planned — its **rule 3** is the spatial check: no `own_sensor` track within `ZERO_C_RADIUS_M` of the relayed C position at the same timestamp. Keep it that way. Do **not** add a rule asserting "the detector found nothing but B": adjacent-lane vehicles are expected, and such a rule would fail on correct footage |
| `12.3.5.2` detection log | Expect several `own_sensor` objects per frame, not one. The evidence is the *absence of one at C's position*, not a sparse log |
| R19 evidence narrative | State plainly that C is scenario-injected. The claim being demonstrated is that A warns about a vehicle its sensors never saw — which is exactly true, and stronger for being stated openly than for being implied by an empty road |

### The remaining deviation: the clip is 10 s, not 60 s

The raw file holds 27.5 s, but B is only ahead in-lane between t≈6 s and t≈16 s — before that the coach is not yet the lead vehicle, and after t≈16 s the ego overtakes it. Trimming to that window is what makes every frame satisfy the content criteria.

Because the detector replays a file to simulate a live camera, a longer run is obtained by **looping** the clip rather than by finding longer footage. Each loop reads as a fresh approach cycle: B re-appears at ~60 m and closes again.

## Rejected candidates

22 clips were downloaded and frame-inspected. The three closest, and the row each failed:

| Clip | Why rejected |
|---|---|
| Pexels `35408009` — night highway following a truck | **Best geometry of all 22** — a truck held ahead in-lane for the full 28.8 s with nothing visible beyond it. Rejected on detectability: the truck body is unlit, resolving only as marker lights against black. A brightness/contrast boost recovered no body detail, so YOLO confidence would be unreliable and the footage reads poorly projected |
| Pexels `31901299` — busy urban traffic from a car | Daylight and 51 s, but the ego is queued and near-stationary: no approach dynamics, and several vehicles are visible ahead in the ego lane at once |
| Pexels `5382503` — US highway, 76 s | Long enough, but the ego lane is empty for most of it and the hood mirrors the sky across the lower half of the frame |

The remainder failed on one of four recurring patterns: an empty ego lane (scenic footage is filmed without traffic), an interior/passenger framing rather than a windscreen view, portrait orientation, or dusk glare.

Research datasets containing genuine sustained car-following — comma2k19 and Udacity CH2 (both MIT), KITTI, BDD100K, nuScenes, Waymo and Argoverse (all CC BY-NC or stricter) — were not used: the permissive ones are 100 GB-scale, and the convenient ones would have needed the non-commercial decision recorded below.

## Licence decision of record

The user confirmed on 2026-08-02 that this milestone is a competition entry with no commercial purpose, that non-commercial-licensed material is therefore acceptable, and that credit will be given in the report and the presentation. That decision stands and widens the acceptable-source list for any future clip swap.

It did not have to be exercised here: the Pexels License permits commercial use, so this clip carries no non-commercial restriction into the images it ships in.
