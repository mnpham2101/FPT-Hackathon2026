# Phase 0 — Baseline Connectivity Smoke Test: Run Record

Evidence log for task group 0.8 ([phase0_tasks.md](../phase0_tasks.md)), executing [baseline-connectivity-smoke-test.md](research_notes/baseline-connectivity-smoke-test.md). Steps are recorded as they complete; pass criteria C1–C5 are the note's.

## Standing requirement — container images must be single-platform arm64

**Every image a Container node pulls must be built for `linux/arm64` alone.** A multi-platform manifest index is not accepted — the node stays in `Provisioning` with `waiting to start: trying and failing to pull image`.

This governs every container node in the project (`Scenario_Player`, `V2X_ECU`, `ADA_ECU`), not just netcheck. In CI that means one platform in the buildx invocation:

```yaml
PLATFORMS: linux/arm64
```

```
docker buildx build --platform linux/arm64 --provenance=false --sbom=false \
  -t <host>/<image>:<tag> --push <context>/
```

Keep `--provenance=false --sbom=false`: buildx otherwise attaches attestation entries that turn the result back into a multi-entry index.

**Verify before deploying** — the tag must resolve to a single-platform image, not an index:

```
curl -u <user>:<zak_key> https://registry.hackathon-2.carsky.io/v2/<repo>/manifests/<tag>
```

The Zot UI shows the same thing: one architecture chip on the repository card, not `amd64  arm64`.

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
| Platform | `linux/arm64`, single-platform (commit `5e75920`) |

`registry.carsky.io` returns 502; `registry.hackathon-2.carsky.io` serves the registry (`GET /v2/` → 401 challenge, → 200 with the key) and is the host for both push and blueprint node `image` fields.

## M5–M9 — blueprint config + deploy ✅ 2026-07-31

Deployed as `trial2_minh_netcheck` on room `27gs83k3oeju2mbywu1j8`, namespace `room-12tviahc`, blueprint snapshot `mZPNS7C8VA-cD1Ethjebq`. All three container nodes carry `registry.hackathon-2.carsky.io/m1-netcheck:latest`, `command: ["./entrypoint.sh"]`, `capabilities: ["NET_RAW"]`.

**Room `RUNNING`, 5/5 nodes `Running`**, stable across a 10-minute observation window with no restarts. Per-node config values and the M5–M8 readiness assessment: [phase0-trial2-minh-preflight.md](phase0-trial2-minh-preflight.md).

Deploying alone started the programs — no manual exec was needed, satisfying the HLD §6 startup guarantee.

## M10 — pass criteria ✅ 2026-07-31

| Criterion | Evidence |
|---|---|
| C1 all `Running` | 5/5 nodes; every pod reports `sidecar=running user=running` |
| C2 no `[ERR]` | 0 error lines across bench / V2X / ADA |
| C3 live log per node | 100 lines each, streaming |
| C4 wire capture | 80 / 66 / 66 `[CAP]` lines, `e-eth In/Out` on `10.99.0.x` |
| C5 accumulated stamps | ADA logs `body=seq=288\|bench\|v2x` |

```
bench  [TX] #285 to 10.99.0.11:47100          len=13
v2x    [RX] #288 from 10.99.0.10  body=seq=287|bench
       [TX] #288 relayed to 10.99.0.12:47200  len=17
ada    [RX] #289 from 10.99.0.11  body=seq=288|bench|v2x
       [TX] #289 relayed to 10.99.0.13:47300  len=21
```

**Hop 3 (IVI) used the note §7 option 2 (fallback), not option 1.** ADA's `[TX] … relayed to 10.99.0.13:47300` plus its `[CAP]` line prove the datagram reached the wire; the Skycraft node has no listener, so receipt is unconfirmed. Option 1 (ADB `nc -u -l -p 47300`) remains available if a confirmed hop-3 receipt is wanted.

## Open items

- **Platform tenancy gap** — `GET /api/v1/blueprints` returns all blueprints across every owner unfiltered, and `GET /api/v1/blueprints/{id}` returns another owner's `"visibility": "PRIVATE"` blueprint in full, including node `config`, `env`, and inline script source. Rival teams' designs and images are readable by any participant. Report to BTC; unrelated to M1 delivery.
- **Unreliable REST routes** (2026-07-31): `POST /deployments/{roomId}/restart[/{node}]` returns 500 `INTERNAL_ERROR`; `container-exec` returns 503 `Conduit service not configured`. Prefer teardown + redeploy. Recorded in [carsky-rest-api-blueprint.md](../../requirements/car-sky-guide/carsky-rest-api-blueprint.md).
- Propagate the single-platform arm64 requirement into [deploy-walkthrough-netcheck.md](../../requirements/car-sky-guide/deploy-walkthrough-netcheck.md) §M7 and its §5 quick reference, and into the three ECU node guides before their images are built.
