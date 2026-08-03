# ADA Phase 3–4 Completion

## Goal
Close the remaining ADA runtime, evidence, CI, and CarSky acceptance gaps without editing Phase 1 or Phase 5 source.

## Tasks
- [x] Integrate a managed detector subprocess with the ADA runtime → Verify: live R3 stdout and UDP R2 are consumed in one run.
- [x] Correct detector R3 lifecycle fields and tracking measurements → Verify: schema-valid IDs, speed, timestamps, and store-owned state tests pass.
- [x] Add CRA registration, assessment records, risk levels, and dwell transitions → Verify: low/medium/high and clear paths pass deterministic C++ tests.
- [x] Ratify additive R4 `trackedObjects` in shared schemas/bindings → Verify: contract sync and round-trip tests pass without touching IVI source.
- [x] Complete EVT reporting and ADA→IVI pcap tooling → Verify: complete-log checker passes, broken chains fail, and capture scripts pass syntax/round-trip checks.
- [x] Add Phase 3/4 CI workflows → Verify: workflow commands pass locally and Docker `linux/arm64` image builds.
- [ ] Prepare and execute the CarSky evidence run with the user → Verify: deployed logs, IVI packet capture, and measured inference rate are recorded.

## Done When
- [ ] Phase 3 and Phase 4 milestone acceptance boxes have executable evidence; only Phase 5 owner integration remains external.

## Notes
The user ratified additive `trackedObjects`. IVI Kotlin source remains owned by Phase 5; this wave produces a concrete integration request instead of editing it.

## Pre-CarSky hardening
- [x] CRA uses composed A→C distance and TTC evidence.
- [x] A periodic fusion tick makes 300 ms dwell effective.
- [x] R4 serialization uses the shared binding; EVT stores full B/C R3 objects.
- [x] CI runs real UDP R2/R4 loopback plus an out-of-range negative control.
- [x] Detector supports real-time looping and own-sensor expiry/re-admission.
- [x] PCAP rotation is exported through View Log with verified extraction.
- [x] CarSky registry, ARM64, `NET_RAW`, ports, detector and risk configuration are documented.
