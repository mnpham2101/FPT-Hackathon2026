# Zot Registry API Key — Get & Use

The CarSky container registry runs [Zot](https://registry.hackathon-2.carsky.io/). Its API key (format `zak_...`) is the **password for `docker login`** — required for every image push in this project: the netcheck smoke-test image (steps M2–M4 of [baseline-connectivity-smoke-test.md](../../documents/Delivery/Test-Guides/baseline-connectivity-smoke-test.md)), the three team ECU images ([carsky-4-node-blueprint.md §4](carsky-4-node-blueprint.md#4-steps) step 1), and the CI push job (§ CI secret below). Source: the platform doc's "Log In to Zot Registry & Get API Key" module in [Car-Sky-Platform.html](../development-platform-doc/Car-Sky-Platform.html).

## Not to be confused with

- **CarSky REST API key** (`a8k_...`, Settings → Credentials) — authenticates the blueprint/deployment REST API, not the registry. Formats and failure modes: [carsky-credential-verify](../../.claude/skills/carsky-credential-verify/SKILL.md).
- **Keycloak login** (CarSky username + password) — your interactive login; it *creates* the Zot key but is never used as a docker password.

## Get the key

1. Open the registry UI: click **Registry** on the CarSky Dock, or browse to `https://registry.hackathon-2.carsky.io/` directly — that is the host serving Zot, not the `registry.carsky.io` the platform doc names (§ Registry host caveat below).
2. Click **Sign in with A8 Keycloak**; log in with your CarSky username/password if prompted.
3. Top-right **user icon** → **API Keys** → **Create new API key** → review name/expiry → **Create**.
4. **Copy the `zak_...` string immediately** — Zot shows it exactly once; after the dialog closes it cannot be viewed again. Lost key = revoke it and create a new one. No **API Keys** menu = your account lacks registry access — ask the organizers.

## Registry host caveat (open item O1)

**Resolved — the registry host is `registry.hackathon-2.carsky.io`.** (Section title kept for the links that cite it.) The platform doc names `registry.carsky.io`; that host returns **502** and serves nothing (probed 2026-07-30, [smoke-test note § Open items](../../documents/Delivery/Test-Guides/baseline-connectivity-smoke-test.md#9-open-items)). A 502 is an origin-level failure, so it covers the Zot web UI and the `/v2/` Docker endpoint alike; there is no split where one works and the other does not.

The working host is established by a full push → pull → deploy loop, not merely by a probe answering: [phase0-smoke-test-run.md](../../plans/doc/deprecated/phase0-smoke-test-run.md) (2026-07-31) records the netcheck image pushed to `registry.hackathon-2.carsky.io` and three Container nodes running that exact tag in a live Room, and [phase1-comms-run.md](../../plans/doc/deprecated/phase1-comms-run.md) records both ECU lanes pushing there from CI. Every live operational config in this repo — the node guides, the blueprint `image` fields, CI, and the deploy agent/preflight instructions — names that host; none still names the 502 one.

**Carry the rule, not this round's value.** The registry host mirrors the round's *environment* host, exactly as the REST base URL `https://hackathon-2.carsky.io` does. `registry.carsky.io` remains a true statement about the vendor's generic documentation rather than an error in it, and the organizers may rotate environment hosts between rounds — so next round, re-derive the registry host from the environment instead of assuming `hackathon-2`.

Whichever host is current, use **the same one** consistently in `docker login`, image tags, blueprint `image` fields, and CI — a mismatched host is the "push succeeds but image missing" / `ImagePullBackOff` failure mode.

## Use the key

Local docker (password prompt = paste the `zak_...` key):

```
docker login <registry-host> -u <your-carsky-username>
docker tag my-image:latest <registry-host>/my-image:latest
docker push <registry-host>/my-image:latest
```

### CI secret (`CARSKY_ZOT_API_KEY`)

The Phase 0 CI workflow builds images on GitHub Actions (no Docker on the dev machine) and needs the key to push:

1. GitHub → `mnpham2101/FPT-Hackathon2026` → **Settings → Secrets and variables → Actions → New repository secret**.
2. Name: `CARSKY_ZOT_API_KEY` · Value: the `zak_...` string. The docker-login username is not secret and lives in the workflow as a variable.
3. The CI push job stays disabled until this secret exists — build-only jobs run regardless.

## Security rules

- The key is displayed once and stored nowhere retrievable — treat the copy in GitHub Secrets as the only copy.
- Never commit, echo, log, or paste the key into chat, code, or markdown (same rule as [carsky-login](../../.claude/skills/carsky-login/SKILL.md)).
- Revoke keys you no longer use (Zot → API Keys → **Revoke**).

## Troubleshooting

| Symptom | Fix |
|---|---|
| `401 Unauthorized` on `docker login` | Key wrong or expired — create a new one |
| Registry panel in CarSky UI shows auth error | Log in at the registry URL directly, then refresh the panel |
| No **API Keys** menu in Zot | Account lacks registry access — contact an admin |
| Push succeeds but image not visible / `ImagePullBackOff` on deploy | Registry-host mismatch — see § Registry host caveat |
