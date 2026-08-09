# Delivery documents

Reports on what was delivered and the evidence behind it: run records, acceptance write-ups, and the aggregate report the lead assembles from the rest of [documents/](../../).

## Documents

| Document | Form | Topic |
|---|---|---|
| [apk-deploy.md](apk-deploy.md) | Human procedure | Build the IVI APK, install it on the Skycraft (AAOS) node, confirm the app on screen |
| [testing-guide.md](testing-guide.md) | Human procedure | The two test paths, the available blueprints, and the screen/log/pcap evidence a run must produce |
| [baseline-connectivity-smoke-test.md](baseline-connectivity-smoke-test.md) | Research note — **not authoritative** | The phase 0 smoke test: prove a datagram crosses bench → V2X → ADA → IVI before any ECU code exists. Pass criteria C1–C5, the `tools/netcheck/` implementation, manual steps M1–M12 |

Read the first two in that order — the testing guide assumes an installed app. Each numbers its own steps from 1, so a step number is only ever a step of the document naming it.

## Figures

Committed beside the document that renders them; nothing else links them.

| File | Used by |
|---|---|
| [phase5-ivi-test-isolated.svg](phase5-ivi-test-isolated.svg) · `.drawio` | testing-guide.md — the isolated IVI test topology. The `.drawio` is the editable source; edit both together |
| [phase2-4-ada-test-isolated.svg](phase2-4-ada-test-isolated.svg) | testing-guide.md — the same shape for a non-IVI node |
| `4-blueprints.png` | testing-guide.md — the Nydus blueprint list, and a blueprint open on the canvas |
| `download-apk-githubAction.png` | apk-deploy.md — the CI artifact at the end of the `ivi-assemble` job |
| `get-apk-upload-command.png` | apk-deploy.md — the Devices panel, the IVI ADB tab and **Local ADB** |

The evidence itself — CI runs, Room logs, recorded demos — is cited from here by its source, not copied into this folder. The acceptance criteria a run is judged against live in [m1-cooperative-awareness.md §2](../../Requirements/m1-cooperative-awareness.md) and in the active plan, [milestone1_high_level_plan.md](../../Plan/milestone1_high_level_plan.md).
