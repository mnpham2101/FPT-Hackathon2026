# Research Note — Phase 1 Control-Plane vs Data-Plane V2X Messaging (M1)

> Researcher artifact, **not** an HLD. Analyses how the CONTROL plane (modem/service bring-up and configuration) and the DATA plane (the V2X PDUs themselves) should be handled in Phase 1, in the CARSKY cloud environment and on the Quectel hardware variant. Builds on [m1-phase1-working-environment.md](m1-phase1-working-environment.md) and [m1-product-fit-quectel-modem.md](m1-product-fit-quectel-modem.md); requirement numbers refer to [m1-cooperative-awareness.md](m1-cooperative-awareness.md) (R1–R18); **no new numbers are defined here.** Companion diagram: [m1-phase1-v2x-call-flow-sequence.puml](m1-phase1-v2x-call-flow-sequence.puml).

## 1. The two planes, defined for this project

| Plane | What travels on it | Endpoint pair | Standardized wire format? |
|---|---|---|---|
| **Control plane** | Radio/modem bring-up, V2X mode + PC5 flow/service-ID configuration, GNSS/service status | Host (Cortex-A) ↔ modem — or, on cloud, service ↔ its own config/environment | **No (vendor/host-local).** QMI/AT + telux API on hardware; nothing ETSI-standard — it never crosses the air |
| **Data plane** | The V2X PDUs peers exchange: CAM (ego state) + CPM (perceived object C) | ITS station ↔ ITS station (bench scenario player ↔ ego `V2X_Comm`) | **Yes.** ASN.1 UPER facilities payloads over GeoNetworking/BTP (ETSI) |

The planes must not be conflated: control-plane traffic is host-local and vendor-shaped; data-plane bytes are peer-visible and standard-shaped. The transport adapter seam committed for R3/R4 is exactly the line between them — control plane lives in the adapter's *init/config*, data plane in its *send/recv byte stream*.

## 2. Q1 — Message format per plane in Phase 1

### Data plane: ASN.1 UPER CAM/CPM (recommended); GN/BTP framing per §3's feasibility result

