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

## M5–M9 — blueprint config + deploy (USER-MANUAL, subtask `5.0.8.3`)

Pending. Per-node config values and the M5–M8 readiness assessment: [phase0-trial2-minh-preflight.md](phase0-trial2-minh-preflight.md). Blueprint `trial2_minh` is already fully wired (M6 requires nothing) and validates.

**Self-run acceptance:** deploying alone must start the programs — if any node needs a manual exec to produce logs, the run fails (HLD §6 startup guarantee).

## M10 — pass criteria (USER-MANUAL, subtask `6.0.8.4`)

Pending. C1–C5 per the note §2; IVI hop (hop 3) per the note §7 — record which option was used.
