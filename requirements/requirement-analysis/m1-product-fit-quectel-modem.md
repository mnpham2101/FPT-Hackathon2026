# Research Note — Product-Design Fit: Quectel C-V2X Modem on the Cortex-A Host (M1)

> Researcher artifact, **not** an HLD. Analyses whether the Phase 1 working environment
> ([m1-phase1-working-environment.md](m1-phase1-working-environment.md)) fits the actual hardware
> product design in `tmp/v2x_system_with_dedicated_gps_to_mcore.png`. Requirement numbers refer to
> [m1-cooperative-awareness.md](m1-cooperative-awareness.md) (R1–R18); **no new numbers are
> defined here.** Companion diagram:
> [m1-product-fit-quectel-modem.puml](m1-product-fit-quectel-modem.puml).

## 1. The product design (as drawn)

- **Quectel modem** (5G NR Uu + C-V2X PC5, 5.9 GHz) ↔ **Cortex-A MPU** (Adaptive + GUI) over
  **USB3/PCIe with QMI control** — note on diagram: "Cortex-A configures the modem via QMI
  (control path)".
- **Cortex-A ↔ Cortex-M MCU** (Classic AUTOSAR) over automotive Ethernet / SOME-IP.
- **Sensors** (radar/lidar) on the Cortex-M via CAN-FD/SPI.
- **Dedicated GPS receiver → Cortex-M only** (GNSS + 1PPS, one-way). No GNSS feed drawn to the
  Cortex-A or to the modem.

## 2. Question 1 — Can `V2X_Comm` on the Cortex-A process the modem's V2X data?

**Verdict: YES — the researched `V2X_Comm` shape fits this product design, with three
qualifications (below).**

How Qualcomm-based Quectel C-V2X modules actually expose PC5 payloads:

1. **The modem terminates PC5 at the access layer only (PHY/MAC).** Quectel AG15 is built on the
   Qualcomm 9150 C-V2X chipset; the AG55xQ family (AG550Q/AG553Q) adds 5G NR + C-V2X PC5 on the
   same architecture. Everything **above** the access layer — GeoNetworking/BTP (or IEEE 1609.3),
   facilities (CAM/CPM) — runs on a processor outside the modem baseband: either the module's
   small integrated A7 (an option Quectel markets for stack vendors) or, as in this product
   design, the **external Cortex-A host**. The ETSI ITS Release 1 upper-layer standards are
   defined access-layer-independently precisely so the same GN/BTP/facilities stack runs over
   LTE-V2X PC5.
2. **Data path to the host = socket-based, over USB3/PCIe.** The Qualcomm Telematics SDK
   (`telux::cv2x` radio API) exposes the C-V2X radio as **IP and non-IP network interfaces
   (rmnet)**; the app calls `createRxSubscription` / `createTxFlow` and gets back a **socket fd**
   from which it reads received PC5 payloads as raw datagrams (and writes for TX). Control
   (radio state, service IDs, config) is QMI-based, matching the diagram's "QMI ctrl" label.
   So what arrives at `V2X_Comm` on the hardware is exactly the **above-MAC PDU: GeoNetworking
   header + BTP + UPER facilities payload** — the modem decodes none of it.
3. **Vanetza fits on top of that path.** Vanetza's socktap already demonstrates pluggable link
   layers (raw Ethernet, GN-over-UDP-multicast, Cohda LLC, cube:evk RPC/protobuf). A
   Qualcomm/telux backend is **not upstream**, but it is the same pattern: a thin link-layer
   adapter that moves GN packets through the cv2x non-IP socket instead of a UDP socket.
   GeoNetworking over LTE-V2X PC5 is itself standardized (ETSI EN 303 613 access layer;
   TS 103 836-4-3 media-dependent GN over LTE-/NR-V2X), so the bytes Vanetza produces/consumes
   above the access layer are the right protocol family.

**Qualifications (flags):**

- **F-HW1 — Vendor SDK is proprietary.** The Telematics SDK / Quectel AG5xx SDK is
  vendor-licensed (obtained via Quectel FAE), not open-source. This was already ruled in
  [m1-cooperative-awareness.md](m1-cooperative-awareness.md) §3(a): vendor SDK use on real
  hardware is a **user-authorized hardware exception, never a shortlisted solution**. Decision
  owner: user. All open-source code (Vanetza, codecs, our modules) stays above the adapter.
