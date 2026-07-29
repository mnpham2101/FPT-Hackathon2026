---
name: carsky-login
description: How the car-sky agent authenticates to CarSky. Use a user-supplied API key if one exists; otherwise create one from a Keycloak login (undocumented method). Never persist, echo, log, or commit the password or key.
---

# CarSky Login (car-sky agent)

Trigger: [[car-sky]] step 1, alongside [carsky-deploy-preflight](../carsky-deploy-preflight/SKILL.md) (picks *which* credential/environment).

## Two credential types

- **Keycloak login** — email + password. Authenticates a human in a browser. Never sent to the REST API directly.
- **API key** — m2m secret, format `a8k_<prefix>_<secret>`. What `/api/v1/*` accepts, as `X-API-Key` or `Authorization: Bearer`.

Path A uses an existing API key. Path B creates one from a Keycloak login.

## Path A: use an existing API key (preferred)

```
export CS=https://hackathon-2.carsky.io
curl -H "Authorization: Bearer $CS_API_KEY" $CS/api/v1/blueprints   # 200 = key works
```

Source: a prior run, the `CS_API_KEY` env var, or Settings → Credentials in the UI. Creating one there requires Admin or Editor role.

No key? Use Path B. Prefer the user pasting an existing key over handling their password. Verify any key (pasted or new) with [carsky-credential-verify](../carsky-credential-verify/SKILL.md) — it catches display-ID-vs-secret mixups.

## Path B: create a key from a login session (no API key exists)

Undocumented — found by observing platform behavior, not from CarSky's own docs. Use only with explicit user agreement; prefer a human creating a key via the UI when Admin/Editor access is available.

Requires the user's email + password, taken interactively. Never read them from a file, log, or echo them.

Platform facts (M1; re-confirm in preflight):

- Base URL: `https://hackathon-2.carsky.io`
- Keycloak realm `hackathon02`, client `rework` — no direct password grant. `admin-cli`'s direct grant returns a token, but the CarSky API rejects it as an invalid JWT.
- Browser login is a Keycloak form; the resulting code is exchanged by an Envoy OAuth2 proxy, which sets session cookies.

Steps (one cookie jar throughout):

1. `GET /` → redirects to Keycloak's authorize page; fetch it.
2. Read the login form's `action` URL (`/auth/realms/hackathon02/login-actions/authenticate?...`; unescape HTML entities).
3. POST `username`, `password`, `credentialId=""` to that URL, following redirects, same cookie jar. Sets `BearerToken`, `IdToken`, `RefreshToken`, `KEYCLOAK_SESSION`.
4. `GET /api/user` → `200` with email + roles confirms the session.
5. `POST /internal/credentials` `{"name": "<name>"}` → `201 {clientId, key, name}`. `key` is the API credential, shown once — hand it to the user; don't write it into the repo.

Why step 5: the cookie authenticates `/internal/*` and `/api/*`, not `/api/v1/*`. The key from step 5 is what `/api/v1/*` needs.

## Cleanup

- List keys: `GET /internal/credentials`. Delete: `DELETE /internal/credentials/{clientId}`.
- Delete a temporary key after use unless the user wants it kept.
- Never commit, log, or print the password or key. Keep temp files in the scratchpad; delete when done.

## Output

A working API key (`Authorization: Bearer <API_KEY>`) plus the confirmed base URL, handed to [[car-sky]]'s deploy procedure.

## How to apply

Owned by [[car-sky]], after [carsky-deploy-preflight](../carsky-deploy-preflight/SKILL.md) confirms environment and credential. No task IDs. Full REST reference: [carsky-rest-api-blueprint.md](../../../requirements/car-sky-guide/carsky-rest-api-blueprint.md).
