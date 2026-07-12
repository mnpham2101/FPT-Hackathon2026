# Research Note — ADA ECU per the ada-ecu.svg Architecture: C++ Everywhere Except Object Detection?

> Researcher artifact — a user-directed study re-opening the §3(d) ADA language pick in [m1-cooperative-awareness.md](m1-cooperative-awareness.md) (currently mono-Python 3.11+), run per [requirement-analysis-and-solutioning](../.claude/skills/requirement-analysis-and-solutioning/SKILL.md) against [solution-selection-criteria.md](../.claude/rules/solution-selection-criteria.md). **No requirement numbers are minted or renumbered**; all R#/F# refer to the report. Module inventory read from [ada-ecu.svg](ada-ecu.svg) (drawio export; labels quoted as drawn, including the `Collision Risk Assessment` «interface» stereotype).

## The question, answered directly

**Per the [ada-ecu.svg](ada-ecu.svg) architecture: can C++ own every ADA module except object detection, with Python only inside `Cam Object Detector` as a subprocess-invoked callable binary? — Yes, capability-wise: every non-detection module is C++-feasible in M1** (per-module verdicts below; none is problematic, two carry caveats that are placement/design issues, not language issues). The Python-detector boundary is sound as specified (no FFI, no RPC — a subprocess emitting R3 TrackedObject JSON). **The cost is schedule, not capability:** the hybrid spends ≈ 1–2 weeks of the project's ≈ 1.5–2.5-week slack and needs a second C++ owner — see § Schedule consequence and § Recommendation.

## Module → language mapping (the diagram's inventory, module by module)

Verdict = can **C++ own this module in M1**: feasible / feasible-with-caveat / problematic. All named libraries are open-source and Linux-native (hard constraints pass).

