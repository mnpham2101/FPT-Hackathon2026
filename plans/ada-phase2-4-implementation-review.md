# ADA ECU Phase 2–4 implementation review

Audit date: 2026-08-03  
Reviewed tree: `4b4bae1` (`feat/phase2-ada-scaffold`, after merging `main`)  
Source plans: `milestone1.md`, `phase2_tasks.md`, `phase3_tasks.md`, `phase4_tasks.md`  
Design of record: `ADA_ECU/doc/phase2-4-ada-ecu-hld.md`

This is an audit copy. It does not replace the source plans. Stable IDs are retained. A box is marked `[x]` only when the current tree contains the implementation, a relevant automated build/test passes, and an identifiable commit provides provenance. Unchecked rows carry one classification: **Missing**, **Partial**, **External**, or **Optional**.

## Audit evidence and limits

- Current-tree C++ verification: `cmake --build ADA_ECU/build -j 4` and `ctest --test-dir ADA_ECU/build --output-on-failure` passed **14/14** on 2026-08-03.
- Current-tree Python verification: `python3 -m unittest discover -s ADA_ECU/tools/tests -p 'test_*.py'` passed **12/12**.
- A clean configure under `/tmp` could not download GoogleTest because the audit sandbox had no DNS. This is not a product-test failure; the populated project build completed.
- Principal implementation commits: `00b50ad` (detector/fusion), `ed7eee5` (runtime hardening), `c33b6c5` (YOLO full-clip evidence), `e2a8db1` (ML detector/demo), `3d55d7b` (video/provenance), `9f893b6` (folder migration). Most do **not** follow the planned `X.Y.Z.W` atomic-commit format, so provenance exists but subtask atomicity is not demonstrated.
- No CarSky Room deployment log, Running screenshot/status, registry-pull evidence, deployed inference measurement, live View Log, or live ADA→IVI pcap is committed. This review therefore makes no CarSky acceptance claim.

## Executive verdict

The present code demonstrates the intended local behavior: video/YOLO emits B as `own_sensor`; UDP R2 supplies C as `v2x_relayed`; ADA composes `d_AC = d_AB + d_BC`; edge-triggered R4 carries both objects; local loopback, negative control, evidence tooling, contract tests, and an ARM64 image build exist. However, it does **not** align structurally with the approved HLD/task plan. The implementation retained the earlier `include/ada` monolith, duplicate local types and file-based config instead of rebuilding around frozen bindings, parser/observer/store/CRA/output boundaries, a bounded queue, and an assessment database. Phase 3 likewise remains a monolithic `tools/video_detector.py`, not the planned `detector/` package. Local functional readiness is substantially ahead of plan status, but formal plan conformance and deployed acceptance are incomplete.

## Phase 2 checklist

