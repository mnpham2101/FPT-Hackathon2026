# Phase 0 — `trial2_minh` Preflight (read-only, 2026-07-31)

Evidence for smoke-test task group 0.8 ([phase0_tasks.md](../phase0_tasks.md)): REST inspection of the baseline blueprint before the manual M5–M12 steps of [baseline-connectivity-smoke-test.md](research_notes/baseline-connectivity-smoke-test.md). No mutations were made (validate is a non-mutating check).

## Identity

- Blueprint **`trial2_minh`** — id `WX2ungPR-1ZbdJUUWn68u`, created 2026-07-30, not locked.
- A clone **`baseline_m1`** (id `g63_Ay5XWUANvCE39cWxQ`, `parentBlueprintId` = trial2_minh) already exists — usable as the M5 scratch copy; if cloning again, re-verify the clone kept its pins/edges.

## Headline findings

- **Topology is complete — M6 requires nothing.** All 4 `ETHERNET` pins (addresses `10.99.0.10–.13`) and all 4 star edges to the bridge exist (hand-wired in the UI 2026-07-30); the feared import-dropped-pins case did not materialize.
- **Validate passes verbatim:** `{"valid":true,"errors":[]}`.
- **M8 attention:** the IVI skycraft node's REST-visible config has **no AAOS artifact reference** (only `prefix=ivi`, `gpuBackend=virglrenderer`, 1920×1080). Confirm in the UI that the ANDROID IMAGE artifact (image + host_package roles) is bound; without it the Room never reaches all-nodes-ready.
- Node configs already carry the real images/env per the node guides (`m1-scenario-player`/`m1-v2x-ecu`/`m1-ada-ecu` tags, ports 47100/47200/47300, `GATE_ENTER_M=30`/`GATE_EXIT_M=35`).
- No `capabilities` field exists on any node yet (M7 must add `NET_RAW`).
- Image refs use host `registry.carsky.io` — open item O1 (that host answered 502 live; `registry.hackathon-2.carsky.io` answered) applies when M7 swaps in the netcheck tag.
- Observations, not defects: bridge `config: null` (matches every real export; guide §4 step 3 errata already noted in the smoke-test note); all role pins are direction `OUTPUT`, including the receivers.

## Remaining manual work (M5–M8)

| Step | Required by hand |
|---|---|
| M5 | Trivial — open `trial2_minh` (ready-wired) or use the existing `baseline_m1` clone |
| M6 | **Nothing** — pins/edges present, validate green |
| M7 | UI-only, per container node: swap `image` to the netcheck tag (resolve O1 host first), `command: ["./entrypoint.sh"]`, add `capabilities: ["NET_RAW"]`, add env — bench `ROLE=bench` + `NEXT_HOP_HOST/PORT` (copy from its `V2X_ECU_HOST/PORT`); V2X `ROLE=v2x` + `NEXT_HOP_HOST/PORT` (keep `LISTEN_PORT=47100`); ADA `ROLE=ada`, **add** `LISTEN_PORT=47200` beside `V2X_LISTEN_PORT`, + `NEXT_HOP_HOST/PORT` |
| M8 | Verify/set the IVI AAOS artifact binding (see headline finding) |