| Diagram module | Report mapping | C++ in M1 | Libraries needed | Note |
|---|---|---|---|---|
| `DataObserver` / `V2X listener` (receives `Current Input`) | R7-output ingest + R9 GNSS ingest | **Feasible** | UDP socket (POSIX or standalone Asio); JSON parse (nlohmann/json, MIT) | ADA receives already-decoded R3 JSON from the V2X ECU — plain datagram read + parse |
| `DataObserver` / `Radar listener` | M1 realization = the R13 bench sensor-view ingest (`source = own_sensor`); real radar = out-of-M1 | **Feasible** (as the R13 listener) | Same UDP + JSON stack | Radar/lidar hardware is Cortex-M/sensor territory excluded by §1 (F12) — the bench injects the LOS-filtered object list directly; the diagram slot is *realized* by that stream in M1 |
| `DataObserver` / `Camera listener` | R20 clip path / R21 detector input + subprocess supervision | **Feasible** | File I/O; `posix_spawn`/`fork-exec` + pipe read of the detector's stdout | In the hybrid this module doubles as the detector-subprocess supervisor (restart, timeout, backpressure) |
| `Data Parser` / `V2X Msg Parser` | Thin R3-JSON validator — **not** CPM decode | **Feasible-with-caveat** | nlohmann/json (+ optional pboettch/json-schema-validator, MIT) | Caveat is placement, not language: CPM parsing is frozen in the V2X ECU (R7); as drawn the box could be misread as ADA-side CPM decode — resolve at HLD ([[project-architecture]]), don't silently re-scope R7 |
| `Data Parser` / `Cam Object Detector` | **R21 — the one Python module (the user's split)** | n/a — Python by design | Python 3.11 + ultralytics/onnxruntime + OpenCV; packaging per § The detector boundary | Timeboxed bonus, **off the P1 acceptance path** — R13 scenario detections remain the gating ADA source |
| `Data Parser` / `Radar Object Detector` | Out-of-M1 (future) | n/a in M1 | — | In M1 the bench emits already-parsed R3 objects — there is no radar detection step to implement; real radar detection follows the radar future |
| `Collision Risk Assessment` «interface» | R16 CRA abstraction | **Feasible** | Pure-virtual base + factory/registry (stdlib); config file lib for realization selection | R16's 0-diff plug-in KPI is passable in C++; caveat for later: plug-ins arrive by recompile/registration, a higher contribution bar than a Python registry |
| ↳ `Chained Collision` realization | R17 NLOS/TTC (the M1 scenario) | **Feasible** | Arithmetic + hysteresis; externalized config (toml++/yaml-cpp/JSON — constitution rule 5) | The highest-churn code in the project (threshold/TTC tuning weeks 3–8) — C++ is *capable*; iteration is slower (§ Schedule consequence) |
| ↳ `Intersection Collision (future)`, `Priority VehiclePremption` | Future R16 realizations | n/a in M1 | — | Intersection hazards already in § Future developments; priority-vehicle is **not yet listed** — add only by user decision (it matches FPT-Mentor's example list) |
| `Current TrackedObject` (store) | R15 track store + admission state machine | **Feasible** | Structs on the R3 schema; gtest for the hysteresis unit tests | State machine + distance gate — language-trivial |
| Output → `IVI ECU` | R4 warning events + 10 Hz state emission | **Feasible** | UDP + JSON serialize (nlohmann/json) | Identical wire behavior from C++ or Python — R4 is language-neutral by design |
| Output → `other ECUs/hardwares` | R8 store snapshot → V2X ECU Tx | **Feasible** | Same UDP + JSON stack | Broader "commands to other ECUs" is out of M1 scope — future item if the user wants it recorded |
| (cross-cutting) evidence logging | R23 JSONL event log | **Feasible** | `std::ofstream` append or spdlog (MIT) | |

**Per-module answer: yes across the board** — C++ can own `DataObserver` (all three listeners), `Data Parser` (minus the Python detector), the `Collision Risk Assessment` «interface» + `Chained Collision`, the `Current TrackedObject` store, both output paths, and logging. No module is problematic; the two caveats (`V2X Msg Parser` placement, plug-in contribution bar) are design-level, not language blockers.

## The detector boundary (`Cam Object Detector` — the one Python module)

- **The user's "no API" premise holds in the intended sense:** no FFI (pybind11/ctypes/embedded interpreter), no RPC framework. The boundary **is still an interface** — a process-level contract: argv + exit codes + **R3 TrackedObject JSON (JSONL) over stdout** (or file/socket), versioned like any seam.
- **R3 already makes the data language-neutral by design** — JSON Schema + per-language bindings + CI round-trips is R3's stated intent, and R4 does the same for risk output. The "producible by any programming language" demand is a **frozen contract property under every candidate**, including the current mono-Python pick.
- **Packaging** ([cache](../.claude/references/pyinstaller-onefile-packaging-ml.md)): PyInstaller **onefile** extracts to temp per run — seconds of startup at hundreds of MB for an onnxruntime+OpenCV bundle, so **one-shot-per-clip is the wrong invocation shape**; a **long-running process streaming JSONL per frame** pays startup once and is the sane mode. PyInstaller **onedir** or a plain **venv + entry script** give identical subprocess semantics with less/zero packaging toolchain — in a container, single-file packaging is cosmetic. Nuitka: longer builds, no inference speedup (ONNX Runtime is native under Python already). Licenses pass (PyInstaller GPL-2.0-with-exception, Nuitka Apache-2.0).
- **Contract impact:** the boundary never touches the committed P1 chain (R21 is the timeboxed bonus; R13 is the gating source), and the R3 swap test survives — bench objects and detector objects both arrive as R3 payloads.
- Note: this same detector-as-separate-process shape is **also available inside mono-Python** — process isolation is an architecture choice, not a language-pick consequence.

## Comparison baselines (rules require > 1 candidate; all pass hard constraints)

| Criterion | (a) mono-Python (current pick) | **(b) the diagram's hybrid: C++ all modules, Python detector binary** | (c) full C++ (detection via ONNX Runtime C++ API) |
|---|---|---|---|
| C1 — likelihood of accomplishing | **Best** — proven KPI headroom, one language, no new integration surface | Pass — every module C++-feasible (table above); conditions: second C++ owner or F6-owner split; subprocess supervision + C++ JSON/schema/test plumbing | Pass-with-conditions — additionally hand-builds letterbox/NMS/rescale ([cache](../.claude/references/onnxruntime-cpp-yolo-inference.md)); reference impls exist |
| C2 — fastest/easiest for M1 | **Best** — iteration speed on the highest-churn code (the original §3(d) driver) | ≈ 1–2 weeks slower (toolchain + plumbing + slower tuning loops on R17) | Worst — (b) plus a C++ vision pipeline inside R21's ≤ 2-week timebox |
| C3 — future features | Meets all listed futures; cheapest plug-in bar | Neutral-to-negative: listed futures are inference-bound (native under all candidates — the 56 ms/frame figure doesn't move with the caller's language); plug-in bar rises; pre-stages an ADA-port future **not on the § Future developments list** | As (b); genuinely reduces ego language count — its one real point |
| C4 — smaller footprint | **Best** — no packaging toolchain, no C++ additions | Adds packaging (or venv mgmt) + nlohmann/json + gtest + supervision code | Adds OpenCV-C++ + wrapper lib; removes Python |
| Verdict | **Achievable — recommended for M1** | **Achievable-at-risk** (schedule/skill — not capability) | Achievable-at-risk (heaviest effort) |

**No M1 KPI needs C++:** the report translated §1's "high-performance" into the R4/R7/R17 KPIs; at 10 Hz × 3 objects and ≤ 1 KB @ 10 Hz, Python holds with orders-of-magnitude headroom ([prior verification](m1-node-toolset-support-verification.md)). **The portability rationale doesn't transfer:** §1 gives the portability focus goal to the V2X ECU (telux is a C++ API); ADA's own goal line ends "object detection / AI deep learning is not an M1 focus", and no listed future ports ADA to vehicle hardware — if the user intends that, it's a new future item to add by decision, flagged here, not absorbed.

## Schedule consequence (the ~9-week window)

- F6 already commits the team's known C++ capacity to the V2X ECU (week-0 gate, named owner, deliberately small scope cap that stabilizes early).
- The hybrid puts **all committed P1 ADA scope** (R15/R16/R17/R23/R4-emission — the code tuned hardest in weeks 3–8) into C++, while Python keeps only the optional bonus — each language lands where it costs most/matters least for M1.
- Net: **+≈ 1–2 weeks** (toolchain, JSON/schema/gtest plumbing, slower tuning iterations) against **≈ 1.5–2.5 weeks total project slack** (report § Feasibility study result), with the IVI Kotlin ramp already the schedule-riskiest committed item — and it requires a **second C++-capable owner** (unverified team-capacity fact) or splits the F6 owner across two critical-path nodes.

## Recommendation

- **Capability answer to the user's question: yes** — C++ can own every ada-ecu.svg module except `Cam Object Detector`, which works as a Python callable binary behind an R3-JSONL subprocess boundary exactly as proposed.
- **Advisability under the ranked criteria: keep mono-Python for M1** (drivers: **C1 + C2**, C4 supporting; C3 does not counterweigh — futures are inference-bound and the criteria warn against over-engineering below C1/C2). The hybrid buys no M1 KPI and prices its cost in the project's entire slack.
- **Adopted regardless of language** (no ratification needed — changes no pick, scope, or KPI): the [ada-ecu.svg](ada-ecu.svg) module decomposition goes to [[project-architecture]] as HLD input for R15–R17 (it matches R16's intent well; `V2X Msg Parser` placement resolved there); detector-as-separate-process stays an open architecture option inside the current pick.
- If the user weighs the capability answer above the schedule cost, the hybrid is a legitimate, buildable choice — elect it via the package below.

## If the user elects the hybrid — ready-to-ratify package

Provided so an override is one decision, not a rework; **this note edits nothing in the report**:

- **§3(d) ADA bullet — replace with:** *"ADA: C++17 core + Python detector-as-callable-binary (user decision YYYY-MM-DD, electing the hybrid per m1-ada-dual-language-study.md). C++17 implements the ada-ecu.svg DataObserver, Data Parser (validator), R15 store, R16 CRA «interface» + R17 Chained Collision realization, R23, R4/R8 emission; Python implements R21 only, as a long-running subprocess (venv or PyInstaller-onedir) streaming R3 TrackedObject JSONL over stdout — no FFI, no RPC. Skill-gated per F19."*
- **Stack summary ADA row — replace with:** *"ADA ECU (app-level container) | C++17 core (skill-gated F19) + Python 3.11 detector subprocess | track store + CRA + NLOS/TTC + evidence logging (C++); YOLO11n JSONL detector (Python, off P1 path) | detector boundary = R3 JSONL over stdout"*
- **New flag F19 — draft:** *"F19 — ADA core language = C++17 (user-elected hybrid): week-0 skill gate mirroring F6 — named ADA C++ owner **distinct from the F6 V2X owner** (or explicit user acceptance of shared-owner risk), 1-day hello-world on the toolchain (UDP rx + JSON parse + one gtest); scope cap = R15/R16/R17/R23/R4-emission; detector stays Python behind the subprocess JSONL boundary in long-running streaming mode. Fallback pre-ratified: if the gate fails or R15–R17 are not KPI-green by end of week 5 (A), ADA reverts to mono-Python — R3/R4 make the reversal contract-loss-free."*

## Open items

- User decision: keep mono-Python (recommended) or ratify the hybrid package.
- User decision (independent of language): add "priority-vehicle preemption warning" and any "commands to other ECUs" to § Future developments, or leave unlisted.
- [[project-architecture]]: resolve the `V2X Msg Parser` placement (thin R3 validator in ADA vs R7 re-scope) when consuming the sketch as HLD input.

## Sources

- [ada-ecu.svg](ada-ecu.svg) — the user's ADA architecture diagram (module names quoted as drawn, incl. the «interface» stereotype).
- [m1-cooperative-awareness.md](m1-cooperative-awareness.md) §1 (ADA responsibilities, cloud table, § Future developments), §2 (R3/R4/R15–R17/R21/R23), §3(d)/(g), §4 (F6, F7, F11, F12).
- [m1-node-toolset-support-verification.md](m1-node-toolset-support-verification.md) — prior confirmation that Python meets every ADA KPI.
- Cached evidence: [onnxruntime-cpp-yolo-inference.md](../.claude/references/onnxruntime-cpp-yolo-inference.md), [pyinstaller-onefile-packaging-ml.md](../.claude/references/pyinstaller-onefile-packaging-ml.md), [yolo11-cpu-inference-benchmarks.md](../.claude/references/yolo11-cpu-inference-benchmarks.md).