| Status | Stable ID | Acceptance criterion (preserved) | Audit evidence / finding |
|---|---|---|---|
| [ ] | `5.2.1.1` | `ada-ecu/` absent; contract sync and ADA build green. | **Missing (blocking):** lowercase `ada-ecu/` still exists. HLD D1's consolidation has not occurred. |
| [ ] | `5.2.1.2` | Retired ADA deck files absent and no links resolve to them. | **External:** both `presentation/ada/ada-phase2-3-4-deck.md` and `.html` still exist; explicitly user-gated and outside product code. |
| [ ] | `5.2.1.3` | README links to HLD; links resolve; no requirement text is restated. | **Partial:** README exists but has no HLD link (`rg phase2-4-ada-ecu-hld ADA_ECU/README.md` is empty). |
| [ ] | `13.2.2.1` | Build/ctest green; tunables only in the config defaults table. | **Partial (high):** `config.cpp`/`config.hpp` work and tests pass (`00b50ad`, `ed7eee5`), but the HLD requires env-only config while runtime still requires `config/ada-ecu.conf`; names/defaults also diverge (`MISS_LIMIT_MS`, `IVI_HOST`, ports 46002/46004). |
| [ ] | `6.2.2.2` | Build/ctest green; socket headers occur only under `src/net/`. | **Partial (high):** UDP works, but socket API is duplicated in `src/udp_r2_receiver.cpp` and `src/udp_r4_sender.cpp`; no sole `src/net/udp_socket` holder. |
| [ ] | `18.2.2.3` | Build/ctest green; frozen `[EVT]` field vocabulary is emitted. | **Partial (high):** payload-carrying `[EVT]` exists (`event_logger.cpp`, `ed7eee5`), but shape is `{ts,event,payload}`, not HLD `{event,mono_ms,epoch_ms,counters,payload}`; no counters/monotonic stamp. |
| [ ] | `3.2.2.4` | Bounded queue tests pass deterministically without sleep synchronization. | **Missing (high):** no bounded `InputQueue`; `main.cpp` directly coordinates inputs. |
| [ ] | `2.2.3.1` | R2 valid/null-speed/out-of-range/malformed cases pass; mapping uses frozen binding and `v2x:<stationId>:<objectId>`. | **Partial (high):** mapping/id/source are tested, but `r2_mapper.cpp` uses a second ADA model rather than the frozen binding end-to-end; the planned malformed/range suite is absent. |
| [ ] | `3.2.3.2` | R3 parser accepts valid lines, rejects missing/invalid/unknown fields, and ignores incoming state. | **Partial:** `r3_mapper.cpp` and detector ingest work, but planned strict parser coverage and module boundary are absent. |
| [ ] | `3.2.3.3` | Every malformed corpus case has an explicitly asserted disposition. | **Missing:** no `tests/fixtures/malformed/` corpus. |
| [x] | `3.2.4.1` | All R3 fields survive store round trip; source-aware nearest/all behavior is tested; build/ctest green. | `include/ada/track_store.hpp`, `src/track_store.cpp`, `tests/track_store_tests.cpp`; current 14/14; commits `00b50ad`, `ed7eee5`. Path differs from plan but objective and tests are identifiable. |
| [ ] | `13.2.4.2` | Every admission-diagram edge is covered; build/ctest green. | **Partial (high):** own/V2X admission, hysteresis, hit promotion and timeout behavior exist, but are embedded in `TrackStore`; there is no separate state machine and no traceable edge-by-edge test matrix. Expired objects remain as `not_tracked`, contradicting HLD D3's erase rule. |
| [x] | `13.2.4.3` | Integration admits only inside enter gate, holds through hysteresis, expires by timeout, emits transitions; build/ctest green. | Store integration and source-specific expiry are exercised by `track_store_tests.cpp` and phase4 CI; commits `00b50ad`, `ed7eee5`. |
| [ ] | `14.2.5.1` | CRA interface frozen and build/ctest green. | **Partial (high):** `CollisionRiskAssessor` interface exists, but differs materially from HLD `ICollisionRiskAssessment/RiskContext/RiskFinding` and has no assessment DB context. |
| [ ] | `14.2.5.2` | CRA record schema is valid, sample validates, and contract sync remains green. | **Partial (high):** `schemas/cra_assessment_record.schema.json` exists (`00b50ad`) but wrong planned path/name, no sample, and no test loads/validates it. |
| [ ] | `14.2.5.3` | Typed database accessor enforces the committed schema; build/ctest green. | **Missing (blocking for strict R14):** no assessment database/accessor. State is held inside `NlosRiskAssessor`. |
| [ ] | `14.2.5.4` | Registry duplicate/unknown/enable-list tests pass; plugin addition needs no core edits. | **Partial (high):** `make_builtin_assessor()` is a factory conditional, not a registry; no duplicate/unknown registry tests and adding a plugin edits the factory. |
| [x] | `2.2.6.1` | V2X listener receives R2 through UDP and build/ctest stays green. | `udp_r2_receiver.*`, `v2x_r2_ingest.*`, live loopback in `phase4-ci.yml`; commits `00b50ad`, `ed7eee5`. Sole-socket detail remains charged to `6.2.2.2`. |
| [x] | `12.2.6.2` | Detector subprocess stdout is consumed, clean/non-zero lifecycle handled, no orphan remains; build/ctest green. | `detector_process.*`, `detector_jsonl_ingest.*`, restart/loop wiring and tests/CI; commits `00b50ad`, `ed7eee5`. |
| [x] | `3.2.6.3` | Mock fixtures validate against synced R2/R3 and loopback datagrams are received byte-identically. | `testdata/r3_own_sensor.jsonl`, `tools/mock_v2x_sender.py`, tool unit tests and phase4 live-UDP CI; `00b50ad`/`ed7eee5`. Fixture path differs but evidence is executable. |
| [x] | `13.2.6.4` | Composition root links; expiry/fusion tick continues during silence; build/ctest green. | `src/main.cpp`; current build/ctest; periodic tick hardened in `ed7eee5`. |
| [ ] | `18.2.6.5` | Checker passes conforming log and rejects illegal edge, early promotion, mid-band drop, and empty EVT. | **Partial:** checker and 12 Python tests exist, but current CLI is a combined-chain checker; the exact Phase-2 state-machine negative matrix is not all represented. |
| [x] | `5.2.7.1` | Shell checks pass; executable entrypoint; single-platform Linux/ARM64 image builds. | `Dockerfile`, `entrypoint.sh`, `.dockerignore`; ARM64 lane and recorded local ARM64 build; commits `c33b6c5`, `00b50ad`, `ed7eee5`. |
| [ ] | `5.2.8.1` | Dedicated `phase2-ci.yml`/`ada-ecu-image` lane is valid and green. | **Missing (high):** no `.github/workflows/phase2-ci.yml`; ARM64 build is in `phase3-ci.yml`. |
| [ ] | `13.2.8.2` | Dedicated loopback lane observes full tentative→tracked→not_tracked cycles for each source. | **Partial:** phase4 CI exercises positive/negative integration but does not assert the exact per-source Phase-2 cycle. |
| [ ] | `12.2.9.1` | Clip preflight script compiles and its tests pass locally and in CI. | **Missing (medium):** `tools/check_clip_spec.py` does not exist; benchmark checks only part of the intended spec. |
| [ ] | `12.2.9.2` | Proposal send/reply is recorded in `plans/doc/phase2-ada-scaffold-run.md`. | **External:** no run record; requires human/FPT-Mentor action. |
| [x] | `12.2.9.3` | Demo clip exists, passes intake constraints, and content is confirmed; task is optional/non-blocking. | Clip and provenance sidecar exist (`ADA_ECU/media/*`, `3d55d7b`); automated full-clip/zero-C evidence in `c33b6c5`. Human visual confirmation is documented in sidecar rather than a Phase-2 run record. |
| [ ] | `5.2.9.4` | Node-config JSON is valid; env names exactly match code/detector; links resolve. | **Partial (blocking deploy):** guide exists and recent plan commits explicitly identify its deploy-blocking half, but code still uses aliases/defaults that diverge from HLD names; must revalidate before CarSky. |
| [ ] | `13.2.10.1` | Milestone and HLD have no contradictions; changes are documented in an atomic task commit. | **Partial:** milestone was updated in `ed7eee5`, but implementation still contradicts HLD D1–D4/D8 and commit is not task-atomic. |

