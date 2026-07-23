# Research Note — Per-Node Toolset Support Verification (M1)

> Researcher artifact, **not** an HLD. Verifies that the §3 stack picks in [m1-cooperative-awareness.md](m1-cooperative-awareness.md) support every per-node responsibility in its § ECU and Scenario Player responsibility and every focus goal in its § Cloud development constraints table. Requirement numbers refer to that report (R1–R25); **no new numbers are defined here and none are renumbered**. Environment facts from [BTC letter 09/07/2026](../tmp/BTC_phan_hoi_V2X_team.pdf) (authoritative on CarSky) and [m1-phase1-working-environment.md](m1-phase1-working-environment.md). *Amended 2026-07-12: § Multi-process wake-on-warning added on user follow-up (end-state now mandatory).*

## Verdict summary

| Node | Pick (report §3) | Verdict |
|---|---|---|
| V2X ECU | C++17 (skill-gated F6) + Vanetza ITS2 | **Supported** — full construct+decode coverage; residual risk is team skill (F6), not capability |
| ADA ECU | Python 3.11+ | **Supported** — all responsibilities within Python capacity at the report's KPIs |
| IVI ECU | Kotlin + Compose + SceneView/Filament | **Supported** — see § The Kotlin question; 3D GPU risk is a platform property already gated (R19); one new HUN caveat; mandatory multi-process end-state verified (§ Multi-process wake-on-warning) |
| Bench / Scenario Player | Python + C++ encode component | **Supported** — no mismatch with "emulates the modem's connection point" |

No pick needs reversal. Two caveats fold into existing work items; two amendment texts (R18, F15) are proposed for user ratification (§ Gaps & recommendations).

## The Kotlin question — answered explicitly

**Yes — Kotlin supports the IVI scenario in full.** Every IVI responsibility maps to a first-class Android/Kotlin API on the provided AAOS node (BTC letter §3 confirms the provided IVI node is AAOS):

- **Rendering ADA-sent info:** `java.net.DatagramSocket` receive in a Kotlin service + JSON parse — stdlib-level work; the R4 UDP+JSON path was picked for exactly this receiver (report §3(f)).
- **3D god view:** SceneView/Filament is Kotlin/Compose-native, Apache-2.0, no GMS/ARCore needed ([cache](../.claude/references/sceneview-filament-android-3d.md)). The open risk — Filament needs OpenGL ES 3.x and the virtualized node may fall back to SwiftShader software rendering — is a **platform GPU property, not a Kotlin gap**, and is already bounded by R19's two-stage gate with the 2D Compose-Canvas fallback inside the same view seam.
- **2D god view + 2D ⇄ 3D switching:** Compose `Canvas` + a state-driven toggle behind the R19 view-interface seam — core Compose, no third-party dependency.
- **Switching to another app:** Android task switching + a warning heads-up notification is the R18 pattern; Kotlin/AAOS support it natively with one caveat (HUN category, below).
- **Multi-process wake-on-warning (now a mandatory future feature):** verified end-to-end in § Multi-process wake-on-warning — every element is implementable in Kotlin on AAOS, and Kotlin beats the recorded Flutter costs on this axis.
- **Runs in-browser:** the browser is CarSky's display transport for the whole vECU — invisible to the app; no toolkit impact.

The Flutter-on-Android fallback (F3) remains available but nothing found here triggers it.

## V2X ECU — C++17 (app-level Linux container)

§1 responsibilities and the cloud-table focus goal (portability) against the pick:

