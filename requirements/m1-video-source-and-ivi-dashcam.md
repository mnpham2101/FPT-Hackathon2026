# Video Source for Phase 3, and Whether the Dashcam Feed Can Reach Both ADA and IVI

Researcher artifact — a [dev-environment-research](../.claude/skills/dev-environment-research/SKILL.md) run against the CarSky platform, **not an HLD**. It defines **no new requirement numbers**: R1–R19 are frozen and this serves R12, R5, R6, and R16.

It answers four user questions of 2026-08-02 and **extends** [video-source-for-r12.md](../documents/KnowledgeBase/video-source-for-r12.md), which already settled the ADA-side source. That note stands unchanged; this one adds the platform detail it did not carry, the ADA↔IVI fan-out question it never asked, and a concrete answer on how the file physically reaches a deployed container. Cross-node because it spans ADA (R12), IVI (R16), and the blueprint (R5/R6).

Diagrams: [m1-video-source-topology.svg](m1-video-source-topology.svg) — blueprint with the video path drawn on, also as [.drawio](m1-video-source-topology.drawio) and [.puml](m1-video-source-topology.puml) · [m1-video-source-flow.puml](m1-video-source-flow.puml) — clip to R19 evidence. Every diagram carries the legend below.

**Verification pass, 2026-08-02.** Every platform claim below was re-checked against [Car-Sky-Platform.html](development-platform-doc/Car-Sky-Platform.html) after the first draft — all 68 API paths enumerated, the Container Node config table read field by field, and whole-file string counts run for `volume`, `configMap`, `hostPath`, `mtu`, `bandwidth`, `latency`, `fan-out`, `quota`. **The recommendation in §9 is unchanged.** Six statements were corrected or sharpened and are marked **[v]** where they appear: §2 (the `container-file` API; the container-side `usb` pin), §3 (a new candidate (f)), §4 (the worked Container↔Container video edge; the `a8_pin` disclaimer), §5 (multicast attribution).

## 0. Reading the R-numbers

Every `Rn` in this document is a **requirement ID from [the report](m1-cooperative-awareness.md) §2** — there is no second numbering scheme for messages. The report orders them "contracts first (R1–R6), then data-flow order", and that split is the whole source of the apparent ambiguity:

- **R1–R6 are the contract requirements.** Each one's *deliverable is itself a schema or a topology*, so the project names the artifact by its requirement number — "the R2 message", "an R3 object", "the R4 warning". R5 and R6 are contracts too, but of deployment and network rather than of a message.
- **R7–R19 are behavioural requirements.** The deliverable is running code. R13 is one of these: the track store and its admission state machine.

So R3 and R13 are the same *kind* of thing — both requirements — and differ only in what they oblige. R3 obliges a schema every perception source conforms to; R13 obliges the state machine that decides when an object in that schema becomes `tracked`. R3 defines the `state` field; R13 defines what moves it.

## 1. The four answers, up front

| # | Question | Answer |
|---|---|---|
| 1 | Provide a dashcam video and run detection for Phase 3 using a CarSky tool | **No CarSky tool does this.** The platform's only in-Room video facility is the `video` pin — a raw-RGBA shared-memory *transport* with no content behind it — plus a Videos library that is a **sink** for VM screen recordings with no upload path and no API. The clip is team-supplied. Detection runs entirely inside ADA (OpenCV decode → YOLO11n ONNX CPU). |
| 2 | Can the video stream to ADA and IVI at once, with the IVI showing a dashcam view | **Yes as "both show the same footage", no as "one live stream fanned out".** The `video`-pin fan-out is unusable here (no C++ helper, undocumented frame header, the Skycraft side documented in exactly one sentence, and every worked video edge in the doc labelled **1-to-1**). Because the source is a fixed recording, the correct shape is **one file, two independent readers** — ADA reads its baked-in copy; the IVI plays the same clip over HTTP from the ADA node, or a local copy. |
| 3 | How to construct the blueprint and the connections | **For the recommendation: no topology change at all.** Four role nodes plus the bridge, one `ethernet` pin each, exactly as today — the ADA node gains two env vars, and (only if the dashcam view is accepted) one `exposedPorts` entry. Snippets in §6. |
| 4 | Would the video need uploading, or be encoded somehow, when deploying | **Baked into the image at `docker build` time.** One `COPY media/ /app/media/` line, placed as its own early layer. A 60 s 720p20 H.264 clip is ~30 MB: it adds ~30 MB to the image, uploads once, and is layer-cached on every later push. There is **no volume and no declarative upload**; a post-deploy `container-file` API does exist but is not the deploy path (§5). Numbers, ceilings and the git question in §5. |

## 2. Environment model — every video surface CarSky exposes

All rows traced to [Car-Sky-Platform.html](development-platform-doc/Car-Sky-Platform.html); quotes are the doc's own words.