## Phase 3 checklist

| Status | Stable ID | Acceptance criterion (preserved) | Audit evidence / finding |
|---|---|---|---|
| [ ] | `12.3.1.1` | ARM64 wheel lane prints versions and either passes or names the blocking package. | **Partial:** `phase3-ci.yml` builds Linux/ARM64 and prior report records native wheels, but no dedicated install/version-print lane. |
| [ ] | `12.3.2.1` | Detector config tests pass and only config module reads environment. | **Partial (high):** CLI/config logic is concentrated in `tools/video_detector.py`; planned `detector/config.py` does not exist. |
| [ ] | `12.3.2.2` | Frame-source tests pass and OpenCV calls occur only in `detector/frame_source.py`. | **Partial (high):** video decode works and is smoked, but OpenCV is embedded in the monolithic script; seam/module rule unmet. |
| [ ] | `12.3.2.3` | Distance tests pass for known bbox values/invalid inputs. | **Partial:** pinhole distance exists and full-clip trend is measured, but no isolated distance module/unit suite. |
| [ ] | `12.3.2.4` | Tracker tests prove stable IDs and threshold-controlled reassociation. | **Missing (high):** output is effectively fixed to `own:B`; no IoU tracker module or association tests. |
| [ ] | `12.3.2.5` | ONNX Runtime CPU session test executes with committed model and expected shapes. | **Partial:** real ONNX inference and ML smoke pass (`e2a8db1`, `c33b6c5`), but no isolated session/shape unit test at planned boundary. |
| [ ] | `12.3.2.6` | Emitted JSONL validates by loading synced R3 schema; tests pass. | **Partial:** stdout R3 JSONL and round-trip tests exist; monolithic emitter does not implement the planned schema-loading boundary. |
| [ ] | `12.3.2.7` | Entrypoint tests pass and stdout is asserted JSONL-only. | **Partial:** CLI works and ML evidence uses stderr, but no dedicated `detector/main.py` and no explicit stdout-purity test. |
| [ ] | `12.3.3.1` | Exporter compiles; committed ONNX opens and exposes expected shapes. | **Partial:** model is committed and loads, but `export_yolo11n.py` is absent; `download_yolo_model.py` downloads rather than reproducibly exports. |
| [x] | `12.3.3.2` | Generated fixture opens through the frame source and yields declared frame count. | `tools/make_sample_video.py`, smoke/unit suite, `ed7eee5`; the committed demo clip, not synthetic output, is used for acceptance. |
| [ ] | `12.3.3.3` | Detector lane is valid, green, and has zero dependency skips. | **Partial:** `detector-video-and-ml` is comprehensive but does not install/run the planned `detector/` package tests or assert zero skips. |
| [ ] | `12.3.4.1` | Final clip preflight exits zero and a run report records actual-vs-expected attributes. | **Partial:** metadata and benchmark are in `phase2_3_4_report.md`; designated preflight tool/run document are absent. |
| [ ] | `12.3.4.2` | Retired without implementation; scope moved to `12.3.7.2`. | **Optional:** stable ID intentionally superseded; no work should be committed under it. |
| [ ] | `12.3.4.3` | B distance trend is monotonic through approach, crosses gate once, and final camera constants are committed in config and node guide. | **Partial (medium):** report shows approach and one crossing, but only 87.76% raw non-increasing steps; calibration uses `CAMERA_FOCAL_PX=2000`, diverging from planned `VEHICLE_WIDTH_M/CAMERA_HFOV_DEG`, and no designated run doc. |
| [x] | `12.3.5.1` | Zero-C checker rejects empty/malformed/C-like inputs and passes compliant data locally/CI. | `tools/check_zero_c.py`, `test_check_zero_c.py`; `c33b6c5`, hardened `ed7eee5`; current Python suite passes. |
| [x] | `12.3.5.2` | Full clip yields non-empty R3 log, B coverage/distance/warm-up/≥5 Hz/zero-C KPIs are recorded. | `benchmark_video_detector.py`, `phase2_3_4_report.md`; 50/50, 20.395 Hz local and 17.543 Hz ARM64, zero-C; `c33b6c5`, `ed7eee5`. |
| [x] | `3.3.5.3` | Real detector output reaches store as tracked `own_sensor`; no fixture is required in this acceptance arm. | ML output→ADA integration documented and detector subprocess wired; commits `c33b6c5`, `00b50ad`, `ed7eee5`. Phase4 CI still uses a deterministic `cat` seam, so real-ML integration evidence is local/report evidence. |
| [x] | `12.3.5.4` | CI zero-C lane is green with a non-zero examined count. | Full-clip step in `phase3-ci.yml` runs benchmark then `check_zero_c.py`; `c33b6c5`. |
| [x] | `5.3.6.1` | ARM64 image contains detector/model/video and starts detector to emit R3. | Dockerfile copies tools/model/media; `arm64-container` lane and recorded 14/14/in-image smoke; `c33b6c5`, `00b50ad`, `ed7eee5`. |
| [ ] | `5.3.6.2` | ADA is Running on CarSky and deployed inference rate is recorded in `phase3-ada-detector-run.md`. | **External (blocking acceptance):** not deployed; no run document/evidence. |
| [x] | `12.3.7.1` | Complete provenance sidecar records real source/licence/content verdict; raw source is untracked. | `media/ego-b-occluding-c.source.md`, `.gitignore`; atomic tagged commit `3d55d7b`. |
| [x] | `12.3.7.2` | Final ffmpeg-produced clip is committed, H.264/MP4 spec/content checks pass, image contains it, and binary handling is correct. | `media/ego-b-occluding-c.mp4`, Dockerfile, provenance/report, phase3 CI; `3d55d7b`, `c33b6c5`. |
| [ ] | `5.3.7.3` | Registry push records media layer bytes/digests and second push uploads zero media bytes. | **External:** requires Zot credential/registry push; no evidence record. |

