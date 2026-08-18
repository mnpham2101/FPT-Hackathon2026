# Final-Round Evidence Manifest

Copy this file to `test-report/final-round-<YYYYMMDD-HHMM>/MANIFEST.md` before a run. Fill every field from the same candidate and deployment. Do not commit the populated copy: `test-report/` is intentionally ignored.

## Candidate identity

| Field | Value |
|---|---|
| Evidence run ID | `final-round-<YYYYMMDD-HHMM>` |
| Git branch | `feat/final-round-evidence-presentation` |
| Git commit | `<40-character SHA>` |
| Release tag | `<tag or not-yet-tagged>` |
| GitHub Actions run | `<URL and run ID>` |
| Test operator | `<name>` |

## Runtime artifacts

| Artifact | Immutable identity |
|---|---|
| Scenario Player image | `registry.hackathon-2.carsky.io/m1-scenario-player@sha256:<digest>` |
| V2X ECU image | `registry.hackathon-2.carsky.io/m1-v2x-ecu@sha256:<digest>` |
| ADA ECU image | `registry.hackathon-2.carsky.io/m1-ada-ecu@sha256:<digest>` |
| IVI APK | `<filename>` · SHA-256 `<digest>` |
| Detector model | `ADA_ECU/models/yolo11n.onnx` · SHA-256 `<digest>` |
| Detector clip | `ADA_ECU/media/ego-b-occluding-c.mp4` · SHA-256 `<digest>` |

## CarSky run

| Field | Value |
|---|---|
| Blueprint | `m1_system_test` |
| Deployment/Room | `<deployment name and ID>` |
| Device | `<device name and ID>` |
| Scenario/config | `<path plus relevant env values>` |
| Start time | `<ISO 8601 with timezone>` |
| End time | `<ISO 8601 with timezone>` |
| Expected outcome | `<happy path, relay-disabled counterfactual, or degraded/recovery>` |

## Same-run evidence inventory

| Evidence | File | Time locator / result |
|---|---|---|
| Blueprint read-back | `<file>` | `<node/image/pin check>` |
| Scenario Player log | `<file>` | `<first and last relevant timestamp>` |
| V2X ECU log | `<file>` | `<rx/decode/R2-forward locator>` |
| ADA ECU log | `<file>` | `<own B, relayed C, assessment and R4 locators>` |
| IVI logcat | `<file>` | `<R4 RX and message-driven UI locator>` |
| Screen recording | `<file>` | `<Home → Warning → timeout/recovery locator>` |
| V2X pcap | `<file>` | `<47100 and 47200 packet counts>` |
| ADA pcap | `<file>` | `<47200 and 47300 packet counts>` |
| Zero-C receipt | `<file>` | `<exit code and non-zero examined counts>` |
| Automated summary | `<file>` | `<pass/fail>` |

## Expected versus observed

| Check | Expected | Observed | Verdict |
|---|---|---|---|
| B provenance | Detector produces only `own_sensor` / `own:*` records | `<locator>` | `<PASS/FAIL>` |
| C provenance | C exists only as `v2x_relayed` / `v2x:*` | `<locator>` | `<PASS/FAIL>` |
| Core chain | UDP traffic crosses ports 47100, 47200 and 47300 | `<pcap locators>` | `<PASS/FAIL>` |
| Fusion output | R4 warning is built while B and C are simultaneously valid | `<r4_tx locator>` | `<PASS/FAIL>` |
| Consumer outcome | IVI receives R4 and enters WarningView because of the message | `<logcat and video locators>` | `<PASS/FAIL>` |
| Counterfactual/degraded policy | Removing or staling relay input removes C warning safely; restoration recovers it | `<locators>` | `<PASS/FAIL>` |

## Scope statement

- CarSky Container/Skycraft nodes run workloads that **simulate the roles** of vehicle ECUs in this blueprint; they are not claims of physical ECU, complete vECU or HIL validation.
- Team-built and exercised: V2X/ADA/IVI applications, R1–R4 contracts, UDP message path, fusion/risk logic, HMI and evidence tools.
- Simulated inputs: vehicle B/C motion, CPM stimulus and the recorded camera clip.
- Deferred validation: production V2X radio, live calibrated camera, PKI/security, HIL, closed-track and road testing.

## Sign-off

| Role | Name | Verdict / date |
|---|---|---|
| Test operator | `<name>` | `<result>` |
| Demo owner | `<name>` | `<approved/rejected>` |
| Presentation owner | `<name>` | `<approved/rejected>` |
