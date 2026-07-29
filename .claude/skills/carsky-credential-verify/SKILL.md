---
name: carsky-credential-verify
description: Verify a candidate CarSky API credential is actually usable before [[car-sky]] builds/pushes/deploys anything. Confirms the real key format (a8k_<prefix>_<secret>), decodes the platform's two distinct 401 messages, and catches the credential-ID-vs-secret mixup that looks like a working key but isn't. Run whenever a user supplies a credential, or asks to test/verify CarSky API access.
---

# CarSky Credential Verify

Trigger: [[carsky-deploy-preflight]] is resolving input #3 ("which credential"), or the user hands over a credential string and asks to confirm it works. Run this **before** any build/push/deploy action that depends on the credential — a wrong or malformed credential should fail here, cheaply, not mid-deploy.

## Verified credential format (live, 2026-07-29, `hackathon-2.carsky.io`)

`GET /api/v1/openapi.json` (unauthenticated) declares the real scheme — this is authoritative over any narrower assumption in other docs:

```json
"ApiKeyAuth": {
  "type": "apiKey", "in": "header", "name": "X-API-Key",
  "description": "Paste your API key: `a8k_<prefix>_<secret>`. Also accepts `Authorization: Bearer a8k_...` header."
}
```

- The real secret always starts with `a8k_`. Either header works: `X-API-Key: a8k_...` or `Authorization: Bearer a8k_...`.
- It is minted once, in the UI: **Settings (⚙) → Credentials → New credential** — shown **only at creation time**.

## The gotcha that will fool you (confirmed live)

The Settings → Credentials **list view** (after creation) shows an identifier styled like:

```
m2m-62a5d873-6dcb-4e97-a046-f1defb05a9b0-claude-trial1-minh    (id, for a credential named "claude-trial1-minh")
```

This `m2m-<uuid>-<credential-name>` string is the credential's **display ID**, not the secret — it is never accepted as a credential. A live test against it returned:

```
{"error":"UNAUTHORIZED","message":"Unrecognized credential format"}
```

The same error, for the same reason, comes back if someone pastes a Keycloak session artifact instead of a real key (see next section). **If a user hands you an `m2m-...` string, it is not usable — tell them so and ask for the `a8k_...` secret instead.** If the secret was never copied down at creation time, it cannot be recovered; the only fix is deleting that credential and creating a new one, copying the `a8k_...` value this time.

## A raw Keycloak login session is not a substitute — but it can bootstrap one

Confirmed live (2026-07-29), first pass: a fully successful username/password login against this platform's Keycloak realm (`hackathon02`, client `rework`, Authorization Code + PKCE) does **not by itself** yield a usable API credential — direct password-grant to `rework` is rejected outright (`unauthorized_client`, Direct Access Grants disabled), and the resulting session is stored server-side as opaque, non-JWT, HttpOnly cookies (`BearerToken`, `IdToken`, `RefreshToken` — zero `.` separators). Pasting any of those cookie values directly as `Authorization: Bearer <value>` gives the same `"Unrecognized credential format"` error as the gotcha above.

**Correction from a later session with more context:** that login session isn't a dead end — it authorizes a *different* route family. `/api/v1/*` (the REST API) needs the real `a8k_...` key, but the same cookies **do** authorize `/internal/*` and `/api/*` (no `v1`), which are behind an Envoy OAuth2 proxy. `POST /internal/credentials` on that cookie session actually **mints** a fresh `a8k_...` key. That's the full, verified bootstrap procedure — see [carsky-login](../carsky-login/SKILL.md) for the exact steps; don't re-derive it here. This skill's job is narrower: once you have a candidate key (user-supplied or freshly minted via carsky-login), verify its shape and diagnose why it's rejected if it is.

## Verify procedure

1. Get the candidate string from the user (never guess or reuse one from history/memory — a stale or wrong-environment key deploys to the wrong place or fails auth).
2. Sanity-check the shape first: does it start with `a8k_`? If not (e.g. `m2m-...`, a raw JWT, a cookie value), stop and explain why per the gotcha above — don't spend a network call on an obviously-wrong shape.
3. Spend exactly one cheap read call to confirm: `GET /api/v1/blueprints` (or `?name=<filter>` if a specific blueprint is already known) with the credential in `Authorization: Bearer <key>`.
4. Decode the response:

| Response | Meaning | Next action |
|---|---|---|
| `200` + a JSON list | Credential is valid and live | Proceed — pin it as input #3 |
| `401 {"message":"Missing credentials"}` | No header was actually sent (a client/script bug, not the user's key) | Fix the request, not the key |
| `401 {"message":"Unrecognized credential format"}` | A header was sent but isn't a real `a8k_...` key (ID pasted instead of secret, Keycloak token, cookie, typo) | Explain the gotcha above; ask for the real secret |
| `401`/`403` with any other message | Key is shaped right but rejected (revoked, wrong environment/realm, expired) | Ask the user to confirm the environment (`hackathon-2.carsky.io` may rotate between rounds) or mint a fresh credential |

5. Never log, echo back in full, commit, or persist the credential string itself — only report the verify **outcome** (valid / invalid + reason).

## Output

- A go/no-go on the credential, plus the specific reason on no-go (from the table above) so the user knows exactly what to fix.
- On go: hand the confirmed credential back to [[carsky-deploy-preflight]] as input #3.

## How to apply

Called by [[carsky-deploy-preflight]] when resolving "which credential," and directly by [[car-sky]] whenever a user hands over a credential ad hoc (e.g. mid-conversation, not as part of a full deploy). Produces no task IDs — it's a diagnostic gate, not implementation work.