## Phase 4 checklist

| Status | Stable ID | Acceptance criterion (preserved) | Audit evidence / finding |
|---|---|---|---|
| [x] | `15.4.1.1` | Composition uses `d_AC=d_AB+d_BC`, lateral sum, reports unknown B, and build/ctest passes. | `scene_composer.*`, tests in `track_store_tests.cpp`; `ed7eee5`; current 14/14. |
| [ ] | `14.4.1.2` | NLOS plugin registers through CRA registry, reads/writes assessment DB, covers B-unknown/C-clear/threshold/TTC, and adding it is one module plus one registry line. | **Partial (blocking strict R14):** behavior and B-unknown/threshold coverage exist, but there is no DB or real registry and implementation is not the planned plugin boundary. |
| [x] | `14.4.1.3` | Injectable-time dwell suppresses flapping; every `low→medium→high→medium→low` change emits once, with clearing snapshot. | Dwell/transition logic uses supplied `now_ms`, clear preserves snapshots/geometry, phase4 CI observes medium→high→low; `ed7eee5`; current tests pass. Exact five-transition unit sequence could be stronger but core criterion is executable. |
| [x] | `15.4.2.1` | Warning maps finding/geometry through frozen R4 binding and emitted cases schema-validate, including null C clear and full R3 object. | `warning_builder.cpp` uses `contracts::R4WarningEvent`; R4 round-trip/schema/CI checks; `00b50ad`, `ed7eee5`. |
| [ ] | `15.4.2.2` | One UDP datagram per R4; loopback exact JSON; send failure counted/nonfatal; full body in `r4_tx`. | **Partial (high):** successful loopback/full-body/delivery status are proven, but unreachable-send counted/nonfatal lacks a focused automated test and socket boundary diverges. |
| [x] | `15.4.2.3` | Main tick runs expire→assess→compose→build→send and CI observes `r4_tx`. | `main.cpp`, phase4 positive/negative lane; `00b50ad`, `ed7eee5`. |
| [ ] | `15.4.2.4` | When enabled, periodic state has monotonic seq; default zero emits no state datagrams. | **Optional:** not implemented; explicitly non-gating. |
| [x] | `18.4.3.1` | Configurable loopback receiver records full bodies, schema-validates, and enforces minimum count. | `mock_ivi_receiver.py`, phase4 CI positive/negative arms; `00b50ad`, `ed7eee5`. Validation is performed by downstream checker rather than receiver `--validate`, a harmless workflow divergence. |
| [x] | `18.4.3.2` | Offline event report renders chronological risk events and correct summary counts from EVT alone. | `event_report.py`, phase4 CI report artifact; `00b50ad`, `ed7eee5`; Python suite passes. |
| [ ] | `18.4.3.3` | Checker supports fusion/both-tracks/schema modes and rejects each enumerated broken chain. | **Partial (high):** current checker proves both tracks, R4 body/schema and delivery, with negative tests, but CLI collapsed modes and does not explicitly enforce every planned transition-order/b_unknown invariant. |
| [x] | `6.4.4.1` | Capture script passes shell syntax, degrades gracefully, and export-one round-trips byte-identically. | `capture.sh`, phase4 CI capture validation; `00b50ad`, `ed7eee5`. |
| [x] | `6.4.4.2` | Extractor passes syntax, prevents path escape/no-marker success, and round-trips bytes. | `extract_pcap.sh`, phase4 CI `cmp`; `00b50ad`, `ed7eee5`. |
| [x] | `15.4.5.1` | E2E CI proves live detector seam + UDP R2 + schema-valid UDP R4 + event report and out-of-range zero-R4 control, with low/medium/high progression. | `.github/workflows/phase4-ci.yml`; hardening commit `ed7eee5`. Current workflow uses explicit distances and receives 3 datagrams. |
| [ ] | `5.4.6.1` | Current ARM64 image tag is pullable from custom registry and push is recorded. | **External (blocking deploy):** local image built, but Zot push not performed and no registry record exists. |
| [ ] | `5.4.6.2` | ADA node is configured and per-node CarSky status is Running/restart 0 in run doc. | **External (blocking acceptance):** no Room evidence. |
| [ ] | `13.4.6.3` | Live default and out-of-range CarSky runs prove C lifecycle/source and zero R4 in negative scenario. | **External (blocking acceptance):** only local/CI equivalents exist. |
| [ ] | `18.4.6.4` | Saved live View Log passes both-tracks/schema checks and records B/C excerpts plus event report. | **External (blocking acceptance):** local EVT evidence exists; no deployed View Log/run doc. |
| [ ] | `15.4.6.5` | Live ADA→IVI pcap is archived and Wireshark-decoded with at least one matching R4. | **External (blocking acceptance):** capture tooling is ready; no live pcap. |
| [x] | `4.4.7.1` | Additive `trackedObjects` is synced; old samples remain valid; contract sync/CI pass. | Root and node schemas/samples updated in `00b50ad`; sync/round-trip tested. User ratification is recorded in conversation/plan state. |
| [x] | `4.4.7.2` | ADA binding/emitter round-trip includes both tracked objects; additive-version test still passes. | `src/contracts/r4_message.*`, `warning_builder.cpp`, contract tests; `00b50ad`, `ed7eee5`; current 14/14. |
| [ ] | `4.4.7.3` | IVI Kotlin binding decodes with/without array and IVI tests are green. | **External (Phase 5):** plan hands this to IVI owner; prior PR check reported duplicate `R4Json`. Do not modify from ADA branch without owner agreement. |
| [x] | `4.4.7.4` | Evidence checker/CI asserts B and C on the R4 wire. | `check_evt_log.py`, tool tests, phase4 CI; `00b50ad`, `ed7eee5`. |
| [ ] | `20.4.8.1` | Scenario Player TX log adds injectable monotonic stamp and its tests/CI pass. | **External:** belongs to Scenario Player and is intentionally not implemented by ADA work. |
| [ ] | `21.4.8.2` | Post-run K1–K5 checker exists, compiles, passes its complete test matrix, and externalizes all bounds. | **Missing (low/non-gating):** `check_run_alignment.py` and tests do not exist; scheduled after Phase 3/4 acceptance. |

