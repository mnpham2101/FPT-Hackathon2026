# Video Source for the R12 Detector

Researcher artifact — environment/platform study, **not an HLD**. It resolves the Phase 2 "video-input study" deliverable of [milestone1.md](../../../plans/milestone1.md) §5 and the § Input constraints open item of the report ("Video format, frame rate, data rate, and capture conditions are not yet known"). It defines **no new requirement numbers** — R1–R19 are frozen; this serves R12.

Diagrams: [video-source-environment.puml](video-source-environment.puml) (environment), [video-source-flow.puml](video-source-flow.puml) (frame-to-track flow).

## 1. Platform finding — CarSky serves no camera content

**Negative finding, stated plainly: CarSky provides no dashcam, camera, or recorded road video to a Container Node.** The platform provides a `video` *transport* and no *content* for it.

| Platform surface | What it actually is | Can it source R12 video? |
|---|---|---|
| `video` pin (Container / Skycraft node) | iceoryx2 shared-memory topic carrying raw **RGBA** frames, `IN`/`OUT`, wired `VIDEO ↔ VIDEO` | No — a transport. Some node in the blueprint must publish the frames. |
| Node types (10 defined) | Container, Script, Skycraft, Ethernet Bridge, Proxy/Outpost, GPIO Panel, KUKSA Broker, CAN Bus, LIN Bus, Device Proxy | No — there is no Camera, Sensor, Replay, or Simulation node type. |
| Artifacts categories | `ANDROID IMAGE`, `AGL IMAGE`, `DBC`, `LDF`, `VSS`, `SCRIPT`, `USB` | No — no video/mp4 artifact category exists. |
| Videos module ("Recorded Video Library") | Storage of screen recordings made by the Devices → Screen widget, downloadable as `.mp4` | No — a sink for VM screen captures, with no documented path back into a blueprint. |
| Screen Stream / Road Simulator widgets | WebRTC viewers of a Skycraft guest's display | No — they consume a VM screen track, they do not produce road footage. |
| Device Proxy + USB disk image | FAT32 `.img` artifact holding arbitrary files, attached to a **Skycraft** node's `usb` pin | No — Device Proxy allows `usb` pins only; it cannot reach a Container Node. |
| Container Node `devices` config | Host device passthrough, e.g. `/dev/video0`, marked "Confirm support on your target cluster" | No — a shared K8s cluster has no vehicle camera attached, and support is explicitly unconfirmed. |
| BTC advisory, §6(3) | "Video source: (i) a prepared clip whose content matches the scenario — **the recommended default**; or (ii) render the POV directly from a road sim" | No — BTC also names no platform-supplied footage; both options are team-produced. |

Two further platform facts that bound the options:

- **No bind mount.** The documented Container Node config fields are `image`, `env`, `exposedPorts`, `capabilities`, `gpu`, `cgroupV2`, `kuksaIntercepts`, `socketcanShim`, `devices`. There is **no volume / host-path / bind-mount field**. A file reaches a Container Node only by being in its image.
- **`VIDEO` pins are API-creatable**, unlike `ETHERNET` — the `addPin` enum is `VHAL|KUKSA|CAN|LIN|VIDEO|GPIO|GENERIC` ([carsky-rest-api-blueprint.md](../../../requirements/car-sky-guide/carsky-rest-api-blueprint.md)). Relevant only if the pin path is ever adopted.

## 2. Candidate comparison

Hard constraints ([solution-selection-criteria.md](../../../.claude/rules/solution-selection-criteria.md)) pass for all three: open-source tooling, Linux-targeted. Ranked criteria C1 accomplishability · C2 fastest for M1 · C3 future features · C4 smaller library.

| # | Candidate | Verdict |
|---|---|---|
| (a) | Live/recorded dashcam video served by CarSky to a Container Node | **Does not exist.** Not evidenced anywhere in the platform doc or the BTC advisory (§1). Disqualified as stated. |
| (a′) | Bench Scenario Player publishes frames on a `video` pin, ADA subscribes | Technically possible, **rejected for M1**. |
| (b) | User-supplied clip baked into the ADA ECU image, path from an env var | **Selected.** |
| (c) | Synthetic clip from `make_sample_video.py` | **Test fixture / fallback only**, not the demo source. |

