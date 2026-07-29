---
name: carsky-login
description: Procedure the car-sky agent follows to authenticate to the CarSky platform — prefer a user-supplied API key (Bearer) for the REST API; when none exists, bootstrap one via the verified Keycloak login-form → Envoy session-cookie → mint-API-key flow. Never persist, echo, log, or commit the password or key.
---

# CarSky Login (car-sky agent)

Trigger: [[car-sky]] needs to authenticate before driving the platform (called from step 1 of its procedure, alongside [carsky-deploy-preflight](../carsky-deploy-preflight/SKILL.md), which decides *which* credential/environment). This skill is *how* to obtain a working credential. Verified live on 2026-07-29 against `https://hackathon-2.carsky.io`.

## Two auth paths — try A first

### Path A — API key (preferred, normal case)

The REST API (`/api/v1/*`) authenticates with `Authorization: Bearer <API_KEY>`. If the user already has a key (from a prior run, an env var like `CS_API_KEY`, or the UI's **Settings → Credentials**), use it directly — no password login needed.

```
export CS=https://hackathon-2.carsky.io
curl -H "Authorization: Bearer $CS_API_KEY" $CS/api/v1/blueprints   # 200 == key works
```

If the user has no key and wants the agent to mint one, go to Path B. Always prefer having the user paste an existing key over handling their password.

### Path B — password bootstrap (only when no API key exists)

Used to mint an API key when none is available. Requires the user's CarSky email + password — **take them interactively at run time; never read them from a committed file, never echo/log them.**

Platform facts (M1 environment; re-confirm in preflight — the org may rotate them):

- Base URL: `https://hackathon-2.carsky.io`
- Identity: Keycloak realm `hackathon02`, client `rework` (confidential + PKCE — **no direct password grant**; the `admin-cli` direct grant returns a JWT the app API rejects as `Invalid JWT`, so it is not a shortcut).
- The browser login is a Keycloak form-POST whose code is exchanged by an **Envoy OAuth2 proxy** that sets the session cookies.

Steps (all sharing one cookie jar):

1. `GET /` → follows a `302` to the Keycloak authorize page. Fetch that page.
2. Parse the login `<form action="...">` — a `/auth/realms/hackathon02/login-actions/authenticate?session_code=…&execution=…&client_id=rework&tab_id=…&client_data=…` URL (unescape HTML entities).
3. `POST` that action with `username`, `password`, `credentialId=` (form-encoded), keeping+following redirects with the cookie jar. Envoy's `/callback` exchanges the code and sets cookies `BearerToken`, `IdToken`, `RefreshToken`, `KEYCLOAK_SESSION`. The session now authenticates the Envoy-fronted `/internal/*` and `/api/*` routes.
4. Sanity check: `GET /api/user` (with the cookie jar) → `200` with your `email` and `roles` confirms the session.
5. Mint the REST key: `POST /internal/credentials` with `{"name":"<memorable-name>"}` (cookie-authenticated) → `201 {clientId, key, name}`. The `key` is the Bearer for `/api/v1/*`. **CarSky shows it once** — hand it to the user (their secret store / env var), do not write it into the repo.

Note the boundary discovered live: `/api/v1/*` is **not** behind Envoy, so the cookie alone does not authorize it — that is why Path B ends by minting a Bearer key. `/internal/*` and `/api/*` (no `v1`) *are* behind Envoy and take the cookie.

## Cleanup & hygiene

- List keys: `GET /internal/credentials`. Revoke one: `DELETE /internal/credentials/{clientId}`.
- If the agent minted a throwaway key for a single deploy, revoke it afterward unless the user asked to keep it.
- Never commit, log, or print the password or the API key. Keep any temp files holding them out of the repo (use the scratchpad) and delete them when done.

## Output

- A working `Authorization: Bearer <API_KEY>` usable against `/api/v1/*`, plus the confirmed base URL — handed back to [[car-sky]]'s deploy procedure.

## How to apply

Owned by [[car-sky]]. Run after [carsky-deploy-preflight](../carsky-deploy-preflight/SKILL.md) has pinned which environment/credential to use. Produces no task IDs. Full REST reference: [carsky-rest-api-blueprint.md](../../../requirements/car-sky-guide/carsky-rest-api-blueprint.md).