## Milestone acceptance mapping

| Milestone acceptance | Local status | Deployment status |
|---|---|---|
| Phase 2: R3 fields and same interface for own/V2X | **Partial:** functional common store, but duplicate model/parser boundaries diverge from frozen-contract HLD. | Not required for unit closure; no Phase-2 CarSky run record. |
| Phase 2: R13 lifecycle, hysteresis, externalized gate constants | **Pass functionally** in C++ tests/CI; HLD erase semantics and env-only config are not aligned. | Not proven live. |
| Phase 2: CRA DB schema and mentor video proposal | **Fail/Partial:** schema exists but is not enforced; DB accessor absent; mentor-send record absent. | External follow-up needed. |
| Phase 3: real B detections, distance, own_sensor, zero C, CPU ≥5 Hz | **Pass locally and in recorded ARM64 container evidence.** | Deployed-node rate remains unproven. |
| Phase 4: relayed C lifecycle, CRA/plugin/DB | **Partial:** lifecycle/risk behavior passes; planned CRA registry/database architecture is absent. | Live bench/CarSky lifecycle unproven. |
| Phase 4: composed R4 and complete offline event evidence | **Pass locally/CI**, including both `trackedObjects`. | Live IVI delivery and pcap unproven. |

## SOLID assessment against actual boundaries