### Why (a′) is rejected for M1

- **Costs a frozen-contract change.** R5/R6 declare exactly one `ethernet` pin per node into one bridge. A video pin plus its edge on two nodes is an R5/R6 re-freeze — flagged, not absorbed (CLAUDE.md principle 1). Fails C2.
- **No C++ helper.** The `a8_pin` sample layer is Python only; the platform doc states that for `KUKSA` and `VIDEO` pins "there is no equivalent C++ helper yet — read the iceoryx2 shared-memory format (28-byte header + RGBA)". The ADA core is C++17, and the header layout is **not documented**. Fails C1.
- **Raw-RGBA bandwidth is 100× the file path.** 1280×720 RGBA = 3.69 MB/frame; at 20 fps that is 73.7 MB/s (590 Mbit/s) on the Room network, versus ~0.5 MB/s for the same content as an H.264 file read locally. Even 640×360 RGBA at 20 fps is 18.4 MB/s. Contradicts the ADA node's focus goal ("condensed messages, low bandwidth, low latency", report §1).
- **Ordering hazard.** "The Publisher must be created before any Subscriber" — otherwise the shared-memory service sizes its buffer too small and silently truncates frames. A new cross-node startup-ordering dependency on the demo's critical path. Fails C1.
- **Buys only C3**, which the selection rule ranks below C1 and C2. The same C3 value is obtained for free by keeping the detector's frame acquisition behind a frame-source seam (a design point for [[project-architecture]], not a platform choice).

### Why (c) is a fallback, not the source

`ADA_ECU/tools/make_sample_video.py` writes 12 frames of 640×360 at 10 fps, `mp4v` (MPEG-4 Part 2), containing a flat grey rectangle labelled "B" on a dark background. That is a **decoder-and-contract smoke fixture**: it proves `VideoCapture` opens, frames are read, and R3 JSONL leaves stdout. A pretrained COCO detector will not classify a labelled rectangle as `car`, so it cannot produce R12's acceptance evidence ("detection log over the provided clip with per-frame objects and distance estimates"). Keep it for CI; never demo from it.

### Why (b) wins

- **C1 (accomplishability):** it is the path already written into [node-ada-ecu.md](../../../requirements/car-sky-guide/node-ada-ecu.md) ("the provided video clip(s) ship inside the image (`COPY` at build time) — no live video pin is used in M1"), the path the report fixes as scope ("Detects objects only from provided saved video files"), and BTC's recommended default ("app reads the file directly — simplest"). Zero platform unknowns: `open(path)` inside the container.
- **C2 (fastest):** no new pins, no blueprint change, no contract re-freeze, no iceoryx2 reader. One `COPY` line and one env var.
- **C3 (future):** the deferred "camera live feed" item returns as a different implementation behind the detector's frame-source seam; nothing here forecloses it.
- **C4 (smaller library):** OpenCV's `VideoCapture` is already the R12 tech stack; no additional dependency is introduced.

## 3. Video-input spec to build Phase 3 against

This is the Phase 2 deliverable to send to FPT-Mentor. Every row marked *assume* is a **proposal awaiting confirmation** — no authoritative number exists in any source. The content rows are the binding ones: a spec-perfect clip with the wrong content fails R12; an off-spec clip with the right content is re-encodable in one `ffmpeg` command.

