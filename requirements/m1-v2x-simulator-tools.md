# Research Note — V2X Simulator & Modem-Emulation Tools (M1, cloud-only)

> Researcher artifact, **not** an HLD. Surveys 3rd-party tools that could emulate a C-V2X modem/GNSS device or generate/replay V2X message streams for the bench-node scenario player, under the cloud-only decision of 2026-07-08 (no hardware in any part of the project). Requirement numbers refer to [m1-cooperative-awareness.md](m1-cooperative-awareness.md); no new numbers are defined here. Environment model: [m1-phase1-working-environment.md](m1-phase1-working-environment.md); hardware variant (deferred): [m1-product-fit-quectel-modem.md](m1-product-fit-quectel-modem.md).

## Research question

- **(1) Device-level emulation:** does any open-source tool emulate the C-V2X modem/GNSS device (QMI/AT interface, GNSS feed) that R6's provider would otherwise talk to?
- **(2) Message-level simulation (user-preferred fit):** does any open-source tool generate/replay realistic V2X message streams (CAM/CPM/DENM/BSM) that could replace or augment the team-written scenario player ([working-env note, deliverable 1](m1-phase1-working-environment.md))?
- **Bar for adoption:** a 3rd-party generator is worth taking only if it is cheaper or more credible than extending the already-planned team scenario player, or adds realism (trajectories, traffic) the team would otherwise script by hand.

## Diagram

- [m1-v2x-simulator-tools.puml](m1-v2x-simulator-tools.puml) — the bench→ego pipeline's tool slots (trajectory source, message builder/codec, impairment, GNSS feed, conformance cross-check) and which surveyed tool maps to each; rejected layers annotated.

## Findings

Verdict basis: [solution-selection-criteria.md](../.claude/rules/solution-selection-criteria.md) — hard constraints (open-source, Linux) applied first, then C1 implementability, C2 M1 speed, C3 future features, C4 smaller lib. Layer legend: *device* (modem/GNSS interface), *stack* (GN/BTP/facilities), *message* (payload generation/codec), *scenario* (traffic/trajectory simulation).

| Tool | Layer | License | TS 103 324 CPM | Verdict |
|---|---|---|---|---|
| Vanetza `socktap` | stack + message | LGPL-3.0 | No — TR 103 562 shape only; no release through v26.06 (2026-06) mentions TS 103 324; issue #194 closed with no visible merged PR | **Reject** as bench generator (existing "could use — upgrade path" row in the [working-env note](m1-phase1-working-environment.md) unchanged) |
| Artery | scenario + stack | GPL-2.0, **but OMNeT++ dependency is Academic Public License** (commercial use requires paid OMNEST license) | Partial, mainly via forks/extensions | **Reject** — disqualified (OMNeT++ fails open-source hard constraint); also discrete-event, not real-time injectable |
| ms-van3t / VaN3Twin | scenario + stack + message | GPL-2.0 | **Yes — CPS per ETSI TS 103 324 V2.1.1** (only surveyed tool at the standard revision); CAM/DENM/IVIM/VAM too | **Reject** as scenario-player replacement (C2 + R13/R18 conflicts, §below); **could use** later as interop rig / background-traffic source |
| V2X-Hub (USDOT) | stack (infrastructure) | Apache-2.0 | No — SAE J2735 (US) message set; translator/aggregator, not a generator | **Reject** — wrong message family (R1 targets ETSI CPM; J2735 paywalled) |
| CARLA + OpenCDA | scenario | CARLA MIT; **OpenCDA non-commercial research license**, last release v0.1.2 (2022-03) | No — internal formats; ETSI CPM only via 3rd-party research glue | **Reject** — OpenCDA disqualified (not open source); CARLA needs GPU-class hosting the cloud nodes don't have |
| gpsd + gpsfake | device (GNSS) | BSD | n/a | **Could use** — unchanged from the [working-env note](m1-phase1-working-environment.md): standard-interface GNSS variant for R6; direct position feed stays primary |
| gps-sdr-sim | device (GNSS RF) | MIT | n/a | **Reject** — generates I/Q baseband for SDR hardware (bladeRF/HackRF/USRP); no radio or SDR exists anywhere in the environment |
| srsRAN / OAI sidelink | baseband (PC5 PHY/MAC) | AGPL/OAI licenses | n/a | **Reject** — NR sidelink mode 2 is research-grade in open stacks; radio layer is explicitly out of scope and out of judging (FPT-Mentor) |
| python-dbusmock (ModemManager template) | device (modem D-Bus) | LGPL-3.0+ | n/a | **Reject for M1** — mocks ModemManager D-Bus enough for `mmcli`, but no M1 component talks to ModemManager (cloud branch = direct provider; deferred hardware path plans `qmicli`); revisit at the hardware milestone |
| SUMO (standalone) | scenario (trajectories) | EPL-2.0 | n/a (feeds trajectories, not messages) | **Could use** — optional trajectory feed into the team scenario player for background traffic/scale tests; not for the M1 demo scenario (R18 clip consistency, §below) |
| pycrate | message (codec) | LGPL-2.1, actively maintained (pycrate-org since 2024-02) | Compiler + UPER runtime; ships precompiled ETSI ITS modules (CAM_2, DENM_3, VAM_3); CPM TS module compile from ETSI Forge **unverified** | **Should use** (narrow role) — independent decoder cross-checking asn1tools-encoded bytes at the R1 freeze and in CI |
| ika-rwth-aachen `etsi_its_messages` | message (codec, ROS wrapper) | MIT | **Yes** — `etsi_its_cpm_ts_coding` is asn1c-generated C for CPM TS 103 324 V2.1.1; coding packages are plain CMake C libs, ROS not required | **Could use** — fallback independent decoder if pycrate's compiler rejects the CPM modules; also a worked proof that TS 103 324 UPER round-trips |
| Eclipse MOSAIC | scenario (co-sim) | EPL (historic Fraunhofer-license parts migrating) | CAM generation documented; CPM not evident | **Reject for M1** — same heavy-framework class as Artery/ms-van3t (C2); recorded for completeness |