| Principle | Verdict | Evidence and required response |
|---|---|---|
| SRP | **Partial / weak** | `tools/video_detector.py` owns config, OpenCV frame acquisition, preprocessing, ONNX inference, selection, distance, tracking identity, emission and CLI. `TrackStore` owns storage plus admission policy. `main.cpp` owns orchestration, lifecycle, fusion, event shaping and shutdown. Split to HLD boundaries if plan conformance is chosen. |
| OCP | **Fail for CRA; partial elsewhere** | New risk algorithms require editing `make_builtin_assessor()` and likely concrete state/output flow. HLD's registry + `RiskContext` + DB was designed to avoid this. Detector backends are selected in one monolithic CLI branch rather than injected modules. |
| LSP | **Mostly pass but under-tested** | `NlosRiskAssessor` substitutes for `CollisionRiskAssessor`, and no unsafe downcast is visible. There is only one implementation and no polymorphic contract test, so substitutability is asserted more than demonstrated. |
| ISP | **Pass at small interfaces; incomplete architecture** | `CollisionRiskAssessor::assess` is narrow; sender/receiver classes are focused. Missing frame-source/inference/emitter and parser/store interfaces means consumers still depend on concrete multipurpose modules. |
| DIP | **Fail at composition edges** | Core code depends on concrete `UdpR2Receiver`, `UdpR4Sender`, `DetectorProcess`, concrete clock calls and concrete store. Planned socket holder, input queue, injectable clock/context, CRA registry and DB seams are absent. Local tests compensate through loopback but do not provide architectural inversion. |