| Surface | What it actually is | Serves R12? |
|---|---|---|
| `video` pin — Container Node | "shared-memory video topic for RGBA IN/OUT"; Python `a8_pin.video(...)` over **iceoryx2**; wired `VIDEO ↔ VIDEO` only | Transport only — some node in the blueprint must publish the frames. **No content.** |
| `video` pin — Skycraft Node | "Camera/video input forwarded into the guest" — the **entire** doc treatment. No wiring example, no counterpart node named, absent from the node's own "Typical wiring" block | No, and unusable: see §4 |
| Videos module ("Recorded Video Library") | Storage of clips made by the Devices → Screen widget's Recorder Part, downloadable as `.mp4` | No — a **sink**. The doc's verbs are record / play back / download / delete; there is no Add or Upload action, and **[v] the string "video" appears zero times across all 68 `/api/v1/...` paths** |
| Artifacts categories | `ANDROID IMAGE`, `AGL IMAGE`, `DBC`, `LDF`, `VSS`, `SCRIPT`, `USB` — seven, no others | No — there is no video/media/blob category, and the only artifact a Container Node consumes is its own `image` reference |
| **[v]** Device Proxy + FAT32 `.img` | "Any file can be placed in the disk image… `mcopy -i ota.img demo.mp4 ::/`", with a tip to raise the image to 512 MB–1 GB for many videos. `mediaImageDir` is "mounted into both the device-proxy and **Skycraft** pods" | **A real second path, evaluated and rejected — candidate (f), §3.** The Device Proxy page says "Connect the `usb` OUTPUT pin … to the `usb` INPUT pin of a **Skycraft** Node", but two other pages admit a container: the wiring rules say "Skycraft/**Container** has a `usb` INPUT pin", and the Container Node's own pin list says "**usb** — Connect to a Device Proxy Node for USB forwarding" |
| Container Node `devices` | "Host device passthrough (e.g. `/dev/video0`). **Confirm support on your target cluster.**" | No — a shared K8s cluster has no vehicle camera, and support is explicitly unconfirmed |
| **[v]** `POST /api/v1/deployments/:roomId/container-file/:nodeKey` | "Read/write file in container." The doc's **only** documented way to put a file into a container without rebuilding it | Not the deploy path — post-deploy, imperative, and undocumented (no schema, no size limit, no example). Useful as a *patch* channel, not as the source of truth: §5 |
| Scout/probe SDK (`capture-video`, `lens`) | Edge-hub capture: V4L2 → H.264 Annex B → UDS → lens → WebRTC. The doc is emphatic that this is "**two separate wire protocols, by pin kind**" — media pins bypass the tether protocol entirely | No — needs a physical hub with a camera, and the in-blueprint Proxy/Outpost node's pin list is `can, lin, gpio, ethernet, tunnel, audio`: **`video` is absent even though `audio` is present** |
| BTC advisory §6(3) | "(i) a prepared clip whose content matches the scenario — **the recommended default**; or (ii) render the POV from a road sim" | No — the organizers name no platform-supplied footage either; both options are team-produced |

Three further facts that bound every option below:

- **[v] No volume, no bind mount, no declarative file injection.** The documented Container Node config is exactly eleven fields — `image`, `command`, `args`, `env`, `exposedPorts`, `capabilities`, `gpu`, `cgroupV2`, `kuksaIntercepts`, `socketcanShim`, `devices` — and nothing else. Whole-file counts: `configMap` 0, `persistent` 0, `PVC` 0, `hostPath` 0, `emptyDir` 0, "bind mount" 0; every `mount` hit is the USB disk image, the guest's `/sdcard/Music/usb_1`, or `cgroupV2`'s "Mount `/sys/fs/cgroup` RW". A file reaches a container **declaratively** only inside its image — the sole escape hatch is the imperative `container-file` API above.
- **The bridge is an L2 broadcast domain of unmeasured capacity.** "It behaves like an unmanaged software switch." **[v] Confirmed by count: `mtu` 0, `bandwidth` 0, `latency` 0, `throughput` 0, `jumbo` 0 occurrences in the entire platform doc** — no figure exists to design against. Our own [deploy walkthrough](car-sky-guide/deploy-walkthrough-netcheck.md) warns "the bridge is a tunnelled fabric, so 1500 bytes is not guaranteed" and gives a `PAD=1400` bisect procedure.
- **`VIDEO` pins are API-creatable, `ETHERNET` pins are not** — the `addPin` enum is `VHAL|KUKSA|CAN|LIN|VIDEO|GPIO|GENERIC` ([carsky-rest-api-blueprint.md](car-sky-guide/carsky-rest-api-blueprint.md)). The pin nobody wants can be scripted; the pin everyone needs is a manual UI step. Curiously, **VIDEO does not appear in the doc's own Concepts → Pin Types table at all** — it exists only in the node pin lists, the `+ Add Pin` picker, and the wiring rules.

## 3. Decision A — where R12's frames come from at runtime

Hard constraints pass for every candidate (open-source, Linux). Ranked criteria: **C1** accomplishability · **C2** fastest for M1 · **C3** future features · **C4** smaller library.

