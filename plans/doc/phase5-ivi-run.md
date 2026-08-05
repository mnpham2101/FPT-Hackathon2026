# Phase 5 IVI run record

Evidence record for the Phase 5 in-Room IVI subtasks of [phase5_minh_tasks.md](../phase5_minh_tasks.md) group 5.9. Each section carries one subtask's recorded outputs; the procedure the outputs came from is [deploy-ivi-hmi-walkthrough.md](../../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md), cited by section.

**Deviation covering every section below:** the run was performed against the team's full **m1-system-test Room** (deployment `m1_system_test-deploy`, Rework device `KIS`), which was already `Running`, rather than the mini-blueprint of `5.5.9.1`–`5.5.9.5`. The ADB route, the guest properties and the install are node facts independent of which Room hosts the node; the mini-blueprint subtasks stay open.

## `16.5.9.19` — Provenance of the three ADB tunnel inputs

Answers [§6.1](../../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#61-confirm-before-relying-on-these) items 2, 3 and 4. All three values were obtained 2026-08-05.

| Input | Provenance |
|---|---|
| `reach-backend` binary | Organizer-supplied zip, unpacked to `tools/apk uploader/reach_be/reach/` — `reach-backend.exe` (Windows, used here) beside a POSIX `reach-backend`. Git-ignored; only the folder's guide is committed |
| Gateway URL | `https://hackathon-2.carsky.io` — **the workbench base URL itself**, no separate gateway host. Read from the Rework **Local ADB** dialog: Devices → device `KIS` → the ADB widget's tab (part `ivi-adb`) → ADB SHELL panel → **Local ADB** button → *Connect from Terminal* |
| `a8k_…` token | Shown in the same *Connect from Terminal* dialog. It is **not** the CarSky API key: a distinct per-device derived value in single-segment `a8k_<value>` form, against the API key's `a8k_<prefix>_<secret>` form. Stored at `secrets/reach-adb-token-ivi.txt` (git-ignored); the value is not written into the repository. A redeploy may mint a new token — re-open the dialog after redeploying |

The click path to the dialog, step by step, is in [tools/apk uploader/README.md](../../tools/apk%20uploader/README.md) step 1.