- **F-HW2 — A telux link-layer adapter must be written** (~the size of socktap's existing
  backends). This is exactly the swappable transport adapter already committed for R3/R4 — no
  environment-model change needed, but the adapter's config surface should anticipate PC5 flow
  setup (service ID, priority) so the swap stays adapter-only.
- **F-HW3 — CPM version caveat unchanged.** Vanetza's CPM is TR 103 562-shaped, not TS 103 324
  (R1's normative target) — the delta applies identically on hardware and bench; resolve it once
  at the R1 freeze (open item in
  [m1-phase1-working-environment.md](m1-phase1-working-environment.md), finding 2).

## 3. Question 2 — Does the bench environment simulate the PDU the modem would deliver?

**Verdict: YES at every layer the ego software touches; the differences sit entirely below the
transport adapter. The "swap only the transport adapter" claim holds, precisely at the
GN-packet byte boundary.**

| Layer | Bench (Phase 1 environment) | Hardware (product design) | Identical? |
|---|---|---|---|
| Facilities payload (CAM/CPM) | UPER bytes built by scenario player (asn1tools) | UPER bytes built by peer's ITS stack | **Yes — bit-identical by the ASN.1 standard** |
| GN + BTP framing | Vanetza GN/BTP, packets wrapped in UDP multicast (socktap UDP mode, `239.118.122.97:8947`) | Vanetza GN/BTP, packets on the cv2x non-IP socket (GN over PC5 per TS 103 836-4-3) | **Yes above the link layer** — same headers, same bytes handed to/from the decoder |
| Link/host interface | UDP datagram on a multicast socket | telux Rx-subscription/Tx-flow socket fd over USB3/PCIe (rmnet), QMI control | **No — this is the adapter swap** |
| Radio | none (sanctioned out of scope) | PC5 sidelink PHY/MAC, GNSS-timed | **No — never simulated** |

What the bench deliberately does **not** simulate (and why that's acceptable): PC5 flow/service-ID
configuration and L2 addressing, DCC/congestion control, per-packet radio metadata (RSSI/CBR),
the true PC5 loss/latency profile (the impairment engine is a config-driven approximation), and
modem GNSS timing. None of these leak above the transport adapter under the current
`recv() → GN-packet bytes` contract. Consequence for the swap: replace "read UDP datagram, strip
encapsulation" with "read cv2x socket" plus QMI/telux radio bring-up (R6 work) — decoder, ADA,
IVI untouched.

## 4. Product-design / environment-model modifications recommended

1. **GNSS gap at the modem (product design).** PC5 sidelink requires GNSS time synchronization at
   the modem; Quectel C-V2X modules integrate a multi-constellation GNSS receiver partly for
   this. The diagram shows **no GNSS at the modem** — add the modem's GNSS antenna feed to the
   product design. This is a drawing/design gap, not an environment-model change.
2. **GNSS source conflict for the Cortex-A (product design vs plan §2).** The diagram's only GNSS
   path is dedicated GPS → Cortex-M — which is exactly the **deferred-scope item** (plan §6:
   "Cortex-M GNSS + telemetry path"). Plan §2 says M1 GNSS comes from the **modem's integrated
   receiver**. Recommended resolution: since fix 1 gives the modem GNSS anyway, the Cortex-A
   reads position/time from the modem (QMI LOC / telux location API) via a `ModemGnssProvider` —
   matching plan §3's reserved provider — and the GPS→M-core path stays deferred (a redundancy /
   Classic-AUTOSAR path for a later milestone). Alternative (if the product must be
   dedicated-GPS-only): a `SomeipGnssProvider` pulling position from the Cortex-M over SOME/IP —
   this pulls deferred scope into M1's interface surface and is **not** recommended. Decision
   owner: user (flagged, not absorbed).
3. **Environment model: no change required.** The bench-node model, `V2X_Comm` decomposition
   (transport adapter + decoder), and `GnssProvider` abstraction all survive contact with the
   product design unchanged; the hardware variant only adds concrete implementations behind
   existing seams.

## 5. Mapping to existing requirements

| R# | Finding |
|---|---|
| R1 | Unchanged; CPM TR 103 562 vs TS 103 324 delta applies equally on hardware (F-HW3). |
| R3, R4 | Committed swappable transport interface is validated: hardware swap = UDP adapter → telux cv2x-socket adapter (F-HW2). Broadcast-loop + parse KPIs unchanged. |
| R5 | Hardware path makes the GNSS-disciplined KPI (≤ 10 ms) reachable via modem GNSS / 1PPS; bench keeps the ≤ 50 ms NTP bound. |
| R6 | Diagram confirms the QMI/AT control path R6 assumes. Concrete risk sharpened: SDK access is FAE-gated and the telux adapter is new code — the 1–2 week bring-up estimate and the week-2 go/no-go gate (flag F1) stand. RISK verdict unchanged. |
| R14 | Latency budget unchanged; PC5 air segment adds single-digit ms. |
| R15/R16 | Untouched — above the seams. |

## 6. Open items

- User decision on F-HW1 (vendor-SDK hardware exception) before any hardware bring-up work.
- User decision on modification 2 (Cortex-A GNSS source: modem vs SOME/IP-from-M-core).
- Confirm with the hardware owner which Quectel part (AG15 vs AG55xQ variant) and whether the
  Quectel SDK / Telematics SDK license is obtainable in the project window (feeds R6's F1 gate).
- F-HW3 / CPM version delta — resolve at R1 freeze (already tracked).

## 7. Sources

- [Quectel AG15 automotive C-V2X module (product page)](https://www.quectel.com/product/c-v2x-ag15-automotive-module/)
- [Quectel AG15 announcement — Qualcomm 9150 chipset, integrated A7 option for ITS stack](https://www.prnewswire.com/news-releases/quectel-announces-new-c-v2x-module-to-support-autonomous-driving-based-on-qualcomm-9150-c-v2x-chipset-solution-300744858.html)
- [Quectel 5G & C-V2X AG55xQ automotive module series](https://www.quectel.com/product/5g-c-v2x-ag55xq-automotive-module/)
- [Quectel AG55xQ series specification (PDF)](https://quectel.com/content/uploads/2024/03/Quectel_AG55xQ_Series_Automotive_Module_Specification_V1.7-1.pdf)
- [Quectel forum — SDK for AG550Q obtained via sales/FAE](https://forums.quectel.com/t/sdk-for-ag550q-cn/18179)
- [Qualcomm Telematics SDK — telux::cv2x namespace (Rx subscription socket, Tx flows)](https://developer.qualcomm.com/sites/default/files/docs/telematics/api/v1.46.28/a00068.html)
- [Qualcomm Telematics SDK — C-V2X interface spec (IP + non-IP rmnet interfaces)](https://developer.qualcomm.com/sites/default/files/docs/telematics/api/v1.30.3/a00016.html)
- [Qualcomm C-V2X 9150 chipset](https://www.qualcomm.com/automotive/products/qualcomm-c-v2x-9150)
- [Vanetza socktap — link layers incl. GN-over-UDP multicast](https://www.vanetza.org/tools/socktap/)
- [Vanetza on cube:evk — host-side socktap with RPC link layer to the V2X radio (the integration pattern)](https://www.nfiniity.com/blog/How-to-use-Vanetza-on-the-CUBE-EVK.html)
- [Vanetza building for Cohda MK5 — vendor-device link-layer variant](https://www.vanetza.org/recipes/cohda-sdk-build/)
- [ETSI TS 103 836-4-3 V2.1.1 — GeoNetworking media-dependent functionality for LTE-V2X / NR-V2X PC5](https://www.etsi.org/deliver/etsi_ts/103800_103899/1038360403/02.01.01_60/ts_1038360403v020101p.pdf)
- [Draft ETSI EN 303 613 — LTE-V2X access layer for ITS at 5.9 GHz](https://www.etsi.org/deliver/etsi_en/303600_303699/303613/01.01.00_20/en_303613v010100a.pdf)
- [5GAA — List of C-V2X devices (host-stack deployment landscape)](https://5gaa.org/content/uploads/2021/11/5GAA_List_of_C_V2X_devices.pdf)