- **Facilities payloads:** CAM per ETSI EN 302 637-2 (release 2: TS 103 900) and CPM — normative target TS 103 324 per R1, with the known Vanetza TR 103 562-shape delta still open (F-HW3 / working-env finding 2). Encoded **ASN.1 UPER** — bit-identical to what a real peer stack or the Quectel path would deliver (validated layer-by-layer in [m1-product-fit-quectel-modem.md](m1-product-fit-quectel-modem.md) §3).
- **Network/transport framing:** the standard framing is GeoNetworking + BTP (EN 302 636-4-1 / -5-1); CAM on BTP destination port **2001** (TS 103 248 port registry), CPM on port **2009** (assigned for the Collective Perception Service in TS 103 324). **Refined by §3:** in Phase 1 the GN layer is *not* implemented — payloads travel in plain UDP datagrams with at most the 4-byte BTP-B header for port semantics; full GN/BTP is the documented upgrade path inside the transport adapter.
- **Phase 1 carrier:** UDP (multicast or unicast per BTC's transport confirmation) as the bench→ego channel; **both sides encode/decode with Python `asn1tools`** against the same official ETSI ASN.1 modules — interoperability by the standard, not by a shared library.
- **Criteria justification:** C1 — the UPER facilities bytes are the invariant layer: the same bytes hardware will deliver, so nothing above the adapter is bench-only; C2 — codec-only removes all stack bring-up work (see §3); C3 — the hardware swap stays adapter-only (F-HW2), and standard ASN.1 (a §6 deferred item) is brought forward at near-zero cost because the modules are published; C4 — asn1tools is a small, single-purpose library.
- **Flag (decision owner: user, at the R1 contract freeze).** [m1-cooperative-awareness.md](m1-cooperative-awareness.md) §3(b) picked **JSON mirroring CPM** — a pick premised on the pre-mentor environment model (two team-run vECUs, hand-rolled UDP stub, plan §2's "if hand-rolling, JSON is acceptable"). The mentor's bench-node model moved the committed route to standard encoding, and plan §2 says: *if a real V2X stack is used, the standard ASN.1 encoding is free — use it.* The supersession chain to ratify at the R1 freeze is: **JSON (report §3(b)) → UPER via Vanetza GN/BTP (previous revision of this note) → UPER codec-only over UDP with GN/BTP deferred to the adapter (this revision, per §3)**. R1's pydantic/JSON-Schema enforcement mechanism is replaced by the ASN.1 modules + round-trip test as the conformance check. Not silently absorbed — flagged.

### Control plane: externalized config + systemd lifecycle + startup log (cloud); QMI/AT + telux (hardware)

**In the Phase 1 cloud environment there is no V2X-standard control plane to implement, and none should be invented.** There is no modem to configure; QMI/AT and PC5 flow setup have no counterpart. The control plane degenerates to three host-local mechanisms:

1. **Externalized configuration file(s)** — the "message format" of the cloud control plane. One config surface (INI or YAML) carrying: ego StationID, transport endpoint (group/port), GNSS provider selection (`sim`), broadcast cadence (10 Hz), gate constants, impairment profile + scenario script (bench side). Constitution rule 5 (no hardcoded tunables) makes this mandatory anyway.
2. **Process lifecycle via systemd** — the R3 auto-start-on-boot criterion; already the committed pick (report §3(g)).
3. **Startup log** — the plan §3 re-scoped substitute for "modem identified, V2X mode confirmed, GNSS fix acquired": the log names the active transport and GNSS provider and reports a fix.

No local control API, RPC surface, or bench↔ego control channel is needed for Phase 1 — scenario start/stop is bench-side process control. Criteria: C1/C2 (config + systemd cannot fail and cost hours); C4 (zero new dependencies). **One forward-compat rule (from F-HW2):** the transport adapter's config schema must already reserve the PC5 control-plane keys — service ID, traffic priority, periodicity — as placeholders, so the hardware swap changes values and adapter code, not config shape.

**What changes on hardware:** the control plane becomes real code inside the adapter's init: QMI/AT identity + firmware + V2X-mode query (open-source `libqmi`/`qmicli` covers generic QMI services; the cv2x-specific path is telux — the user-authorized vendor-SDK exception F-HW1), then telux `startCv2x` / `createRxSubscription` / `createTxFlow` with the service IDs from config. Data plane is untouched above the link layer — same facilities bytes, then on the cv2x non-IP socket.

## 3. Feasibility — cloud control-plane/stack bring-up, and the data-plane-only fallback

The previous revision's data-plane pick ("ego decodes with Vanetza") quietly assumed a piece of work whose feasibility was never assessed: **bringing up the Vanetza GN/BTP stack on the ego**. That bring-up *is* control-plane-shaped work (router instantiation, position-provider wiring, GN beaconing, DCC gatekeeper, security-disabled profile — configured even when "just decoding"), and it is not free:

- **Vanetza is a C++14 library** built with CMake ≥ 3.12 and depending on Boost ≥ 1.58, GeographicLib and Crypto++. `socktap` is a **demo binary**, not a reusable service — consuming Vanetza in our own ego process means C++ integration against `libvanetza` (`find_package(Vanetza)`, router/position/DCC objects), and there are **no official Python bindings**, so a bridge to the Python-side `ADA` (IPC or bindings) must also be written.
- **Estimated effort:** build + library integration + Python bridge ≈ 1–2 weeks of C++-capable effort — competing with the whole comms track inside the 3-week Round-2 window (21.07–10.08), on a stack whose CPM shape is the wrong version anyway (F-HW3).

**Feasibility verdicts (per option, Phase 1):**

| Option | Verdict | Reasoning |
|---|---|---|
| Full Vanetza GN/BTP integration on ego | **AT-RISK** | C++ toolchain + three native dependencies; socktap demo-grade; libvanetza API integration + cross-language bridge to ADA; 1–2 weeks specialist effort; CPM version delta on top |
| **Data-plane-only: UPER codec over UDP, asn1tools both sides (optional 4-byte BTP-B envelope)** | **ACHIEVABLE — recommended** | Pure Python end to end, same official ASN.1 modules on bench and ego, no stack bring-up, days of effort |
| JSON mirror (original report §3(b) pick) | ACHIEVABLE | Retained as the documented fallback if ASN.1 module compilation with asn1tools hits a blocker; pydantic conformance mechanism returns |

**The recommended Phase 1 scenario — control plane assumed established, data plane only.** Treat the bench→ego link as an *established* channel (the mentor's model already promises this: bench messages logically arrive in the ego network; BTC confirms the concrete transport after blueprint submission). Then Phase 1 work is exactly: **construct → encode → send → receive → decode → validate** UPER CAM/CPM facilities payloads. What is dropped and why it is safe:

- **GN headers** provide geo-routing/multi-hop forwarding — meaningless for a one-hop bench→ego injection; nothing in the M1 demo routes.
- **DCC** manages radio-channel congestion — there is no radio; the scenario player's config-driven impairment (mentor-recommended option b) already supplies the channel effects the acceptance criteria test against.
- **What is kept is the contract:** R1's frozen schema lives at the *facilities* layer; the CPM/CAM bytes stay fully standard and bit-identical to the hardware case ([m1-product-fit-quectel-modem.md](m1-product-fit-quectel-modem.md) §3's layer table — the facilities row is the invariant). The "swap only the transport adapter" claim survives: adding GN/BTP later (Vanetza socktap UDP mode on cloud, or GN-over-PC5 with a telux backend on hardware) happens *inside* the adapter without touching codec, `ADA`, or `IVI`.

**Goal alignment.** The mission in CLAUDE.md is not ambiguous and matches this scope: *demonstrate cooperative (non-line-of-sight) awareness — B detects the obstructed C, relays it, A receives and displays it* — the deliverable is the **cooperative-awareness application, not a full V2X stack implementation**. Selection criteria #1 (likelihood of accomplishing) and #2 (fastest path to the milestone) both pick the codec-only route; implementing a full stack in Phase 1 would be exactly the criterion-#3 over-engineering [solution-selection-criteria.md](../.claude/rules/solution-selection-criteria.md) warns against. Vanetza is therefore **repositioned, not discarded**: it remains the named upgrade path for GN/BTP framing and for hardware interop milestones.

## 4. Q2 — End-to-end call flow and the Phase 1 boundary

Sequence diagram: [m1-phase1-v2x-call-flow-sequence.puml](m1-phase1-v2x-call-flow-sequence.puml) (module names match the existing component/activity diagrams; the `V2X_Comm` deliverable boundary is marked with `group` frames). Under §3's recommendation, the "decode GN + BTP headers" step inside the boundary reduces to "strip envelope (BTP-B header if used)".

1. **Bring-up (control plane).** systemd starts the V2X service on boot (R3) → load externalized config → init `GnssProvider` (sim) and confirm fix → open transport adapter (join/bind the UDP endpoint) → emit startup log (R6 cloud substitute). On hardware this block becomes QMI/AT identify + telux cv2x start + Rx-subscription/Tx-flow setup.
2. **Data-plane RX.** Scenario player ticks trajectories → builds CAM(B) + CPM(B perceives C) → UPER-encodes → applies impairment → sends the UDP datagram. Ego `V2X_Comm`: receive datagram → strip envelope → UPER-decode payload → validate fields/units (fail ⇒ log + discard) → **publish decoded PDU to `ADA`**. `ADA` logs the full message parsed into fields (the Phase 1 acceptance check, R4), emits a *mocked* warning event; `IVI` renders a placeholder.
3. **Data-plane TX.** 10 Hz heartbeat loop (R3): read ego position/time from `GnssProvider` → build the full R1 schema with mock object contents → UPER-encode (+ envelope) → **hand encoded bytes to the transport adapter** → bench logs the received ego message parsed into fields (peer-side R4 mirror).

**Where Phase 1 STARTS:** at service bring-up — systemd start, config load, GnssProvider init, transport open, startup log. **Where Phase 1 TERMINATES:** on RX, at the decoded + validated PDU handed to `ADA` (plus `ADA` logging it parsed into fields); on TX, at the encoded packet handed to the transport channel. Everything deeper — `ADA`'s real environment model, geometry, gating, risk — is Phases 2–4 + 6; the real `IVI` display is Phase 5. In Phase 1 those exist only as a mock consumer/stub, per the mock-then-real principle. The `V2X_Comm` deliverable boundary is narrower still: *raw datagram in / send API called* ⇄ *decoded PDU out / encoded bytes out* — the boxed region in the sequence diagram.

## 5. Q3 — GNSS mocking/deferral and the GnssProvider role

**(a) Extract ego position from received V2X messages? NO.** The reference position in a received CAM/CPM is the **sender's** (peer B's) position — data-plane payload *about the peer*, consumed by `ADA`'s environment model to place B (and, via the CPM perceived-object entry, C). Ego GNSS is the receiver's own position/time source, a control/platform input. Conflating them is a category error with concrete failures: ego "position" would jump to wherever the last peer was; the R5 common-timebase KPI becomes unmeasurable (ego time derived from peer messages is circular); and ego localization would silently die whenever messages stop — exactly when the expiry logic (R12 mirror, R14) needs a trustworthy local clock. Rule: **`GnssProvider` never parses V2X messages; `V2X_Comm` never writes into `GnssProvider`.** The only legitimate meeting point is inside `ADA`, which combines ego position (from `GnssProvider`) with sender reference position (from the PDU) to compute relative geometry.

**(b) Real hardware: YES — `ModemGnssProvider` from the Quectel modem.** Already the recommended resolution in [m1-product-fit-quectel-modem.md](m1-product-fit-quectel-modem.md) §4: the modem integrates a GNSS receiver (required for PC5 timing anyway); the Cortex-A reads position/time via **QMI LOC** (open-source `libqmi`/`qmicli --loc-*` supports start/position-report/follow) or the telux location API (vendor-SDK exception F-HW1). This matches plan §2 ("GNSS comes from the modem's integrated receiver") and plan §3's reserved `ModemGnssProvider`. The dedicated GPS→Cortex-M path stays deferred (plan §6).

**(c) Phase 1 cloud: `SimGnssProvider` fed by the scenario player.** Candidate sources:

| Candidate | Assessment |
|---|---|
| **Direct position feed from the scenario player** (position samples over a local socket, or static position from config) | C1: no external daemon, works on any Linux image; C2: trivial; keeps ego position consistent with scenario ground truth by construction. **Recommended primary.** |
| NMEA replay → `gpsfake`/`gpsd` (pty) | Standard interface (`gpsfake` opens a pty and feeds a gpsd instance from an NMEA log; no root needed); the interface a real GNSS stack exposes. **Kept as the standard-interface variant behind the same interface** — use it if/when a stack's own position plumbing is exercised. |
| AAOS GNSS HAL (virtio/vsock, Cuttlefish/Trout pattern) | **Unconfirmed image support** (working-env finding 5) — do not hard-depend; revisit after CabinSky confirmation. |

Interface shape: `GnssProvider` returns a fix (lat/lon/alt, timestamp, fix status; heading/speed optional) at a configured rate; the active provider is named in the startup log (the re-scoped R6 acceptance). If a future stack layer needs its own position provider (e.g. Vanetza's `PositionProvider` on the upgrade path), bridge our `GnssProvider` into it — one adapter class, keeping a single ego-position source of truth.

**Deferral boundary:** Phase 1 = `SimGnssProvider` + chrony/NTP timebase (R5 ≤ 50 ms bound). Deferred: `ModemGnssProvider` + GNSS-disciplined ≤ 10 ms bound (hardware milestone, R6/F1 gate); AAOS HAL feed (contingent on image confirmation); Cortex-M GPS path (plan §6, unchanged).

## 6. Mapping to existing requirements

| R# | Finding |
|---|---|
| R1 | Data-plane format recommendation = UPER CAM/CPM, codec-only over UDP (supersession chain JSON → Vanetza GN/BTP → codec-only; **user ratifies at R1 freeze**); CPM TR 103 562 vs TS 103 324 delta neutralized for Phase 1 by compiling the official TS 103 324 modules directly with asn1tools (F-HW3 then applies only to the Vanetza upgrade path). |
| R3 | Control plane in cloud = config file + systemd + startup log; TX heartbeat = 10 Hz full-schema CAM with mock contents; adapter config reserves PC5 keys (service ID, priority). |
| R4 | RX terminates Phase 1 at decoded-PDU-to-ADA + full-field parse log; decode-failure path logs + discards so the ≥ 99% delivery KPI counts correctly. |
| R5 | GnssProvider is the ego time/position source — never derived from received messages (Q3a); chrony/NTP in Phase 1, GNSS-disciplined on hardware. |
| R6 | Cloud substitute = startup log naming provider + fix (per plan §3 table); hardware = QMI/AT + telux control plane + `ModemGnssProvider` via QMI LOC. RISK verdict and F1 gate unchanged. |
| R14 | Unaffected — both planes' Phase 1 choices keep the latency budget (UPER codec cost is single-digit ms). |
| R16 | IVI sits beyond the Phase 1 boundary — placeholder render only; real display is Phase 5. |

## 7. Open items

- User ratification of the data-plane format at the R1 freeze: UPER codec-only over UDP (GN/BTP deferred to the adapter upgrade path); R1's enforcement mechanism changes from pydantic/JSON-Schema to ASN.1 modules + round-trip CI.
- Verify early (a day-1 spike) that the official TS 103 324 CPM + TS 103 900 CAM ASN.1 modules compile and round-trip with asn1tools — this is the codec-only route's single technical risk; JSON mirror is the documented fallback if it blocks.
- Confirm AAOS GNSS HAL availability with the CabinSky team before any provider depends on it.
- F-HW1 (vendor-SDK exception) user decision before hardware control-plane work (unchanged).
- Confirm bench→ego transport with BTC after blueprint submission (adapter swap only; unchanged).

## 8. Sources

- [ETSI TS 103 248 V2.1.1 — BTP port numbers (CAM = 2001)](https://www.etsi.org/deliver/etsi_ts/103200_103299/103248/02.01.01_60/ts_103248v020101p.pdf)
- [ETSI TS 103 324 V2.1.1 — Collective Perception Service (CPM; BTP port 2009)](https://www.etsi.org/deliver/etsi_ts/103300_103399/103324/02.01.01_60/ts_103324v020101p.pdf)
- [ETSI TS 103 900 V2.1.1 — Cooperative Awareness (CAM, release 2)](https://www.etsi.org/deliver/etsi_ts/103900_103999/103900/02.01.01_60/ts_103900v020101p.pdf)
- [Vanetza — how to build (CMake ≥ 3.12, Boost ≥ 1.58, GeographicLib, Crypto++; find_package integration)](https://www.vanetza.org/how-to-build/)
- [Vanetza socktap — demo application, link layers, INI config, positioning](https://www.vanetza.org/tools/socktap/)
- [vanetza socktap gps_position_provider.cpp — gpsd-backed position provider](https://github.com/riebl/vanetza/blob/master/tools/socktap/gps_position_provider.cpp)
- [libqmi qmicli — LOC service commands (--loc-start, --loc-get-position-report, --loc-follow-position-report)](https://www.freedesktop.org/software/libqmi/man/1.28.0/qmicli.1.html)
- [libqmi-glib — Location Service (LOC) reference](https://www.freedesktop.org/software/libqmi/libqmi-glib/1.34.0/ch13.html)
- [gpsfake(1) — gpsd test harness: pty + NMEA log replay, no root required](https://gpsd.gitlab.io/gpsd/gpsfake.html)
- [Qualcomm Telematics SDK — telux::cv2x (Rx subscription socket, Tx flows)](https://developer.qualcomm.com/sites/default/files/docs/telematics/api/v1.46.28/a00068.html)
- [Wireshark ITS dissector — BTP port ↔ message-type mapping in practice](https://github.com/wireshark/wireshark/blob/master/epan/dissectors/asn1/its/packet-its-template.c)
