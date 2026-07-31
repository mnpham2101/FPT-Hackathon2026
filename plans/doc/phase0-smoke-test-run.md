# Phase 0 — Baseline Connectivity Smoke Test: Run Record

Evidence log for task group 0.8 ([phase0_tasks.md](../phase0_tasks.md)), executing [baseline-connectivity-smoke-test.md](research_notes/baseline-connectivity-smoke-test.md). Steps are recorded as they complete; pass criteria C1–C5 are the note's.

## M1 — netcheck tool authored

Subtask `6.0.8.1`, commit `c17b488`: `tools/netcheck/` (Dockerfile, entrypoint.sh, capture.sh, netcheck.py) verbatim from the note §4.2–§4.5.

## M2–M4 — registry login, build, push ✅ 2026-07-31

Executed by GitHub Actions (the dev host has no Docker), job `netcheck-image` in [.github/workflows/phase0-ci.yml](../../.github/workflows/phase0-ci.yml).

| Item | Value |
|---|---|
| **Registry host (O1 answer)** | `registry.hackathon-2.carsky.io` — **closes open item O1** |
| Registry account | `kis@hackathon.fpt.com` (workflow default; `CARSKY_REGISTRY_USER` variable overrides) |
| Credential | `CARSKY_ZOT_API_KEY` GitHub Actions secret (`zak_…`, never committed) |
| Pushed tag | `registry.hackathon-2.carsky.io/m1-netcheck:latest` |
| Manifest digest | `sha256:e3283d45c1df91b0698c6ed78867144d7d1d022191b42bc17d6c2f380fb5e557` |
| Image size | 5 layers, 19 121 467 bytes |

**O1 resolved:** `registry.carsky.io` returns 502; `registry.hackathon-2.carsky.io` serves the registry (`GET /v2/` → 401 challenge `Basic realm="Authorization Required"`, → 200 with the key). Every image reference — CI, blueprint node configs, M7 — must use the hackathon-2 host.

**Verification (registry API, 2026-07-31):** `/v2/_catalog` → `{"repositories":["m1-netcheck","tanbd2/dummy-video-color","vitalguard/vital-guard-ai-dms"]}`; `/v2/m1-netcheck/tags/list` → `{"name":"m1-netcheck","tags":["latest"]}`. The repository was absent before the push and present after.

## Open blocker — the image pull reference (found 2026-07-31)

Two deploy attempts failed with the containers stuck in `Provisioning` and Kubernetes reporting `waiting to start: trying and failing to pull image`, while the bridge and IVI reached `Running`.

- **Attempt 1** — node configs still carried the baseline ECU images (`registry.carsky.io/m1-v2x-ecu:latest` …), which do not exist. Cause: M7 not yet applied.
- **Attempt 2** — M7 applied and verified correct over REST (`registry.hackathon-2.carsky.io/m1-netcheck:latest`, `./entrypoint.sh`, `NET_RAW`), yet the pull still failed.

Evidence gathered:

- The image genuinely exists — manifest `sha256:e3283d45…` readable with the Zot credential.
- Anonymous pull is refused (401) for **every** repository, ours and other teams' alike, so the cluster must pull with a credential we do not control.
- The one known-working platform blueprint ([blueprint-KIS.json](../../requirements/development-platform-doc/blueprint-KIS.json)) references images as **`localhost:5000/<namespace>/<image>:tag`**, not by public hostname; every other repository in the registry is namespaced `<team>/<image>` while ours sits at the root.

**Hypothesis:** the address used to *push* (external ingress `registry.hackathon-2.carsky.io`) is not the address a node must use to *pull*. Remaining candidate: `registry.carsky.io/m1-netcheck:latest` (the platform doc's own convention and the baseline's; its 502 is on the external ingress and may not affect in-cluster resolution). Eliminated: `localhost:5000/m1-netcheck:latest` — tested live 2026-07-31 on the V2X node (deploy `dn7lg2xt8m6hdqr7ce-uz`, hackathon-2 refs on the other two nodes as controls), identical pull failure; the KIS blueprint's `localhost:5000` references presumably resolve only for platform-mirrored images. A namespaced path alone (`kis/m1-netcheck` via the public host) is **weakened** by the cross-team evidence below — Vital-Guard's image is namespaced and fails identically.

**Eliminated live (2026-07-31, attempts 3–4):**

- **Architecture** — re-pushed as a multi-arch amd64+arm64 manifest list (CI `896bc7a`); fresh pods (restart recreated them) still failed.
- **Buildx attestations** — re-pushed with `--provenance=false --sbom=false` (CI `e55f0ec`), verified a clean two-entry index in the registry; still failed.
- **Ours-only problem** — Vital-Guard's `DMS AI Engine` node fails with the byte-identical `trying and failing to pull image` on their own namespaced image, while every non-registry node type (script-node, can-bus, skycraft, eth-bridge, kuksa) runs fine platform-wide. No container node demonstrably pulling from this Zot registry has been observed running.

Also found while diagnosing: `POST /deployments/{roomId}/restart[/{node}]` returns 500 `INTERNAL_ERROR` (though pods were observed recreated afterwards) and `container-exec` returns `Conduit service not configured` — both platform-side gaps, recorded in [carsky-rest-api-blueprint.md](../../requirements/car-sky-guide/carsky-rest-api-blueprint.md).

**Action:** (a) cheap UI test — point one node's image at `localhost:5000/m1-netcheck:latest`, then `registry.carsky.io/m1-netcheck:latest`, redeploying between; (b) escalate to the BTC organizers with the evidence above (image present + multi-arch, anonymous pull 401, second team failing identically) asking how Rooms authenticate pulls and which reference blueprints must use. Correct [deploy-walkthrough-netcheck.md](../../requirements/car-sky-guide/deploy-walkthrough-netcheck.md) §M7 and this file once known.

## M5–M9 — blueprint config + deploy (USER-MANUAL, subtask `5.0.8.3`)

In progress — blocked by the pull reference above. Per-node config values and the M5–M8 readiness assessment: [phase0-trial2-minh-preflight.md](phase0-trial2-minh-preflight.md). Blueprint `trial2_minh` is already fully wired (M6 requires nothing) and validates.

**Self-run acceptance:** deploying alone must start the programs — if any node needs a manual exec to produce logs, the run fails (HLD §6 startup guarantee).

## M10 — pass criteria (USER-MANUAL, subtask `6.0.8.4`)

Pending. C1–C5 per the note §2; IVI hop (hop 3) per the note §7 — record which option was used.
