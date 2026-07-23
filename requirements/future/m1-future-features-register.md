# Future-Features Register — Cooperative Vehicle Awareness (not M1 scope)

> Mirror of [m1-cooperative-awareness.md § Future developments](../m1-cooperative-awareness.md#future-developments) — the authoritative future-features list; on any conflict the report wins, and changes land there first. Rx references resolve to the report's §2 (R1–R19). Nothing here is M1 scope; each entry names the M1 seam it extends.

- **Modular warning scenarios.** Future warning scenarios (intersection hazards, curve blind spots) plug in as new realizations of the Collision Risk Assessment abstraction (R14) without reworking existing code.
- **Priority-vehicle preemption warning.** Warn when an emergency/priority vehicle approaches so the driver can yield — a new R14 realization, drawn as a future realization in [ada-ecu.svg](../ada-ecu.svg).
- **Speed-scaled collision risk assessment.** The admission/risk distance criterion (R13) scales with traffic speed instead of M1's set threshold.
- **Camera live feed in the IVI Display area.** The Display area (R16) renders live camera frames; live video also feeds ADA's obstruction algorithm.
- **Live object detection at speed.** B travels up to 120 km/h (33.3 m/s) and must detect obstructions within stopping + broadcast distance. Derived spec:
  - **Detection range, all (A):** detect a car at ≥ 130–150 m dry / ≥ 190 m wet — braking 79 m dry (a = 7 m/s²) / 139 m wet (a = 4 m/s²) + driver reaction 1.0–1.5 s ≈ 33–50 m + ~10 m margin for an (A) ~300 ms end-to-end broadcast budget; an automated-braking variant (no reaction term) relaxes to ≥ 100 m dry / ≥ 160 m wet.
  - **Camera HFOV is an explicit (A) assumption** (60° vs 90° swings pixel subtension ~1.5×) — fixed when the camera spec exists.
  - **The binding constraint is detection range (input resolution), not FPS:** at 640-px inference a 1.8 m car is ~10–12 px at 100 m — below the ~20–30 px nano-detector reliability floor (COCO "small" < 32² px) ⇒ ≥ 1280–1920 px inference at ≥ 10 Hz.
  - **CPU inference fails outright** (≈ 224 ms @ 1280 / ≈ 505 ms @ 1920) ⇒ **GPU-class acceleration required** (reference: T4 TensorRT ≈ 1.5 ms @ 640, scaling to ≈ 6–14 ms). Acceleration-stack openness (open candidates: ROCm, OpenVINO, ONNX Runtime) is the user's decision at that milestone — M1 carries zero GPU dependency (§4).
- **Ego video clip display on the IVI.** The provided ego-POV clip (B occluding the view ahead) plays in the Display area (R16) — §1's demo-table "video feed" method; an added surface, not a rework.
- **2D ⇄ 3D view switching.** A user-facing toggle between the 2D and the optional 3D God view, behind R17's view seam (SceneView/Filament is the optional-3D stack).
- **Multi-process wake-on-warning.** A dedicated sleeping app wakes and draws the scene on an ADA event; M1's optional multi-process path (R16) is its foundation.
- **Custom appearance / themes.** Vehicle avatars could be customized.
- **Fast-UI + C2X benchmark criteria.** Extremely fast UI behavior alongside C2X services — e.g. a foreground media app plus 2D/3D display or notification of vehicle C / V2X message arrival; benchmark thresholds are negotiable against technical feasibility.
- **Multiple hidden obstructions detection.** ADA detects and tracks multiple objects outside line of sight simultaneously (M1 is single-object only).
- **Single-message aggregation.** Multiple detected obstructions collapse into one V2X message — no broadcast storm.
- **User-opt message reduction.** The user can opt for fewer V2X messages.
- **Criticality filtering.** The user can opt to receive only warnings at or above a chosen criticality level; criticality is looked up from the ADA→IVI message's `warningType` field (R4) — see [m1-warning-message-extensibility.md](../requirement-analysis/m1-warning-message-extensibility.md).
- **Other hazard-warning types.** Slippery roads, falling rocks, road holes, road condition, presence of children, police, speed limits, no-horn/other road rules, traffic conditions — carried by DENM (event position + cause code), the named message family for these types (R1 note).
- **Extensible V2X message-type dispatch.** The Rx pipeline (R9) dispatches on message type, so further families enter as a new codec module plus one dispatch entry — M1 decodes CPM only (R1).
- **Commands to other ECUs.** ADA's output stage extends to command/actuation output to further ECUs/hardware; M1 implements only the R10 store snapshot to the V2X ECU.