| Attribute | Proposed value | Status | Rationale |
|---|---|---|---|
| Container | MP4 (ISO BMFF), `faststart` | assume | Universally decodable by OpenCV's FFmpeg backend; the format the platform's own screen recordings export as. |
| Video codec | H.264 (AVC), Main or High profile, `yuv420p` 8-bit, progressive | assume | Open-source decode via FFmpeg; no runtime encoder needed. |
| Audio | none | assume | Unused; an audio track only costs bytes. |
| Resolution | 1280×720 | assume | YOLO11n letterboxes to 640×640 for inference, so 720p is the smallest source that still leaves headroom for a distant B; 1080p triples decode cost for no detection gain on a CPU-only node. |
| Frame rate | 20 fps, constant | assume | Above the 5 Hz the detector actually consumes, below the decode cost of 30 fps. Constant frame rate keeps `frame_index / fps` a valid timestamp. |
| Duration | 60–120 s | assume | Must contain at least one complete R13 admission cycle (C enters `gate_enter` 30 m, is tracked, leaves past `gate_exit` 35 m) plus lead-in and lead-out. |
| Bitrate / file size | ~4 Mbit/s → ~30 MB at 60 s; **file ≤ 60 MB** | assume | Keeps the added image layer small enough to push to Zot on every rebuild. |
| Detector sample rate | every 4th frame → 5 Hz effective | assume | YOLO11n ONNX CPU reference latency is 56.1 ms at 640; budget ≤ 200 ms per sampled frame on 2 vCPU leaves ~3× margin. |
| **Content — B** | Vehicle B visible and occluding the lane directly ahead in ≥ 90% of frames, at an apparent range of roughly 10–40 m | assume | R12 detects B, the visible occluder; the range band must overlap the R13 gate so the composed `d_AC = d_AB + d_BC` is meaningful. |
| **Content — C** | Vehicle C **never visible in any frame** | **binding** | The premise of the whole use case (report §1, R19). BTC §4 makes the same point about LOS filtering. |
| Viewpoint | Ego (A) forward-facing camera, roughly fixed to the vehicle, near-collinear same-heading convoy | binding | Matches the plan's composition assumption (§2 Scope & Assumptions). |

### Measurable checks (KPIs)

These are R12/R19 evidence, not new requirements:

1. `ffprobe` on the delivered file reports container, codec, resolution, and frame rate matching the table; a preflight step rejects a non-conforming file with the failing attribute named.
2. OpenCV `VideoCapture` inside the `ada-ecu:latest` image opens the clip and reads ≥ 99% of the declared frame count with zero decode errors.
3. Effective inference rate ≥ 5 Hz measured over the whole clip on the deployed node — i.e. wall-clock ≤ 200 ms per sampled frame.
4. Detection log contains ≥ 1 `class = vehicle`, `source = own_sensor` entry with a distance estimate for ≥ 90% of sampled frames.
5. **Zero entries labelled C** across the full run (R12 acceptance, feeds the R19 zero-C check).
6. The clip adds ≤ 60 MB to the ADA ECU image.

### Vague → precise, recorded for veto

- "video format / frame rate / data rate not yet known" → MP4/H.264, 1280×720, 20 fps, ~4 Mbit/s, ≤ 60 MB (§3 table).
- "capture conditions" → ego forward-facing camera, daylight, dry, near-collinear convoy, B at 10–40 m, C never in frame.
- "offline pace acceptable" (plan Phase 3) → effective inference rate ≥ 5 Hz, wall-clock ≤ 200 ms per sampled frame.

## 4. What the user must provide

**One file.** Everything else is already decided by the existing guides.

| Item | Value |
|---|---|
| Deliverable | One ego-POV clip meeting §3, named `ego-b-occluding-c.mp4` |
| Repo path | `ADA_ECU/media/ego-b-occluding-c.mp4` |
| How it reaches the container | `COPY media/ /app/media/` in `ADA_ECU/Dockerfile` — baked into the image; **bind mounts do not exist on a Container Node** (§1) |
| Path inside the container | `/app/media/ego-b-occluding-c.mp4` |
| How the detector learns the path | env var `VIDEO_CLIP_PATH`, set in the blueprint node config, default `/app/media/ego-b-occluding-c.mp4` — never a literal in code (CLAUDE.md principle 5, [node-code-layout.md](../../../.claude/rules/node-code-layout.md)) |
| Companion env var | `DETECTOR_FRAME_STRIDE`, default `4` |

Adding two env vars to the ADA node config is additive to [node-ada-ecu.md § Blueprint node config](../../../requirements/car-sky-guide/node-ada-ecu.md) and touches no frozen contract.

**If no such clip exists**, in preference order: trim a real dashcam recording of a car directly ahead in the same lane; use an openly-licensed driving-POV clip; render an ego POV from a road sim (BTC option ii — costs a 3D renderer and GPU-class load, explicitly discouraged for a shared server). The synthetic generator (§2c) is the last resort and forfeits R12's detection evidence.