Overall: the implementation is pragmatic and demo-capable, but it does **not** meet the HLD's intended SOLID level. The most consequential debt is not naming/path cosmetics: it is missing CRA persistence/registry seams, a bounded single-writer input model, and detector separation.

## Plan-vs-implementation divergences

1. **Canonical folder conflict remains:** both `ada-ecu/` and `ADA_ECU/` exist despite HLD D1.
2. **Frozen-model rule is bypassed:** runtime uses `include/ada/types.hpp` plus mappers while frozen bindings remain in `src/contracts/`; this creates two object models.
3. **Thread/input architecture differs:** no bounded queue and no clearly enforced single-writer event pipeline.
4. **Config differs:** HLD env-only D9 was replaced by mandatory `.conf` plus env aliases; documented port/env names can drift.
5. **Socket architecture differs:** two transport classes include OS socket APIs instead of one socket holder.
6. **Admission differs:** embedded in store; expired tracks remain `not_tracked` rather than being absent.
7. **R14 differs materially:** interface exists, but no assessment DB, schema enforcement or extensible registry.
8. **R18 vocabulary differs:** no monotonic timestamp/counters, preventing the planned R21 K1–K5 checker input.
9. **Detector packaging differs:** one large `video_detector.py` instead of config/frame-source/distance/tracker/inference/emitter/main modules; fixed `own:B` substitutes for planned association.
10. **CI topology differs:** no Phase-2 workflow and no dedicated detector-wheels lane; later workflows cover much of the behavior but do not close the exact planned checks.
11. **Task history differs:** bulk commits combine many stable task IDs and use no `[X.Y.Z.W]`, so the plan's atomic traceability rule cannot be retroactively proven.
12. **Acceptance evidence location differs:** results live mainly in `ADA_ECU/docs/phase2_3_4_report.md`; designated `plans/doc/phase2-ada-scaffold-run.md`, `phase3-ada-detector-run.md`, and `phase4-ada-fusion-run.md` are absent.

