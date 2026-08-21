# V2X Protocol Foundations: ITS-G5 and the ETSI CPM

**Scope:** general V2X protocol and RF-engineering knowledge behind the V2X ECU node — how a station joins the radio network, how CPM (TS 103 324) is disseminated, what it carries, what sender identity actually means, and what propagation effects a real deployment must budget for. This is subject knowledge, not a design decision: node components, paths, and configuration for the V2X ECU stay in [documents/Design/MODULE-DESIGN/V2X-ECU/v2x-ecu-hld.md](../Design/MODULE-DESIGN/V2X-ECU/v2x-ecu-hld.md); the frozen contract stays in [documents/Requirements/m1-cooperative-awareness.md §2 R1](../Requirements/m1-cooperative-awareness.md) (R1/R9).

- CarSky's M1 network is a simulated/wired Ethernet bridge, one L2 segment behind an Ethernet Bridge node — not real 5.9 GHz RF (CLAUDE.md § Mission). Sections 1 and 5 describe the real ITS-G5 radio layer that the CPM contract (R1) is modelled on; they are foundational knowledge for porting the R7 radio adapter to a real modem in a later milestone, not something the M1 bench exercises.
- Sections 2–4 describe how CPM is generated, what it carries, and what "sender identity" means — directly grounding R1 (the CPM contract) and R9 (the Rx pipeline's dedupe key).

## Introduction

- The V2X ECU (R7–R9) decodes CPM (Collective Perception Message, ETSI TS 103 324) received from the bench (R11, simulating vehicle B) and forwards perceived-object data to the ADA ECU (R2). This note is the protocol-level background the R1 profile and R9 pipeline are built on.
- Five questions, one section each: how a station joins the V2X network, how it sends CPM, what CPM contains, what sender identity means and whether M1 needs it, and what propagation effects a real RF deployment must handle.
- Two worked calculations close the propagation section: a Doppler-shift estimate and a free-space link budget, both built from the project's own vehicle-speed figure (R11: B up to 120 km/h / 33.3 m/s).

## 1. How a vehicle joins a V2X network

- **ITS-G5** (the European V2X profile) runs **IEEE 802.11p** in **OCB mode — Outside the Context of a BSS** — at 5.875–5.905 GHz [EN 302 663]. "Joining" in the Wi-Fi sense does not apply.
- OCB mode has no access point, no beacon, no scan/authenticate/associate state machine, and no 4-way handshake. The BSSID field is fixed to the broadcast wildcard (all-F). A station starts transmitting and receiving the moment it tunes to the channel [markmail.org 802.11p RFC discussion; IETF RFC 8691].
- Every station on the channel receives every frame from every station in range and can transmit to all of them — a shared broadcast medium, not a point-to-point session.
- **Contrast with C-V2X PC5 sidelink** (3GPP Release 14+):
  - **Mode 4** — stations autonomously select radio sub-channels via a distributed, sensing-based semi-persistent scheduling (SPS) scheme; no cellular network attachment needed. This is PC5's functional equivalent of ITS-G5's OCB mode — no infrastructure join.
  - **Mode 3** — the cellular network (eNB/gNB) assigns sub-channels; this mode *does* require network attachment and is only available under cellular coverage.
- **Security/pseudonym provisioning is a separate layer from joining the radio channel.** ETSI TS 102 941 defines the enrolment/authorization workflow: an Enrolment Authority issues a long-lived enrolment credential once, an Authorization Authority then issues short-lived **pseudonym certificates** (Authorization Tickets) the station signs its messages under; the certificate format is ETSI TS 103 097, structurally aligned with the IEEE 1609.2 certificate profile used in the North American SCMS. This provisioning happens out-of-band, before road use — a station can key up and transmit/receive at the OCB/PC5 radio layer with zero certificates present; a compliant receiver's *security* layer is what is expected to discard unsigned or invalid traffic, not the radio join procedure.
- M1's V2X ECU implements neither TS 102 941 nor IEEE 1609.2 — consistent with R9's Rx pipeline (decode → validate → dedupe → forward), which has no signature-verification step.

## 2. How a vehicle sends CPM

- CPM dissemination rules live in ETSI TS 103 324 — the same profile R1 adopts.
- **Generation interval** `T_GenCpm` is bounded: `T_GenCpmMin` = 100 ms, `T_GenCpmMax` = 1000 ms [ETSI TS 103 324 V2.1.1] — i.e. a CPM is generated at least once per second and at most 10 times per second.
- **Triggering is change-based, not fixed-rate**: a station re-triggers CPM generation early — before `T_GenCpm` elapses — when a perceived object's reported position, speed, or heading changes beyond a threshold since its last inclusion (the same redundancy-mitigation principle CAM uses). Absent a triggering change, CPM still repeats at the `T_GenCpm` cadence as a heartbeat, capped at `T_GenCpmMax`.
- **Transport is broadcast, single-hop, no ACK**: one ETSI GeoNetworking + BTP-B frame per CPM, sent as a single-hop broadcast (SHB) or geobroadcast for a wider footprint. There is no unicast addressing to a specific receiving vehicle, no acknowledgement, and no retransmission at this layer — every station on-channel either receives the frame or does not, and the sender never knows which.
- This connectionless, fire-and-forget delivery model is why R9's Rx pipeline owns duplicate/missing-message handling at the ADA-facing boundary — dedupe keyed on `(stationId, objectId, measurement time)` — rather than assuming transport-layer reliability the wire format never provides.
- The bench (R11) reproduces this generation-rate/triggering behaviour over UDP rather than real GeoNetworking/BTP framing; R11's tech stack (the shared R1 Vanetza-based encoder) still emits the ASN.1 UPER CPM payload itself unchanged.

## 3. What a CPM contains

- A CPM PDU is: an ITS PDU header (protocol version, message ID, **Station ID**) + a Management Container (station type, reference position/time) + an Originating Vehicle Container (B's own heading, speed, dimensions) + one or more Perceived Object Containers, one per detected object (C, in M1's case).
- Per perceived object: object ID, time-of-measurement offset from the CPM's reference time, relative position (longitudinal/lateral distance with confidence), relative velocity, and object classification with confidence.
- The project's R1 profile already selects and names the exact field set M1 carries — see [m1-cooperative-awareness.md §2 R1](../Requirements/m1-cooperative-awareness.md#contracts) for the authoritative field table (Station ID, reference time, sender position/heading/speed, perceived object ID/time-of-measurement/relative position/velocity/classification). This section does not restate it.
- **Encoding**: ASN.1 UPER (Unaligned Packed Encoding Rules) — a compact, bit-packed wire format, not a verbose tagged format like XML or JSON. This is why R1's tech stack is the Vanetza ITS2 codec (dedicated CPM ASN.1 targets) rather than a hand-rolled decoder: UPER's bit-level packing is impractical to hand-decode reliably.
- R2 (V2X ECU → ADA ECU) re-encodes the decoded fields as JSON. The UPER-on-the-wire / JSON-across-the-intra-ego-boundary split is deliberate: UPER is the interoperable V2X wire format; JSON is the project's own internal contract once the CPM has already been decoded and validated.

## 4. Sender identity: Station ID, and whether M1 needs it

- **In principle**, the CPM's Station ID (ITS PDU header) is the field that answers "which vehicle sent this."
- **In a real deployment, Station ID is not persistent identity.** ETSI ITS privacy design deliberately rotates a station's Station ID together with its pseudonym certificate at intervals (pseudonym change), specifically so a passive observer cannot track one vehicle over time by watching Station ID alone.
- **Real sender trust comes from the certificate chain, not the Station ID value.** A CPM is signed under the sender's current Authorization Ticket (the short-lived pseudonym certificate issued per ETSI TS 102 941 / IEEE 1609.2 §4); a receiver validates the signature and the certificate's chain to a trusted Authorization Authority before trusting the message content — Station ID by itself proves nothing cryptographically.
- **For M1 specifically: this is not needed, and the project does not build it.** The V2X ECU is receive-only (R9); the demo topology has exactly one CPM source, the bench Scenario Player simulating vehicle B (R11) — there is no multi-sender or adversarial scenario for M1 to defend against. M1 implements no TS 102 941 / IEEE 1609.2 signature verification. Station ID is used **only** as one field of R9's dedupe key (`stationId + objectId + measurement time`) — a grouping/dedup key, not a trust mechanism. This is the project's actual scoping (§2 R9, §4 decision record), not a simplification assumed for this note.
- A future milestone with more than one real sender, or an adversarial setting, would need the certificate-chain trust layer above — out of scope for M1 in its entirety, alongside R10 (ego Tx, deferred), since ego does not yet sign anything it sends either.

## 5. Propagation effects on V2X transmission

- **Multipath fading.** Reflections off other vehicles and roadside structures cause constructive/destructive interference at the receiver. Vehicular V2X channels are frequency-selective and time-varying (WSSUS-class channel models) because both the transmitter and the scatterers around it are moving.
- **Doppler shift.** Relative motion between sender and receiver shifts the received carrier frequency, stressing the OFDM receiver's carrier-frequency-offset tracking. Worked calculation: § 5.1.
- **Shadowing / NLOS attenuation.** A large vehicle between transmitter and receiver attenuates the signal well beyond free-space loss. This is the RF analogue of the project's own visual-occlusion scenario — B blocking A's camera view of C (§1 of the requirements report) — but M1's bench-to-V2X-ECU hop is a wired Ethernet link and does not model this attenuation; it is foundational knowledge for a real ITS-G5 deployment, not something M1 exercises.
- **Channel congestion.** The 5.9 GHz ITS-G5 control channel is a shared broadcast medium — every station's CAM/CPM traffic competes for airtime. Unmanaged, rising vehicle density and message rate raise channel load, increasing collisions and packet error rate. ETSI TS 102 687 defines **Decentralized Congestion Control (DCC)**: a station measures channel busy ratio and moves through Relaxed / Active / Restrictive states, throttling generation rate, transmit power, and/or data rate to hold channel load under target thresholds and to reserve capacity for event-triggered high-priority messages. C-V2X Mode 4's sensing-based SPS scheduler applies the analogous discipline (CBR-driven power/MCS/retransmission control) without a DCC state machine by name.
- **Hidden-node problem.** Two stations that cannot hear each other but are both within range of a common receiver can collide at that receiver without either sender detecting it — carrier sense does not see a transmission it cannot hear. Relevant to CPM reception wherever enough senders share one channel that not all of them are mutually visible.
- None of the above affects M1's bench-to-V2X-ECU hop — that hop is a simulated/wired Ethernet segment (CLAUDE.md § Mission) — but they are what the R7 radio adapter seam's real-hardware port plan must account for once the adapter sits behind a real ITS-G5 modem.

### 5.1 Doppler shift calculation

- Formula: `f_d = (v_relative / c) × f_0`, one-way Doppler shift for closing/receding radial velocity.
- Assumptions: `f_0` = 5.9 GHz (ITS-G5 band centre, 5.875–5.905 GHz [EN 302 663]); `c` = 3×10⁸ m/s; `v_relative` built from the project's own top speed, R11's 120 km/h = 33.3 m/s for vehicle B.
- **Case A — in-convoy relative speed.** A, B, C travel the same direction at similar speed in the demo scenario; the CPM link's Doppler is bounded by the speed differential between convoy vehicles, well under 33.3 m/s in practice.
- **Case B — worst-case head-on closing speed** (the standard V2X worst case: two stations each at the scenario's top speed, approaching from opposite directions): `v_relative = 2 × 33.3 = 66.6 m/s`.
- Results:
  - Single-vehicle radial speed, 33.3 m/s: `f_d = (33.3 / 3×10⁸) × 5.9×10⁹ ≈ 655 Hz`.
  - Worst-case closing speed, 66.6 m/s: `f_d = (66.6 / 3×10⁸) × 5.9×10⁹ ≈ 1310 Hz ≈ 1.31 kHz`.
- Context: an ITS-G5 10 MHz channel's OFDM subcarrier spacing is on the order of tens of kHz (802.11a-derived numerology, half-clocked). A ~1.3 kHz worst-case shift is a small fraction of one subcarrier's width — a bounded, correctable impairment via pilot-based carrier-frequency-offset tracking, not typically a hard failure mode at these speeds, but it erodes SNR margin and belongs in a high-mobility link budget.

<svg viewBox="0 0 640 260" xmlns="http://www.w3.org/2000/svg" font-family="sans-serif" width="100%">
  <rect x="0" y="0" width="640" height="260" fill="#ffffff" stroke="#cccccc"/>
  <text x="320" y="24" text-anchor="middle" font-size="15" font-weight="bold">Doppler shift — worst-case head-on closing (Case B)</text>

  <!-- vehicle B, moving right -->
  <rect x="40" y="110" width="70" height="34" rx="4" fill="#2c6fbb"/>
  <text x="75" y="132" text-anchor="middle" font-size="12" fill="#ffffff">B</text>
  <line x1="115" y1="127" x2="200" y2="127" stroke="#2c6fbb" stroke-width="3" marker-end="url(#arrowB)"/>
  <text x="155" y="115" text-anchor="middle" font-size="12" fill="#2c6fbb">v = 33.3 m/s</text>

  <!-- oncoming station, moving left -->
  <rect x="530" y="110" width="70" height="34" rx="4" fill="#bb2c2c"/>
  <text x="565" y="132" text-anchor="middle" font-size="12" fill="#ffffff">oncoming</text>
  <line x1="525" y1="127" x2="440" y2="127" stroke="#bb2c2c" stroke-width="3" marker-end="url(#arrowR)"/>
  <text x="480" y="115" text-anchor="middle" font-size="12" fill="#bb2c2c">v = 33.3 m/s</text>

  <!-- wavefront squiggle between them -->
  <path d="M 210 180 Q 230 165 250 180 T 290 180 T 330 180 T 370 180 T 410 180 T 430 180"
        fill="none" stroke="#555555" stroke-width="2"/>
  <text x="320" y="205" text-anchor="middle" font-size="12" fill="#555555">5.9 GHz carrier, CPM broadcast</text>

  <defs>
    <marker id="arrowB" markerWidth="8" markerHeight="8" refX="6" refY="4" orient="auto">
      <path d="M0,0 L8,4 L0,8 Z" fill="#2c6fbb"/>
    </marker>
    <marker id="arrowR" markerWidth="8" markerHeight="8" refX="6" refY="4" orient="auto">
      <path d="M0,0 L8,4 L0,8 Z" fill="#bb2c2c"/>
    </marker>
  </defs>

  <text x="320" y="235" text-anchor="middle" font-size="13" font-weight="bold">f_d = (v_relative / c) × f_0 = (66.6 / 3×10⁸) × 5.9×10⁹ ≈ 1.31 kHz</text>
</svg>

### 5.2 Free-space path loss and link budget

- Formula (Friis, distance in km, frequency in MHz): `FSPL(dB) = 20·log10(d_km) + 20·log10(f_MHz) + 32.44`.
- Assumptions, sourced not invented:
  - `f` = 5900 MHz (ITS-G5 band centre).
  - `Pt` = 21 dBm conducted transmit power — a representative ITS-G5 OBU figure; EU regulation (EN 302 571) permits higher EIRP, 21 dBm conducted is the OBU-side figure used here.
  - `Gt = Gr` = 3 dBi — typical low-profile ITS-G5 OBU antenna gain, applied at both ends.
  - Receiver sensitivity ≈ −85 dBm — the most robust IEEE 802.11p MCS (BPSK, rate 1/2, 10 MHz channel) [Rohde & Schwarz 1MA152 application note], i.e. the floor for maximum-range reception.
- Link budget: `Pr(d) = Pt + Gt + Gr − FSPL(d) = 27 dBm − FSPL(d)`.
- Worked point, `d` = 300 m (a representative relay range for a following-vehicle CPM link):
  - `FSPL(0.3 km) = 20·log10(0.3) + 20·log10(5900) + 32.44 ≈ 97.4 dB`.
  - `Pr(300 m) = 27 − 97.4 ≈ −70.4 dBm` — a 14.6 dB margin over the −85 dBm sensitivity floor in free space.
- Free-space maximum range (`Pr` = sensitivity floor, −85 dBm): `FSPL = 112 dB` → solving for `d` gives **≈ 1.6 km**. This is an idealized line-of-sight ceiling with no fade margin. Commonly quoted practical ITS-G5 ranges are 50–1000 m under real conditions (multipath, NLOS shadowing, channel congestion, per § 5) — the gap between the ~1.6 km free-space ceiling and the practical figure is exactly the fade margin a real deployment must budget, on top of the DCC congestion control described above.

<svg viewBox="0 0 620 340" xmlns="http://www.w3.org/2000/svg" font-family="sans-serif" width="100%">
  <rect x="0" y="0" width="620" height="340" fill="#ffffff" stroke="#cccccc"/>
  <text x="310" y="24" text-anchor="middle" font-size="15" font-weight="bold">Received power vs. range — Friis link budget (Pt=21 dBm, Gt=Gr=3 dBi)</text>

  <!-- axes -->
  <line x1="60" y1="40" x2="60" y2="300" stroke="#000000"/>
  <line x1="60" y1="300" x2="580" y2="300" stroke="#000000"/>
  <text x="20" y="45" font-size="11">-50 dBm</text>
  <text x="10" y="303" font-size="11">-95 dBm</text>
  <text x="55" y="318" font-size="11">0 m</text>
  <text x="545" y="318" font-size="11">2000 m</text>
  <text x="310" y="332" text-anchor="middle" font-size="12">Range (m)</text>
  <text x="-160" y="16" transform="rotate(-90)" font-size="12">Received power (dBm)</text>

  <!-- sensitivity threshold line at -85 dBm -> y=273.3 -->
  <line x1="60" y1="273" x2="580" y2="273" stroke="#bb2c2c" stroke-width="1.5" stroke-dasharray="6,4"/>
  <text x="470" y="268" font-size="11" fill="#bb2c2c">receiver sensitivity ≈ -85 dBm</text>

  <!-- Pr(d) curve, points computed from FSPL formula -->
  <polyline fill="none" stroke="#2c6fbb" stroke-width="2.5"
    points="72,72 85,112 110,153 135,176 185,206 260,233 310,246 385,261 462,273 560,286"/>

  <!-- operating point at 300 m -->
  <circle cx="135" cy="176" r="4.5" fill="#1a7a1a"/>
  <text x="145" y="172" font-size="11" fill="#1a7a1a">300 m: -70.4 dBm (14.6 dB margin)</text>

  <!-- free-space ceiling at 1610 m -->
  <circle cx="462" cy="273" r="4.5" fill="#bb2c2c"/>
  <text x="330" y="256" font-size="11" fill="#bb2c2c">~1.6 km: free-space ceiling</text>
</svg>

## References

- ETSI TS 103 324 V2.1.1 (2023-06), Collective Perception Service — [etsi.org deliverable](https://www.etsi.org/deliver/etsi_ts/103300_103399/103324/02.01.01_60/ts_103324v020101p.pdf).
- ETSI EN 302 663, ITS-G5 access layer specification — [etsi.org deliverable](https://www.etsi.org/deliver/etsi_en/302600_302699/302663/01.02.00_20/en_302663v010200a.pdf).
- ETSI TS 102 687, Decentralized Congestion Control mechanisms — [etsi.org deliverable](https://www.etsi.org/deliver/etsi_ts/102600_102699/102687/01.02.01_60/ts_102687v010201p.pdf).
- ETSI TS 102 941 V2.2.1, Trust and privacy management — [etsi.org deliverable](https://www.etsi.org/deliver/etsi_ts/102900_102999/102941/02.02.01_60/ts_102941v020201p.pdf).
- ETSI TS 103 097, security header and certificate format aligned with IEEE 1609.2 — [etsi.org deliverable](https://www.etsi.org/deliver/etsi_ts/103000_103099/103097/02.01.01_60/ts_103097v020101p.pdf).
- IETF RFC 8691, IPv6 over 802.11-OCB — background on OCB mode's no-association behaviour — [datatracker.ietf.org](https://datatracker.ietf.org/doc/html/rfc8691).
- Rohde & Schwarz Application Note 1MA152, "Intelligent Transportation Systems Using IEEE 802.11p" — 802.11p sensitivity figures by MCS — [scdn.rohde-schwarz.com](https://scdn.rohde-schwarz.com/ur/pws/dl_downloads/dl_application/application_notes/1ma152/1MA152_5e_ITS_using_802_11p.pdf).
- Molina-Masegosa & Gozalvez, "Configuration of the C-V2X Mode 4 Sidelink PC5 Interface for Vehicular Communication" (2018) — Mode 3/Mode 4 contrast — [uwicore.umh.es](https://uwicore.umh.es/files/paper/2018_international/MolinaGozalvezSepulcre_ConfigurationCV2X_MSN2018.pdf).
- [m1-cooperative-awareness.md §2 R1, R9](../Requirements/m1-cooperative-awareness.md) — the project's frozen CPM profile and Rx pipeline this note grounds.