| Responsibility | Toolset element | Verdict | Evidence |
|---|---|---|---|
| Configure + interface with the modem (simulated only) | R6 in-node stub FSM, plain C++17 | Supported | BTC letter §2 names the in-node modem stub as the config answer; FSM + acks are language-trivial |
| Connect to the V2X network (simulated or 3rd-party lib) | R5 seam; UDP over the Ethernet pin below it (POSIX sockets or standalone Asio, both open-source/Linux) | Supported | Letter §2: production telux Rx already delivers via socket fd — the hackathon impl is the same read-from-socket shape, clean in C++17 |
| Receive payloads → business logic → forward to ADA | Vanetza ITS2 CPM **decode** + validate/dedupe + JSON forward (e.g. nlohmann/json, MIT) | Supported | Vanetza master ships TS 103 324 v2.1.1 codecs, usable standalone without the GN/BTP stack ([cache](../.claude/references/vanetza-its2-release2-cpm-ts103324.md)) |
| Receive from ADA → construct payloads → broadcast | Same Vanetza asn1c-generated codec, **encode** direction (R8) | Supported | asn1c codecs are bidirectional by construction; R2's golden-vector KPI already requires encode→decode bit-exact round trips, so construct coverage is proven by the committed spike |
| Focus goal: portability across hardware | C++17 = telux linkability; R5 fidelity checklist + port plan | Supported | The pick's driver (report §3(d)); letter §2 "same code, same logic — the evaluated part" |