| # | Candidate | Verdict |
|---|---|---|
| (a) | A CarSky-provided camera or dashcam facility | **Does not exist** — §2. Stated plainly rather than shortlisted |
| (b) | Clip baked into the ADA OCI image, path from `VIDEO_CLIP_PATH` | **Selected** |
| (c) | Clip fetched at container start (HTTP/S3/git-lfs from outside the Room) | Rejected |
| (d) | A separate video-source Container Node publishing on a `video` pin | Rejected for M1 — §4 |
| (e) | Synthetic clip from `tools/make_sample_video.py` | CI fixture only, never the demo source |
| **[v]** (f) | Clip in a FAT32 `.img` USB artifact, attached via a Device Proxy Node to an ADA `usb` INPUT pin | **Rejected** — new to this verification pass |

**Why (b) wins.** C1: zero platform unknowns — `open(path)` inside the container, on the path the report ("detects objects only from provided saved video files"), the [node guide](car-sky-guide/node-ada-ecu.md) and BTC's recommended default all already assume. C2: one `COPY` line and one env var; no new pin, no blueprint change, no contract re-freeze. C4: OpenCV `VideoCapture` is already in R12's tech stack, so nothing new is pulled in. C3 costs nothing because frame acquisition already sits behind the `FrameSource` seam ([ADA decision D6](../documents/Design/ADA-ECU/ada-ecu-design-decisions.md#d6--r12-detector-frame-source-seam-inference-distance-zero-c-evidence)) — a live source arrives later as one new implementation.

**Why (c) is rejected.** It trades a build-time cost for a demo-time network dependency: the node must reach an external host during startup, on a platform where a failed pull already manifests as a node stuck in `Provisioning`. It also makes the image non-self-describing — the same tag behaves differently depending on what the remote served. Fails C1 outright, and buys nothing on C2 since the image must be rebuilt and pushed anyway.

**[v] Why (f) is rejected.** It is the closest thing the platform has to "upload a video file", and it is genuinely documented end to end — `truncate` → `mkfs.vfat` → `mcopy -i ota.img demo.mp4 ::/` → register as an `USB` artifact → attach from inventory. It still loses on all of C1, C2 and C4:

- **The doc contradicts itself on whether a container may be the sink.** The Device Proxy Node's own page says the target is "the `usb` INPUT pin of a **Skycraft** Node" and that each pair attaches "only one USB device at a time". Two other pages allow a Container. The most specific page is the restrictive one, so the container path is unverified — exactly the class of risk C1 exists to avoid, four working days out.
- **It costs a topology change.** A fifth node (Device Proxy) plus a new `usb` pin and edge on ADA — an R5/R6 re-freeze, against zero for option (b). Fails C2.
- **It adds a mount point, an inventory step and an attach step** to the demo's critical path, all after deploy, to deliver a file that `COPY` delivers before deploy.
- **What it would genuinely buy** is swapping the clip without rebuilding the image. That is worth having *later*, and it is precisely what the `container-file` API (§5) already offers with no topology change at all.

**Why (e) cannot be the source.** It writes 12 frames of a flat grey rectangle labelled "B". A pretrained COCO detector will not classify that as a car, so it cannot produce R12's acceptance evidence. It proves the decoder and the JSONL contract, and nothing else.

## 4. Decision B — one feed, two consumers

The question is really two: *can the platform fan a video stream out to a Container Node and a Skycraft Node*, and *is a stream what this demo needs at all*.

| # | Mechanism | Assessment |
|---|---|---|
| B1 | `video` pin: one publisher, VIDEO edges to ADA and IVI | **Infeasible in M1.** Four independent blockers below |
| B2 | UDP unicast ×2 over the R6 bridge, app-level fragmentation | Rejected — writes RTP in miniature against an unmeasured MTU |
| B3 | UDP multicast over the bridge | Rejected — same work as B2 plus an unverified assumption |
| B4 | ADA serves its own clip over HTTP; IVI plays it with Media3 | **Selected, if the dashcam view is accepted at all** |
| B5 | IVI plays a local copy of the same file | Fallback — same code as B4, one config value different |

**B1's blockers**, each sufficient on its own:

- **No C++ path, and the Python path is not an SDK. [v]** The doc: for KUKSA and VIDEO pins "there is no equivalent C++ helper yet — use … the iceoryx2 shared-memory format (**28-byte header + RGBA**)". The header layout is never documented. The ADA core is C++17. And the `a8_pin` sample the Python side would use carries its own disclaimer — "**a reference pattern, not an official CarSky SDK** … write an equivalent layer" — so *neither* language gets a supported client; one gets sample code, the other gets a sentence describing a binary layout.
- **The Skycraft side is one sentence.** "Camera/video input forwarded into the guest" — and that is the complete text. Its sibling pins each name a counterpart ("vhal — … a Script Node acting as VehicleServer", "kuksa — … a KUKSA Databroker node", "ethernet — … an Ethernet Bridge", "usb — … a Device Proxy node"); `video` names none. The node's "Typical wiring" block lists four lines and video is not among them. Nothing states what AAOS then sees — no virtual camera, no `/dev/video*`, no Camera HAL anywhere in the doc. There is nothing here to build against.
- **Bandwidth inverts the node's purpose.** 1280×720 RGBA is 3.69 MB/frame → **73.7 MB/s (590 Mbit/s)** at 20 fps; even 640×360 is 18.4 MB/s. Against ~0.5 MB/s for the same content as an H.264 file read locally. The ADA node's focus goal is condensed, low-bandwidth, low-latency messaging.
- **Fan-out cardinality is unverified — the doc is silent, not permissive. [v]** iceoryx2 itself supports one publisher to many subscribers, with `max_subscribers` fixed at service creation; CarSky exposes no `video` pin config schema at all. The AEB tutorial *does* carry a worked Container↔Container video edge (`bench.front` OUTPUT → `algo.front` INPUT, enabled by `docker build --build-arg WITH_VIDEO=1`), so the Container-to-Container half is exemplified — but it is labelled "**1-to-1 video channel**" in both places it appears, and whole-file counts for `fan-out`/`fanout`/"one publisher" are **0**. Tellingly, the same tutorial *does* spell out shared-bus semantics for the other pin kinds ("Both the Test Bench and the Algorithm publish into the **same** KUKSA broker and the **same** CAN bus") and offers no analogous sentence for video. Add the standing ordering hazard ("the Publisher must be created before any Subscriber… otherwise the shared-memory video service initializes its default buffer too small, **silently truncating frames**") and this lands on the demo's critical path as an unbounded risk. Fails C1 and C2.

**Why B2/B3 are rejected.** Video over UDP means fragmenting each frame into ~1200-byte datagrams — MJPEG at 720p is roughly 100–150 KB/frame, so ~100 datagrams per frame at 20 fps — plus reassembly, loss handling and a decoder on the Android side. That is a small RTP stack, written against an MTU nobody has measured, for a phase that has not started. **[v]** Multicast adds an assumption the Ethernet Bridge page never states — the bridge is described only as a "virtual L2 **broadcast** domain" and the word multicast never appears on its page. Multicast is *implied* to work by two unrelated pages: **Zenoh**'s automatic "peer+multicast mode" fallback when no router container is present (that is Zenoh's session mode riding on the `ethernet` pin, not KUKSA's databroker), and vsomeip's `nydus.vsomeip.add_multicast_route(group, "eth0")`. The `nydus.net` UDP helper's `join_multicast` binds `0.0.0.0`, not a named interface. Android additionally needs a `MulticastLock` on the guest.

**Why B4 wins, if the view is built at all.** The ADA image is based on `python:3.11-slim`, so `python3 -m http.server` is already present — no new dependency, no new node, no second copy of the file. TCP removes the MTU question entirely; a `+faststart` MP4 is exactly what a progressive-download player wants, and that flag is already in the input spec. Media3/ExoPlayer plays it natively. The IVI's media URI is a config value, so switching to B5 (a local copy in the APK, zero network at demo time) changes one string and no code — that is the fallback, kept for the case where the guest cannot reach `10.99.0.12` reliably. `exposedPorts` additionally puts the same feed on a browser route ("Create gateway routes at `/{room}/{node}/{name}/`"), which BTC §7 explicitly endorses as the way to give the jury a second screen.

**The honest framing, which the demo narrative depends on.** The source is a recording, not a camera. "Streaming" it to two consumers conveys no information that two independent readers of the same file do not — and no frame-level synchronisation between the ADA detection and the IVI picture is achievable anyway, because the detector loops the clip at EOF (`DETECTOR_LOOP`) and runs as fast as the CPU allows. **If the dashcam view is built, real-time pacing of the detector's `FileFrameSource` stops being optional** — without it the picture and the warnings visibly drift apart within one clip length. The R19 claim ("zero direct C detections") is proven by the detection log and `check_zero_c.py`, never by what the video surface shows.

## 5. The encoding and upload question, concretely

**There is no upload at deploy time.** No volume, no bind mount, no configMap, no artifact category for media, no Videos upload. The file is a layer in the OCI image, and `docker push` is the transfer.

**[v] The one exception, and why it is not the answer.** The API catalog documents `POST /api/v1/deployments/:roomId/container-file/:nodeKey` — "Read/write file in container" — alongside `POST /api/v1/deployments/:roomId/container-exec/:nodeKey`. That is a genuine post-deploy file channel into a running container, and it is worth knowing about. It is not the deploy path, for three reasons: it is **imperative and post-deploy**, so a redeployed Room loses the file and the demo gains a manual step on its critical path; it is **entirely undocumented** — no request schema, no size limit, no example anywhere in the doc; and it makes the running node differ from its own image tag, which is the same non-self-describing failure that rejected candidate (c). **Use it for what it is good at:** swapping a clip during a rehearsal without a 30 MB rebuild-and-push cycle, and pulling the detection log off a node. Never as the source of truth for what ships.

**The Dockerfile line, and where it goes.** Order the copies so the rarely-changing bytes land in their own early layer — an unchanged clip is then never re-uploaded on any later push:

```dockerfile
# media and model: change rarely, pushed once, cached thereafter
COPY media/  /app/media/
COPY models/ /app/models/
# code: changes every commit
COPY detector/ /app/detector/
```

**What it costs.**

| Quantity | Value | Basis |
|---|---|---|
| Clip size | ~30 MB at 60 s, ~60 MB at 120 s | 1280×720, H.264, ~4 Mbit/s |
| Added image size | ≈ the file size | H.264 is already compressed; the layer's gzip gains ~0% |
| First push | ~12 s at 20 Mbit/s up, ~48 s at 5 Mbit/s | 30 MB / link rate |
| Later pushes | **0 bytes** for that layer | Zot skips blobs it already holds, if the layer digest is unchanged |
| Share of the ADA image | small — an estimate to measure, not a claim | `python:3.11-slim` plus `onnxruntime` + `opencv-python-headless` + `numpy` dominates; the clip is a minority of the total |

A 60 s / 720p / 20 fps clip is comfortably tolerable. The point of leverage is duration, not resolution: doubling the length doubles the bytes, while dropping 1080p→720p was already decided for inference cost.

**Is there a size ceiling?** **Not documented anywhere** — no image-size, layer-size, push-quota or storage-quota value appears in the platform doc, and the registry section's troubleshooting table lists only auth and visibility failures, never a size failure. Two things bound the risk: the platform's own walkthrough shows a real artifact version carrying `image 731.9 MB` + `host_package 463.6 MB` — ~1.2 GB uploaded successfully — so hundreds of megabytes are evidently routine there; and the REST API exposes `GET /api/v1/config/limits` ("View active resource limits") and `GET /api/v1/account-limits`, which is the check that would turn this from unverified into a number (§10). Neither endpoint's response schema is documented, so the values must be fetched, not read.

**Should the clip be committed to git?** Yes, once. The repo already commits binary media — `presentation/assets/*.jpg|png`, `requirements/*.png`, `*.svg` — and `.gitignore` excludes nothing media-related; the ADA HLD already plans to commit `models/yolo11n.onnx` (~10 MB) for the same reason (build-time reproducibility on a node with no volume). The one real cost is permanence: git keeps every blob forever and a re-encoded clip is a full second copy, not a delta. So **iterate the encode locally and commit exactly one final file**. Git LFS would solve it and is not worth a remote-storage dependency six days from the deadline (C2).

## 6. Blueprint construction

Shapes match [blueprint-m1-cooperative-awareness.json](car-sky-guide/blueprint-m1-cooperative-awareness.json); node config is stored flat, per [carsky-rest-api-blueprint.md](car-sky-guide/carsky-rest-api-blueprint.md).

**Recommended — the ADA node, additive env only. No new pin, no new edge, no new node.**

```json
{
  "id": "ada-ecu",
  "label": "ADA ECU",
  "nodeType": "container",
  "config": {
    "image": "registry.hackathon-2.carsky.io/m1-ada-ecu:latest",
    "command": ["./entrypoint.sh"],
    "capabilities": ["NET_RAW"],
    "env": {
      "V2X_LISTEN_PORT": "47200",
      "IVI_ECU_HOST": "10.99.0.13",
      "IVI_ECU_PORT": "47300",
      "GATE_ENTER_M": "30",
      "GATE_EXIT_M": "35",
      "VIDEO_CLIP_PATH": "/app/media/ego-b-occluding-c.mp4",
      "DETECTOR_FRAME_STRIDE": "4"
    }
  },
  "positionX": -450,
  "positionY": 180,
  "pins": []
}
```

The `pins: []` is not an omission — `ethernet` pins cannot be created by import or by the REST `addPin` enum, so the single `ETHERNET`/`OUTPUT` pin at `10.99.0.12` is added by hand in the Nydus canvas afterwards ([node-ada-ecu.md](car-sky-guide/node-ada-ecu.md)). The topology stays the star of [carsky-4-node-blueprint.md](car-sky-guide/carsky-4-node-blueprint.md): bench `10.99.0.10`, V2X `10.99.0.11`, ADA `10.99.0.12`, IVI `10.99.0.13`, all one `ethernet` pin into the bridge at `10.99.0.1`.

**Optional (B4) — same node, serving the clip.** Two env rows and one `exposedPorts` entry; the `ethernet` pin already carries it inside the Room, and the gateway route is the bonus browser view:

```json
"env": {
  "CLIP_HTTP_ENABLED": "true",
  "CLIP_HTTP_PORT": "8080"
},
"exposedPorts": [ { "name": "clip", "port": 8080, "protocol": "http" } ]
```

**[v] The gateway URL shape is documented twice, inconsistently** — the Container Node reference says routes appear at `/{room}/{node}/{name}/` (by port *name*), while the AEB tutorial says `https://<host>/conduit/http/<room-namespace>/<node-key>/<port>/` (by port *number*, with a `/conduit/http` prefix). Only `protocol: "http"` is ever shown. This matters only for the jury's browser view, not for the in-Room path the IVI uses; resolve it by observation once a Room is up (§10).

**The IVI side carries no blueprint config**, and that is a finding in itself: nothing in the Skycraft node config injects environment into the guest app — its config block is the VM image artifact reference and nothing else. IVI-side tunables are therefore `buildConfigField` values in `IVI_ECU/app/build.gradle.kts` — the pattern the app already uses for `WARNING_TIMEOUT_MS`:

```kotlin
buildConfigField("String", "DASHCAM_MEDIA_URI", "\"http://10.99.0.12:8080/ego-b-occluding-c.mp4\"")
```

Switching to B5 replaces that one string with a raw-resource URI. No other change.

**For completeness, the shape that is *not* proposed** — a VIDEO pin. It is creatable over `/batch`'s `addPin`, unlike `ETHERNET`, but its `properties` schema is undocumented, which is part of why it is rejected:

```json
{ "name": "cam0", "pinType": "VIDEO", "direction": "OUTPUT", "properties": {} }
```

## 7. Requirement mapping, feasibility, and measurable outputs

No new requirement numbers. Verdicts per [requirement-quality-criteria.md](../.claude/rules/requirement-quality-criteria.md).

| Item | Serves | Verdict | Reasoning |
|---|---|---|---|
| Clip baked into the ADA image, read via `VIDEO_CLIP_PATH` | R12, R5 | **achievable** | One `COPY` line and two env vars; already the path the report, the node guide and BTC assume |
| Detection at 5 Hz effective, CPU-only, `linux/arm64` | R12 | **achievable, one at-risk dependency** | `onnxruntime` / `opencv-python-headless` aarch64 wheels must resolve at image build; a missing wheel means a source build under QEMU (ADA HLD open item 6) |
| `video` pin into ADA | R12, R6 | **infeasible in M1** | No C++ helper, undocumented frame header, 590 Mbit/s at 720p20, ordering hazard |
| `video` pin into the AAOS guest | — | **infeasible / unverifiable** | One sentence of documentation, no wiring example, nothing stated about what the guest sees |
| USB `.img` via Device Proxy into ADA (candidate f) | R5, R6 | **at-risk, rejected** | Container-side `usb` INPUT contradicted between doc pages; costs a fifth node and an R5/R6 re-freeze to replace one `COPY` line |
| `container-file` API as the deploy path | R5 | **at-risk, rejected** | Undocumented schema and limits; post-deploy and lost on redeploy. Sanctioned only as a rehearsal-time patch channel |
| UDP video (uni- or multicast) over the bridge to both | R6 | **at-risk, rejected** | App-level fragmentation against an unmeasured MTU, plus a decoder on Android |
| IVI dashcam view via HTTP from ADA (B4) | R16 (surface), no requirement of its own | **achievable but out of M1 scope** | ~0.5–1 day: Media3 dependency, one Compose surface, one `DisplayMode`, a `http.server` line in the entrypoint |
| Real-time pacing of the detector's frame source | R12 | **achievable**; becomes **mandatory** if the dashcam view is built | Otherwise the IVI picture and the ADA warnings drift within one clip length |
| Blueprint/topology change | R5, R6 | **none required** | The recommendation touches no pin and no edge |

**Measurable outputs (KPIs).** Items 1–6 are inherited from [video-source-for-r12.md §3](../documents/KnowledgeBase/video-source-for-r12.md); 7–11 are new to this study.

7. `docker image inspect` shows the media layer ≤ 60 MB, and its digest is unchanged across two consecutive builds that do not touch `media/` — proving the layer cache holds.
8. The second `docker push` of the ADA image reports the media blob as already present (0 bytes transferred), and the first is timed once and recorded.
9. The deployed ADA node reaches `Running` and its `[EVT]` log shows `detector_spawn` followed by `own_sensor_ingest` lines — i.e. the baked-in clip opened on the real node, not just locally.
10. *If the dashcam view is built:* the IVI renders the clip at **≥ 15 fps at 1280×720 for ≥ 60 s continuous**, measured with `adb shell dumpsys gfxinfo <pkg>` (janky-frame percentage recorded), **and** the R4-to-overlay latency is unchanged from the video-off case within the existing R17 budget.
11. *If the dashcam view is built:* over a 60 s run, the drift between the frame shown on the IVI and the frame the detector is processing stays under **2 s**, with the detector paced to the clip's declared frame rate.

**Vague → precise, recorded for veto.**

- "stream the video to ADA and IVI at the same time" → both nodes present the same clip within one run; **no frame-level synchronisation is claimed or measured**, only a ≤ 2 s drift bound if the view is built.
- "IVI provides dashcam view" → an R16 Display-area mode rendering the ego clip at ≥ 15 fps / 720p for ≥ 60 s, with the R17 warning overlay composited above it.
- "simulate the live video" → the detector reads the file through the `FrameSource` seam and loops at EOF; frames are paced to the clip's declared rate only when the dashcam view is built, otherwise consumed as fast as the CPU allows at ≥ 5 Hz effective.
- "would the video need uploading" → no deploy-time upload facility exists; the transfer is `docker push` of one image layer.

## 8. Scope flag — the IVI dashcam view is not in M1

**It is deferred scope, and pulling it in is the user's decision, not this note's.** The report lists "**Ego video clip display on the IVI** — the provided ego-POV clip (B occluding the view ahead) plays in the Display area (R16) … **Deferred from M1 for time**; the Display area already hosts video feeds by design, so this is an added surface, not a rework" under § Future developments, mirrored in the [future-features register](future/m1-future-features-register.md) and restated in the report's §4 decision record. R16's acceptance covers the layout and the Display area's mode switching; R17's covers the warning view. Neither mentions a camera view, and [Phase 5's acceptance criteria](../documents/Plan/milestone1_high_level_plan.md) do not either. [node-ivi-ecu.md](car-sky-guide/node-ivi-ecu.md) records the same boundary as the reason the IVI wires no `video` pin.

Two ways to accept it, both requiring the user's word:

- **Additive and timeboxed** — the lighter option, and the recommended one. R16 already states the Display area "also serves video feeds", and `DisplayMode.kt` already exists, so a video mode is an extension of a shipped surface rather than a new requirement. Condition: it gates nothing, it appears in no acceptance criterion, and **it is not started until Phase 3 and Phase 5 acceptance are green**.
- **A new requirement number** — if the user wants it tracked with its own acceptance criteria and task IDs. That needs a full [requirement-analysis-and-solutioning](../.claude/skills/requirement-analysis-and-solutioning/SKILL.md) run promoting it out of § Future developments; an environment-research note may not mint requirement numbers.

Not flagging this would be silently absorbing a deferred item into M1 six days before the deadline, against CLAUDE.md principle 3.

## 9. Recommendation

**Do Decision A now and change nothing else.** Ship the user-supplied ego-POV clip inside the ADA ECU image via one `COPY media/ /app/media/` layer, read through `VIDEO_CLIP_PATH`, exactly as [video-source-for-r12.md](../documents/KnowledgeBase/video-source-for-r12.md) already specified — the blueprint keeps its four role nodes, one `ethernet` pin each, and no `video` pin anywhere. **Criteria C1 and C2 drove this**: it is the only option with zero platform unknowns, and it costs one Dockerfile line against a Phase 3 that has not started with six days left. The two file-delivery alternatives the verification pass surfaced — the USB/Device-Proxy artifact (f) and the `container-file` API — are both real, and both lose to `COPY` on C1 and C2; keep `container-file` in mind as a rehearsal-time clip-swap trick, not as the deploy path.

**Treat the IVI dashcam view as a separate, deferred decision.** If the user accepts it as additive and timeboxed, build it as **B4** — the ADA node serves its own clip over HTTP on `exposedPorts`, the IVI plays it with Media3 behind the warning overlay, and the media URI is a config value so the offline local-copy variant (B5) is one string away. **Criterion C1 drove that pick too**: it is the only fan-out mechanism with no undocumented platform behaviour in its path, where the `video` pin has four independent blockers and UDP video means writing RTP against an MTU nobody has measured. Do not start it until Phase 3 and Phase 5 are green, and if it is built, pace the detector to real time so the picture and the warnings do not drift.

## 10. Open items — what could not be verified, and the check that would settle it

| # | Unverified | Check |
|---|---|---|
| 1 | **Zot image/layer size limit and any storage quota** — no value appears in the platform doc; only a ~1.2 GB artifact observed succeeding | `GET /api/v1/config/limits` and `GET /api/v1/account-limits` with the REST key; failing that, push the real ADA image and observe |
| 2 | **Ethernet Bridge MTU** — never stated; zero MTU/bandwidth/latency figures exist in the whole doc | The `PAD=1400` bisect in [deploy-walkthrough-netcheck.md](car-sky-guide/deploy-walkthrough-netcheck.md); only matters if B2/B3 are ever revisited |
| 3 | **Multicast over the bridge** — implied by Zenoh's peer+multicast fallback and vsomeip's `add_multicast_route`, never stated for the bridge itself | Join a group from two containers in a deployed Room and send; only matters for B3 |
| 4 | **`video` pin fan-out cardinality** — no pin config schema exists; both worked examples are labelled 1-to-1 and the doc is silent on fan-out | Create one VIDEO OUTPUT pin with two edges over `/batch` and call `validate`; not worth doing unless B1 is revived |
| 5 | **What a Skycraft `video` pin delivers into AAOS** — one undocumented sentence, no counterpart node named | Would need BTC/FPT-Mentor to answer; treat as unavailable until they do |
| 6 | **Whether the AAOS guest can reach `10.99.0.12:8080` over TCP** — R4 proves UDP ingress on that guest, not TCP egress | `adb shell curl` (or an in-app probe) against the ADA node once a Room is up; the B5 fallback exists precisely for a negative answer |
| 7 | **Media3/ExoPlayer H.264 playback on this AAOS image** — the node config sets `gpuBackend: virglrenderer` at 1920×1080, but decode support is untested | Install a throwaway APK playing the clip and read `dumpsys gfxinfo`; a software-decode fallback is the mitigation |
| 8 | **aarch64 wheels for `onnxruntime` / `opencv-python-headless`** — assumed, not proven | The first ADA image CI build (already ADA HLD open item 6) |
| 9 | **The clip itself does not exist yet** — it is a user deliverable and gates all Phase 3 evidence | The user supplies `ADA_ECU/media/ego-b-occluding-c.mp4` per the input spec |
| 10 | **Registry host drift** — [node-ada-ecu.md](car-sky-guide/node-ada-ecu.md) still says `registry.carsky.io` while Phase 0 verified `registry.hackathon-2.carsky.io` | Editorial fix in the node guide; unrelated to video but surfaced while checking push cost |
| 11 | **[v] `container-file` API semantics** — no request schema, no size limit, no example anywhere in the doc | `POST /api/v1/deployments/:roomId/container-file/:nodeKey` against a live Room with a small file, then a 30 MB one; only needed if the clip-swap trick is adopted |
| 12 | **[v] Whether a Container Node really accepts a `usb` INPUT pin** — the Device Proxy page says Skycraft only; the wiring rules and the Container Node pin list say Container too | Wire one in the canvas and run `validate`; only matters if candidate (f) is ever revived |
| 13 | **[v] The `exposedPorts` gateway URL shape** — documented two incompatible ways (`/{room}/{node}/{name}/` vs `/conduit/http/<room-ns>/<node-key>/<port>/`) | Deploy with one `exposedPorts` entry and read the route the Deployment Viewer publishes; affects only the jury's browser view |

## Sources

- [Car-Sky-Platform.html](development-platform-doc/Car-Sky-Platform.html) — Container Node config fields and pins; Container Node Coding (`a8_pin.video`, iceoryx2 RGBA, publisher-before-subscriber, "no equivalent C++ helper yet", the "not an official CarSky SDK" disclaimer); Skycraft Node pins and Typical wiring; Ethernet Bridge; Artifacts categories; Videos module; Device Proxy Node and Build a USB Disk Image; Registry (Zot); SDK overview (Scout/probe/lens); Proxy/Outpost pin kinds; API & MCP catalog (`container-file`, `container-exec`, `config/limits`, `account-limits`); the AEB tutorial (both worked VIDEO edges, `WITH_VIDEO=1`, `exposedPorts` routes); live walkthrough Step 5 (the 731.9 MB / 463.6 MB artifact).
- [BTC_phan_hoi_V2X_team.pdf](development-platform-doc/BTC_phan_hoi_V2X_team.pdf) — §3 ADA row and GPU guidance, §4 scenario layer and LOS filtering, §5 split-screen god view, §6(3) video source, §7 serving a page from a container node.
- [m1-cooperative-awareness.md](m1-cooperative-awareness.md) — §1 node roles and § Input constraints, § Future developments (ego video clip display, deferred), §4 decision record, R5, R6, R12, R16, R17.
- [milestone1_high_level_plan.md](../documents/Plan/milestone1_high_level_plan.md) — §2 assumptions, Phase 3 and Phase 5 acceptance, §6 deferred scope.
- [video-source-for-r12.md](../documents/KnowledgeBase/video-source-for-r12.md) and [ada-ecu-hld.md](../documents/Design/ADA-ECU/ada-ecu-hld.md) — the ADA-side source decision, the `FrameSource` seam (D6), and the deployment shape (D9).
- [phase2-4-pr3-review.md](../plans/doc/phase2-4-pr3-review.md) — the true starting state of Phase 3: the detector is a placeholder that decodes frames and discards them, and the core reads a static JSONL file once at startup.
- [carsky-4-node-blueprint.md](car-sky-guide/carsky-4-node-blueprint.md) · [carsky-rest-api-blueprint.md](car-sky-guide/carsky-rest-api-blueprint.md) · [node-ada-ecu.md](car-sky-guide/node-ada-ecu.md) · [node-ivi-ecu.md](car-sky-guide/node-ivi-ecu.md) · [deploy-walkthrough-netcheck.md](car-sky-guide/deploy-walkthrough-netcheck.md) · [zot-registry-api-key.md](car-sky-guide/zot-registry-api-key.md).
- [iceoryx2 publish-subscribe](https://docs.rs/iceoryx2/latest/iceoryx2/) and [v0.8.0 release notes](https://ekxide.io/blog/iceoryx2-0.8-release/) — one publisher to many subscribers is supported natively, with `max_subscribers` fixed at service creation; CarSky exposes no configuration for it.
