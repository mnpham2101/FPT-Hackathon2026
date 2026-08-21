# Final Round Readiness

## Goal

Produce one traceable final-round evidence set and presentation that directly closes the Round 2 feedback gaps without expanding the delivered architecture.

## Tasks

- [ ] Freeze the candidate identity: record the Git commit, branch/tag, GitHub Actions run IDs, image tags and immutable digests, APK checksum, blueprint name, scenario configuration and test timestamp in `test-report/final-round/MANIFEST.md`. → Verify: every video, log, pcap, screenshot and test result in the folder names this same run ID; any reused artifact is explicitly identified and explained.
- [ ] Preflight the existing evidence tools before consuming a CarSky Room: run the log-collector checks, pcap-extractor tests, ADA `check_zero_c.py` tests and all affected CI lanes; fix only defects that prevent final evidence collection. → Verify: local tests and CI are green, the extractor opens a committed/sample capture successfully, and no credential or generated evidence is tracked by Git.
- [ ] Capture one golden full-system run from `m1_system_test`: start screen recording before stimulus, collect every container log plus IVI logcat from the same deployment, save the blueprint read-back and screenshots, and extract V2X/ADA pcaps. → Verify: the set proves `10.99.0.10:47100 → .11`, `.11:47200 → .12`, `.12:47300 → .13`, IVI `[RX]`, message-driven `WarningView`, and visible A/B/ghost-C output from one time-correlated run.
- [ ] Close the provenance and counterfactual claim: derive detector R3 JSONL from the golden ADA evidence, run `ADA_ECU/tools/check_zero_c.py` with the ADA EVT log, then repeat the same scenario with the relay/fusion contribution disabled while leaving the remaining input unchanged. → Verify: the audit exits 0 with non-zero examined counts; the baseline produces relayed C warnings; the counterfactual produces no C warning; restoring the relay restores the warning.
- [ ] Capture one degraded/recovery run on the same artifact identity: stop or delay the CPM/R2 input long enough to expire C, then restore it; use the existing malformed/out-of-range case only if it can be captured without architecture changes. → Verify: correlated logs and screen show input loss, stale-track expiry or safe degradation, no stale warning, IVI timeout/idle behavior, and successful reacquisition after restoration.
- [ ] Publish the evidence report: add a concise final-round delivery report under `documents/Delivery/` with an expected-versus-observed table, evidence locators, metrics, limitations and exact terminology for CarSky workloads that simulate ECU roles. → Verify: every claim resolves to a file and timestamp in the manifest; simulated inputs, real software/network paths and deferred radio/camera/HIL validation are separated explicitly.
- [ ] Build the final presentation and demo script from the verified report: problem, buyer/decision point, team-owned solution, architecture/contracts, live demo, golden-run proof, degraded behavior, one ADA-focused SKU, pilot KPIs, limitations and roadmap. → Verify: the deck renders cleanly, the demo has a recorded fallback from the same run, the core story fits the assigned time, and no slide claims road/vehicle validation that was not performed.
- [ ] Rehearse the panel session and freeze the submission candidate: run timed presentation/demo rehearsals, answer the provenance, stale-data, spoof/conflict, latency, YOLO training, CarSky-boundary, baseline and commercialization questions, then tag the approved candidate. → Verify: two consecutive rehearsals finish within the time limit, each primary claim is answered with an evidence locator, and the final tag matches `MANIFEST.md`.

## Done When

- [ ] One same-run bundle connects source commit, images/APK, blueprint, scenario, video, per-node logs, IVI logcat, pcaps and automated results.
- [ ] Happy-path, zero-direct-C counterfactual and degraded/recovery outcomes are all demonstrated with expected-versus-observed receipts.
- [ ] The presentation uses accurate CarSky terminology, focuses on one ADA/cooperative-awareness offering and has a tested offline fallback.

## Notes

- Critical path: identity freeze → tool preflight → golden run → provenance/degraded runs → report → deck → rehearsal.
- Do not add lane/corner scenarios or change the architecture unless the three evidence runs above are already complete and stable.
- `test-report/` remains generated and git-ignored. Commit the manifest template, report, deck and reusable test/tool fixes; do not commit credentials or raw sensitive platform exports.
- Round 2 feedback source: <https://docs.google.com/document/d/1chV0Zub9I3MCwSmRXKKUVFT-ZczgccriV_QHSsmJoM0/edit?tab=t.0>.