- **Construct AND decode:** yes — one codec source covers both directions (R2's "single codec source" decision), so bench-encode vs ego-decode cannot drift.
- **Scenario-Player-as-modem-endpoint:** the adapter's Rx socket *is* the emulated connection point (telux parity); implementable in C++17 with stdlib/POSIX alone — Asio optional, architecture's call.
- Residual risk: **team C++ skill, not language capability** — already gated (F6 week-0 hello-world gate, scope cap, Python veto path). The P1 JSON wire keeps the codec off the critical path (R2 staging).

## ADA ECU — Python 3.11+ (app-level Linux container)

| Responsibility | Toolset element | Verdict | Evidence |
|---|---|---|---|
| Fuse V2X info + video-detected objects into the R4 warning via the CRA abstraction | R15 store + R16 plugin interface + R17 engine, pure Python | Supported | 10 Hz × 3 objects with ≤ 1 KB JSON messages is orders of magnitude below single-core Python throughput; report §3(d) translated §1 "high-performance" into the R4/R7/R17 KPIs deliberately |
| Extensibility for future warning types | R16 plugin registry + R4 `warningType` string registry | Supported | Language-agnostic design property; R16's 0-diff plug-in KPI verifies it |
| Camera config + live feed | Not implemented — frozen scope boundary | n/a | §1 exclusion, restated for completeness |
| Detect objects from provided saved video files | YOLO11n ONNX-CPU via onnxruntime (MIT) + OpenCV (Apache-2.0), R21 timeboxed bonus | Supported | Cached benchmark 56 ms/frame CPU vs the ≤ 100 ms KPI ([cache](../.claude/references/yolo11-cpu-inference-benchmarks.md)); Python is the native ecosystem for this stack |
| No Cortex-M sensor/GNSS input | GNSS arrives from the V2X ECU per R9 instead | n/a | User decision 2026-07-10 (F13) |
| Focus goal: condensed messages, low bandwidth, low latency | R4 KPIs (≤ 1 KB @ 10 Hz ≈ 80 kbit/s, ≤ 50 ms p95 same-host) | Supported | UDP emit + JSON serialize in CPython comfortably meets these; KPIs are measured, not assumed (R22/R23 make them computable from logs) |

Python **holds as worded**: no responsibility bullet pushes ADA beyond the translated KPIs, and the only compute-heavy item (R21) is CPU-benchmarked, timeboxed, and off the acceptance path.

## IVI ECU — Kotlin (provided AAOS full vECU)

| Responsibility / focus goal | Toolset element | Verdict | Evidence |
|---|---|---|---|
| Display GUI, render ADA-sent info | Compose shell + UDP foreground service (`DatagramSocket`) | Supported | Kotlin is AAOS's first-class language; R4 receiver is stdlib-level |
| 3D god view, every instance displayed | SceneView/Filament (Apache-2.0, Compose-native) | **Supported-with-caveat** | Virtualized-GPU/SwiftShader FPS unknown until on-node — already the R19 stage-1 spike's purpose ([cache](../.claude/references/sceneview-filament-android-3d.md)) |
| 2D ⇄ 3D view switching | Compose Canvas 2D floor + toggle behind the R19 view seam | Supported | Core Compose; no GL dependency on the 2D side |
| Switching to another app (+ warning surfaces while backgrounded) | Task switching + heads-up notification, tap-return via content intent | **Supported-with-caveat** | Third-party HUNs on AAOS require `IMPORTANCE_HIGH` **and** category ∈ {CALL, MESSAGE, NAVIGATION}; `setFullScreenIntent()` does not auto-launch on AAOS; OEM config may hide nav HUNs ([cache](../.claude/references/aaos-notifications-hun-foreground-service.md)) |
| Multi-process wake-on-warning (mandatory future feature) | Orchestrator `startActivity` wake + zygote-forked processes | Supported (future) | Verified in § Multi-process wake-on-warning; Kotlin ≥ Flutter on every cost recorded in § Future developments |

Caveat mechanics, both manifest/config-level (not design risks):

- **HUN category:** R18's heads-up warning must ship with `IMPORTANCE_HIGH` + `CATEGORY_NAVIGATION` (closest drive-relevant fit) and a content-intent tap-return (the R18 flow already matches, since AAOS never auto-launches full-screen intents). Verify on the starter-pack image that nav HUNs aren't OEM-hidden; escape hatches: privileged install on the team-controlled node, or `CATEGORY_CALL` styling.
- **Foreground-service typing:** if the provided image/target is API 34+, the UDP service needs a declared `foregroundServiceType` (+ matching permission); below that, just `FOREGROUND_SERVICE` + `INTERNET`. The image API level is already an open starter-pack fact in R18's dependency line.

## Multi-process wake-on-warning — mandatory end-state verification (added 2026-07-12)

User follow-up: the § Future developments "Multi-process front end with wake-on-warning" pattern is a **mandatory** future feature, not a nice-to-have — M1's R18 single-app design must grow into it without rework. Verified below; evidence cached in [android-background-activity-launch-wake-patterns.md](../.claude/references/android-background-activity-launch-wake-patterns.md).

### End-state capability on Kotlin/AAOS — verdict: yes, every element

| End-state element | Kotlin/AAOS mechanism | Verdict |
|---|---|---|
| Home app owning outer frame/buttons | Compose shell app; optionally installed as the HOME/launcher activity | Supported |
| Composition of separate apps | Separate APKs or `android:process` split — native platform model; visual composition by full-screen task switch (unprivileged) or embedded TaskView / Android 13+ `allowUntrustedActivityEmbedding` (needs privileges or embed-side opt-in) | Supported; embedding variant supported-with-caveat |
| Sleeping app wakes on obstruction event | **Sanctioned wake mechanism: the foreground orchestrator calls `startActivity` on the warning app's exported activity** — an app with a visible window starts activities without restriction, so no exemption is needed; the explicit intent carries the R4 event payload and cold-starts the sleeping process | Supported |
| Wake fallbacks if the shell isn't guaranteed foreground | (a) home-as-launcher — launcher-initiated launches are an explicit BAL exemption; (b) platform-signed/priv-app install + `START_ACTIVITIES_FROM_BACKGROUND` on the team-controlled image; (c) HUN tap (user-in-loop, the M1 R18 pattern) | Supported (image facts to confirm) |
| `setFullScreenIntent()` / HUN limits found earlier | Irrelevant to the primary wake path — wake goes through the foreground orchestrator, not a notification | n/a |
| Cold start in the 100s-of-ms range | Native Android cold start is the platform baseline (a few hundred ms for a small app, measurable via `reportFullyDrawn`); Flutter adds engine/Dart init on top of the same process start | Supported — figure holds or improves vs § Future developments |
| Per-process RAM cost | Zygote copy-on-write fork shares framework/ART pages — extra Kotlin process ≈ its private heap (tens of MB for a small Compose app); Flutter costs a per-app engine instance on top | Supported — strictly lighter than the recorded Flutter cost |

### M1 foundation seams (requirements-level — implementation belongs to [[project-architecture]])

For the later split to be a refactor, not a rewrite, R18's single-app build must ship these seams now:

1. **UI-free R4 ingest module** — the UDP receive + parse path has no compile-time dependency on any UI/shell code; it speaks only the R4 contract. (Process-agnostic by construction.)
2. **Exported intent entry point for the warning/awareness view** — the view is reachable via an explicit intent whose extras are the serialized R4 event; **M1's internal `idle ⇄ warning-view` transition routes through this same entry point**, so the future orchestrator uses the door M1 already exercises.
3. **View behind the R19 seam consuming only R4-derived state** — already a binding R19 obligation; restated here because it is what makes the view relocatable to another process.
4. **No shared in-process mutable singletons between shell and warning view** — all shell→view state crosses as serialized R4 payloads (intent extras / re-sent state), never as shared memory.
5. **`warningType` registry as data, not code** (already an R4 KPI) — readable identically from any process.
6. **Notification plumbing kept** (M1 HUN path) — doubles as the user-in-loop wake fallback in the end-state.

Measurable foundation check (proposed): the awareness-view module compiles with **0 imports from the shell module** (CI dependency check), and the warning view launches standalone via `adb shell am start` with an R4-event extra (binary — proves the exported entry point works from outside the app's process).

### Impact on the report — proposed amendments (for user ratification; this note edits nothing in the report)

- **R18 — append:** *"Foundation obligation (user decision 2026-07-12 — multi-process wake-on-warning is a mandatory future feature): the warning/awareness view sits behind an exported intent entry point carrying the R4 event as serialized extras, the R4 ingest lives in a UI-free module, and no in-process singletons are shared between shell and view; the M1 internal `idle ⇄ warning-view` transition routes through the same entry point. KPI: view module has 0 compile-time imports from the shell module (CI dependency check); warning view launches standalone via `adb shell am start` with an R4-event extra (binary)."*
- **F15 — replace with:** *"F15 — Multi-process IVI front end: implementation deferred; end-state MANDATORY (upgraded from a nice-to-have deferral by user decision 2026-07-12). M1 ships the R18 foundation obligation so the split is a refactor, not a rewrite; the M1-visible slice remains app switching + heads-up warning. Wake mechanism recorded: foreground-orchestrator `startActivity` (unprivileged, primary); home-as-launcher exemption or privileged `START_ACTIVITIES_FROM_BACKGROUND` as fallbacks; HUN tap as the user-in-loop path."*
- **F3 (Kotlin over Flutter) — stands, reinforced; no wording change needed.** The mandatory end-state strengthens the pick: § Future developments already records Flutter's multi-process costs (per-app engine RAM, 100s-of-ms cold start); Kotlin processes zygote-fork with shared framework pages (lighter per process) and cold-start without an engine layer (equal or faster), and the orchestrator/BAL machinery is native Android territory. Kotlin is at least as good as Flutter on every cost dimension of this axis — the F3 decision record needs no amendment.

## Bench node — Scenario Player (Python + C++ encode component)

| Responsibility | Toolset element | Verdict | Evidence |
|---|---|---|---|
| Emulate the Quectel modem's connection point toward the V2X ECU | Python stdlib UDP emit into the R5 adapter socket | Supported | Production telux Rx = app reads payload from a socket (letter §2); the bench writing datagrams to that socket path *is* the connection point — no semantic mismatch |
| Simulate decoded V2X messages at different rates | R10 config-driven engine + rate multipliers; timing in Python | Supported | 10 Hz ± 10% (R11 KPI) needs ~10 ms precision — well within CPython `sleep`/monotonic-clock accuracy on Linux; "decoded" already superseded by R11's vague→precise (bench emits wire format, ego decodes — closer to production) |
| CPM emission in wire encoding | Vanetza ITS2 encode via the C++ component (daemon or pre-encoded replay CLI — shape delegated to architecture, report §3(a)); P1 JSON path is pure Python | Supported | Same single codec source as the ego decoder; Python↔C++ integration is subprocess/local-socket-trivial on Linux |
| Optionally simulate modem configuration responses | Scripted acks over a control channel (Python) | Supported (optional) | The committed config answer is the in-node R6 stub (BTC's default); the letter's "control channel to bench" alternative stays open at trivial Python cost |
| Impairment (loss/delay/jitter) | R12 pre-emission engine, seeded, in Python | Supported | BTC-recommended placement; no `tc netem`/NET_ADMIN dependency |
| LOS filter + GNSS feed | R13 geometry + R9 UDP JSON fixes | Supported | Straight-line kinematics (letter: hand-computed trajectories suffice) |
| Focus goal: playback across different scenarios + god view/dashboard | Config-driven scenario set (R10 KPI) + SSE/polling page (R14) | Supported | SSE is plain HTTP chunking — servable from Python (stdlib/aiohttp/Flask, all open-source); zero WebSocket per D3/F4 |

## Gaps & recommendations

No gap forces a pick change. **Recommendation: keep all four picks.** Items for attention:

1. **AAOS HUN category restriction (new finding)** — add to R18's week-1 bring-up checklist: channel `IMPORTANCE_HIGH` + `CATEGORY_NAVIGATION`, content-intent tap-return, and an on-image check that nav HUNs aren't OEM-hidden. Owner: [[project-architecture]] (design detail inside R18); no user ratification needed — it changes no pick, scope, or KPI.
2. **Foreground-service typing at API 34+** — manifest-level; resolves automatically once the starter pack states the image API level (already an (A) in R18's dependency line).
3. **Proposed R18/F15 amendments (user ratification required)** — § Multi-process wake-on-warning: R18 gains the foundation obligation + KPI; F15 upgrades the end-state from nice-to-have to mandatory while keeping implementation deferred. F3 stands unamended.
4. Existing gated risks are unchanged and re-confirmed as the right gates: Filament-on-virtualized-GPU (R19 two-stage gate), team C++ skill (F6), UPER codec integration (R2 spike + F9 fallback).

## Diagrams

- [m1-node-toolset-support-component.puml](m1-node-toolset-support-component.puml) — the four nodes with their toolsets, per-responsibility components (R-numbered), the contract links between them, and the exported warning-view entry point (F15 foundation door).
- [m1-node-toolset-support-activity.puml](m1-node-toolset-support-activity.puml) — swimlane view of one warning cycle, each step annotated with the tool that carries it (R10 tick → R19 render).

## Open items

- Verify on the starter-pack AAOS image: nav-HUN visibility, image API level, sideload/install path (all inside R18's existing dependency line) — plus, for the F15 end-state fallbacks: whether privileged/platform-signed install and the launcher (HOME) role are assignable on the provided node.
- The R2 cross-decode spike and F6 hello-world gate remain the empirical proof for the C++/Vanetza path — this note verifies capability, not team execution; the gates verify that.
- User ratification of the proposed R18/F15 amendment texts (§ Multi-process wake-on-warning).

## Sources

- [BTC letter 09/07/2026](../tmp/BTC_phan_hoi_V2X_team.pdf) — CarSky node model, seam/socket semantics, provided AAOS IVI node, bench-node role.
- [m1-cooperative-awareness.md](m1-cooperative-awareness.md) §1–§4 — responsibilities, cloud table, picks, KPIs, flags.
- [Notifications on Android Automotive OS — developer.android.com](https://developer.android.com/training/cars/platforms/automotive-os/notifications) (checked 2026-07-12; cached: [aaos-notifications-hun-foreground-service.md](../.claude/references/aaos-notifications-hun-foreground-service.md)).
- [Restrictions on starting activities from the background — developer.android.com](https://developer.android.com/guide/components/activities/background-starts) (checked 2026-07-12; cached with cost figures: [android-background-activity-launch-wake-patterns.md](../.claude/references/android-background-activity-launch-wake-patterns.md)).
- Cached evidence: [vanetza-its2-release2-cpm-ts103324.md](../.claude/references/vanetza-its2-release2-cpm-ts103324.md), [sceneview-filament-android-3d.md](../.claude/references/sceneview-filament-android-3d.md), [yolo11-cpu-inference-benchmarks.md](../.claude/references/yolo11-cpu-inference-benchmarks.md).