### Why no generator replaces the team-written scenario player

- **R13/R17 content rule:** Phase 5's relayed CPM must carry real Phase-4 perception output from the TrackedObject store — no off-the-shelf generator ingests our store; every framework generates CPM content from its own simulated sensors/traffic.
- **R18 ground-truth consistency:** scenario trajectories and CPM content must match the provided clips' ground truth — SUMO/random-traffic-driven generators cannot reproduce externally-recorded trajectories.
- **Impairment placement:** BTC-recommended option (b) puts config-driven drop/delay/jitter inside the scenario player with a reproducible seed — native to our player, foreign to the frameworks.
- **Effort asymmetry (C2):** two scripted vehicles + asn1tools encode ≈ days; ns-3/OMNeT++ framework bring-up ≈ 1–2+ weeks, and the custom content plumbing above would still have to be written.
- **Framing mismatch:** stack-level tools (Vanetza, ms-van3t) emit GN/BTP-framed packets; the ego contract is bare UPER payload over the thin send/recv adapter ([control/data-plane note](m1-phase1-control-data-plane.md)) — adopting one forces GN/BTP parsing onto the ego path that no requirement asks for.
- **Credibility gap, closed cheaply:** the one real weakness of a homegrown generator — both ends sharing the asn1tools codec, so a shared codec/module bug passes silently — is fixed by an independent decoder (pycrate, fallback ika coding package) at ~a day of effort, not by adopting a framework.

## Recommendation for R6

- **(a) Mock GNSS/modem provider — keep as planned, nothing replaces it.** No open-source device-level C-V2X modem emulator exists at the QMI/AT layer; python-dbusmock's ModemManager template is the closest and mocks an interface no M1 component uses. Direct position feed from the scenario player stays primary; gpsd+gpsfake stays the optional standard-interface variant; gps-sdr-sim is the wrong layer entirely. The dbusmock template is recorded as a future-milestone option alongside the [product-fit note](m1-product-fit-quectel-modem.md)'s hardware path.
- **(b) Scenario player — keep team-written, augment with a conformance cross-check.** Adopt **pycrate** (fallback: ika `etsi_its_cpm_ts_coding`) to independently decode the player's emitted CPM/CAM bytes at the R1 freeze and in CI — this buys the interop credibility a 3rd-party generator would have provided, at a fraction of the cost. SUMO is the sanctioned middle path if background-traffic realism is ever wanted (trajectory feed into our player, not a player replacement); ms-van3t is the recorded future interop rig if standard-stack GN/BTP interop ever becomes a judged criterion.

## Open items