## Gaps ordered by severity

### Blocking

1. CarSky registry push/deploy and live Phase 3/4 evidence (`5.3.6.2`, `5.4.6.1`–`15.4.6.5`) are not done.
2. Strict R14 acceptance is not met: no assessment database/accessor or enforced database schema (`14.2.5.2`–`14.2.5.3`, `14.4.1.2`).
3. Two ADA folders remain (`5.2.1.1`), creating an ambiguous node/build source.

### High

1. Decide whether the HLD remains binding. If yes, refactor runtime to frozen contract types, queue/single-writer flow, socket holder, admission module, CRA registry/database, and Phase-3 package seams before calling plan tasks done.
2. Resolve config and node-guide drift before deployment (`13.2.2.1`, `5.2.9.4`).
3. Add exact planned negative tests for malformed parsers, state-machine edges, UDP failure and fusion checker invariants.
4. Coordinate `4.4.7.3` with Phase 5 owner; do not patch IVI from ADA scope.

### Medium

1. Add clip-spec preflight and designated run records.
2. Prove association/tracking rather than always emitting `own:B`, or ratify fixed-single-B identity as an M1 simplification.
3. Add/dedicate Phase-2 and ARM64 wheel CI checks if exact plan traceability is required.
4. Reconcile documented camera calibration parameters with implemented focal-pixel calibration.

### Low

1. Implement R21 run-alignment instrumentation/checker only after required acceptance; it is explicitly non-gating.
2. Periodic R4 state remains optional and may stay deferred.
3. Retire the stale presentation only with the user's explicit approval.

## Choices requiring user ratification

1. **Architecture direction:** choose **A—follow the approved HLD** (recommended for maintainability/SOLID and tester resilience) and refactor behind unchanged R2/R3/R4 behavior, or **B—ratify current simpler implementation** and amend HLD/tasks/acceptance to match it. Do not leave both as simultaneous sources of truth.
2. **Admission storage:** ratify HLD's "`not_tracked` means absent/erased" or current implementation's retained `not_tracked` objects. This affects clear-warning snapshots and checker semantics.
3. **Detector identity:** require real frame-to-frame association (`own:<n>`) or ratify a single fixed `own:B` because M1 has exactly one visible occluder.
4. **Config authority:** ratify env-only CarSky configuration or retain `.conf` + env overrides. If retaining aliases, freeze one documented canonical env set before deployment.
5. **CRA database:** implement the HLD schema-backed accessor (recommended because R14 names it), or seek requirement-owner approval to treat in-assessor memory plus EVT as the database artifact.
6. **Commit traceability:** accept the existing bulk commits as historical evidence and enforce `[X.Y.Z.W]` only going forward, or require a non-destructive mapping note; history should not be rewritten on an open/shared PR merely to manufacture atomicity.
7. **Deployment acceptance:** confirm the custom tenant/registry/Room and complete the walkthrough-led human steps before any Phase 3/4 completion claim is changed from local to deployed.

## Recommended next decision sequence (no implementation implied)

1. Ratify architecture choices 1–5 above.
2. If HLD remains binding, create a new remediation plan with stable IDs (do not reuse or renumber the existing IDs) and acceptance-first tests.
3. Close only local blocking/high gaps selected by that decision.
4. Obtain Zot registry access, push the immutable ARM64 tag, then execute the CarSky walkthrough and archive live log/pcap evidence.
5. Update the original phase plans only after user ratification; this audit copy remains the comparison baseline.