## 5. Requirement mapping and flags

| Requirement | Effect of this study |
|---|---|
| R12 | Source resolved: file baked into the image, path from `VIDEO_CLIP_PATH`. Tech stack unchanged (OpenCV decode, YOLO11n ONNX CPU). |
| R3 | Unchanged — the detector still emits R3 JSONL on stdout. |
| R5 | Additive only: two env vars in the ADA node config; image gains one `COPY`. |
| R6 | **Unchanged** — no `video` pin, no new edge. This is the reason (a′) was rejected. |
| R18, R19 | The zero-C check now has a defined input: a clip in which C is never in frame. |
| R11 | Unaffected — the bench stays CPM-only over `ethernet`. |

Flags for the decision owner (user), not silently absorbed:

- **ADA folder is consolidated.** `ADA_ECU/` is the canonical node folder per [node-code-layout.md](../../../.claude/rules/node-code-layout.md). Detector tools and runtime live under `ADA_ECU/`; no lowercase ADA runtime should be used for build, test, demo, or Docker context.
- **The §3 numbers are proposals.** They become authoritative only when FPT-Mentor confirms them or supplies a clip; until then Phase 3 builds against them and re-encodes if a delivered clip differs.
- **No GPU** (report §4). If a delivered clip is 1080p at 30 fps, the stride is raised rather than the model changed.

## 6. Decision for the planner

**The R12 video source is a user-supplied ego-POV clip baked into the ADA ECU image — CarSky supplies no camera or dashcam content, only an unused RGBA shared-memory `video` pin, so there is nothing to consume from the platform.** Build Phase 3's detector against MP4 / H.264 High, 1280×720, 20 fps constant, 60–120 s, ~4 Mbit/s (≤ 60 MB), read from `/app/media/ego-b-occluding-c.mp4` via the `VIDEO_CLIP_PATH` env var with a `DETECTOR_FRAME_STRIDE` of 4 giving 5 Hz effective inference — every one of those numbers is a proposal to confirm with FPT-Mentor, and none of them is binding on the design as long as the frame acquisition sits behind a seam. The user must hand over exactly one artifact: a clip at `ADA_ECU/media/ego-b-occluding-c.mp4` in which vehicle B is visible and occluding the lane ahead at roughly 10–40 m and vehicle C is **never** visible in any frame; `ADA_ECU/tools/make_sample_video.py` stays a CI smoke fixture and must not be used as the demo source. No frozen contract changes: R6 keeps one `ethernet` pin per node, and R5 gains only two env vars on the ADA node.

## Sources

- [Car-Sky-Platform.html](../../../requirements/development-platform-doc/Car-Sky-Platform.html) — node types, pin kinds, Container Node coding (`a8_pin.video`, iceoryx2 RGBA, publisher-before-subscriber), Container Node config fields, Artifacts categories, Videos module, Screen/Road Simulator widgets, USB disk image.
- [BTC_phan_hoi_V2X_team.pdf](../../../requirements/development-platform-doc/BTC_phan_hoi_V2X_team.pdf) — §3 ADA row ("video: read the file directly, or receive a stream over the video pin"), §4 scenario layer and LOS filtering, §6(3) video-source answer, §3 GPU guidance.
- [m1-cooperative-awareness.md](../../../requirements/m1-cooperative-awareness.md) — §1 § Input constraints, R12, §3(g), §4.
- [milestone1.md](../../../plans/milestone1.md) — §2 assumptions, Phase 2 video-input study, Phase 3 acceptance.
- [node-ada-ecu.md](../../../requirements/car-sky-guide/node-ada-ecu.md), [carsky-4-node-blueprint.md](../../../requirements/car-sky-guide/carsky-4-node-blueprint.md), [carsky-rest-api-blueprint.md](../../../requirements/car-sky-guide/carsky-rest-api-blueprint.md).
- [Ultralytics YOLOv8 vs YOLO11 comparison](https://docs.ultralytics.com/compare/yolov8-vs-yolo11/) — YOLO11n ONNX CPU speed 56.1 ms at 640.