- Verify pycrate's ASN.1 compiler accepts the ETSI Forge CPM TS 103 324 modules — first task of the cross-check adoption; on failure, fall back to ika's pre-generated `etsi_its_cpm_ts_coding`.
- ms-van3t's emulation mode is documented for CAM/DENM over real interfaces (UDP mode available); CPM-over-emulation unverified (docs page rate-limited during this research) — re-check only if the interop-rig option is ever activated.
- Vanetza issue #194 closed without a visible merged PR — if a future Vanetza release ships TS 103 324 CPM, the upgrade-path row in the [working-env note](m1-phase1-working-environment.md) improves; re-check at next Vanetza release.

## Requirement mapping

| Requirement | Impact of this note |
|---|---|
| R1 | Adds the independent-decoder cross-check (pycrate / ika) as conformance evidence for the frozen CPM contract |
| R3, R4 | Bench emission/logging stays with the team scenario player; no 3rd-party generator adopted |
| R6 | Cloud branch confirmed tool-complete: direct feed primary, gpsfake variant, no device emulator exists or is needed; dbusmock recorded for the hardware milestone |
| R13, R17 | Content-source rules (real Phase-4 output, zero-mock path) are what exclude every 3rd-party generator |
| R18 | Clip-consistent ground truth excludes SUMO-driven trajectories for the demo scenario; SUMO allowed for background traffic only |

## Sources

- [ms-van3t / VaN3Twin — multi-stack ETSI-compliant V2X framework for ns-3](https://github.com/ms-van3t-devs/ms-van3t) and [ms-van3t documentation — modules (CPS per TS 103 324 V2.1.1)](https://ms-van3ts-documentation.readthedocs.io/en/master/Modules.html) and [sample V2X emulator application](https://ms-van3ts-documentation.readthedocs.io/en/master/Emulation.html)
- [Vanetza releases (v26.06, 2026-06-23 — no TS 103 324 CPM)](https://github.com/riebl/vanetza/releases) and [issue #194 — adding CPM of ETSI TS 103 324 v2.1.1](https://github.com/riebl/vanetza/issues/194)
- [Artery — OMNeT++ V2X simulation framework](https://github.com/riebl/artery), [Artery CPS discussion #249](https://github.com/riebl/artery/discussions/249) and [OMNeT++ license (Academic Public License; commercial via OMNEST)](https://omnetpp.org/intro/license)
- [USDOT V2X-Hub (Apache-2.0, J2735 handler)](https://github.com/usdot-fhwa-OPS/V2X-Hub)
- [OpenCDA (non-commercial research license; releases end v0.1.2, 2022-03)](https://github.com/ucla-mobility/OpenCDA) and [OpenCDA releases](https://github.com/ucla-mobility/OpenCDA/releases)
- [gpsd project](https://gpsd.gitlab.io/gpsd/) and [gpsfake(1)](https://gpsd.gitlab.io/gpsd/gpsfake.html); [gps-sdr-sim (MIT, SDR RF output)](https://github.com/osqzss/gps-sdr-sim)
- [python-dbusmock (ModemManager template)](https://github.com/martinpitt/python-dbusmock) and [ModemManager mock PR #205](https://github.com/martinpitt/python-dbusmock/pull/205)
- [SUMO — outputs incl. FCD trajectories](https://sumo.dlr.de/docs/Simulation/Output/index.html) and [TraCI Python interfacing](https://sumo.dlr.de/docs/TraCI/Interfacing_TraCI_from_Python.html)
- [pycrate (LGPL-2.1, ASN.1/UPER, precompiled ETSI ITS modules)](https://github.com/pycrate-org/pycrate)
- [ika-rwth-aachen/etsi_its_messages (MIT, CPM TS 103 324 V2.1.1 coding)](https://github.com/ika-rwth-aachen/etsi_its_messages) and [etsi_its_cpm_ts_msgs on ROS index](https://index.ros.org/p/etsi_its_cpm_ts_msgs/)
- [Eclipse MOSAIC](https://eclipse.dev/mosaic/) and [Eclipse MOSAIC project entry (license migration)](https://projects.eclipse.org/proposals/eclipse-mosaic)
- [ETSI TS 103 324 V2.1.1 — Collective Perception Service](https://www.etsi.org/deliver/etsi_ts/103300_103399/103324/02.01.01_60/ts_103324v020101p.pdf) and [official ASN.1 modules — ETSI Forge](https://forge.etsi.org/rep/ITS/asn1/cpm_ts103324)
