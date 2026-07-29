---
name: carsky-credential-verify
description: Check whether a CarSky API credential is valid before car-sky uses it to build, push, or deploy. Confirms the key format, explains the two distinct 401 error messages, and prevents the common mistake of using a credential's display ID instead of its secret.
---

# CarSky Credential Verify

Trigger: [[carsky-deploy-preflight]] is resolving input #3 ("which credential"), or a user provides a credential string and asks whether it works. Run this check before any build, push, or deploy action that depends on the credential. A wrong or malformed credential should fail here, not partway through a deployment.

## 1. Credential format

CarSky's REST API requires an API key on every request.

- The key format is `a8k_<prefix>_<secret>`.
- Send it as `X-API-Key: <key>` or `Authorization: Bearer <key>`. Both headers work.
- The key is created once, in the UI: **Settings → Credentials → New credential**. It is displayed only at the moment of creation.

## 2. Common mistake: display ID instead of secret

After a credential is created, the Credentials list view shows an identifier in this format:

```
m2m-<uuid>-<credential-name>
```

This identifier is not the secret and cannot be used to authenticate. Sending it as a credential returns:

```
{"error":"UNAUTHORIZED","message":"Unrecognized credential format"}
```

If a user provides an `m2m-...` string, explain that it is not usable and ask for the `a8k_...` secret instead. If the secret was not copied at creation time, it cannot be recovered — the only fix is to delete that credential and create a new one.

## 3. A Keycloak login is a different kind of credential

A CarSky account login (email and password) uses Keycloak, a separate authentication system from the API key.

- A Keycloak login session is not, by itself, a valid API credential.
- Sending a Keycloak session value as `Authorization: Bearer <value>` returns the same `"Unrecognized credential format"` error described above.
- A Keycloak login session can be used to create a new API key without opening the UI. That procedure is documented in [carsky-login](../carsky-login/SKILL.md) — do not repeat it here. This skill only checks a credential once one already exists.

## 4. Verification steps

1. Ask the user for the candidate credential string. Do not guess a value or reuse one from a previous session — a wrong or expired key can authenticate against the wrong environment, or fail outright.
2. Check the format: does the string start with `a8k_`? If not (for example, it starts with `m2m-`, or looks like a Keycloak token), stop and explain why using section 2 or 3 above. Do not spend a network call testing an obviously wrong format.
3. Send one request to confirm the credential: `GET /api/v1/blueprints` with the credential in the `Authorization` header.
4. Read the result using this table:

| Response | Meaning | Next action |
|---|---|---|
| `200` with a JSON list | The credential is valid | Proceed — use it as the confirmed credential |
| `401`, message `"Missing credentials"` | No credential was sent in the request | Fix the request; this is not a problem with the credential itself |
| `401`, message `"Unrecognized credential format"` | A credential was sent, but it is not a valid `a8k_...` key | Explain the mistake in section 2; ask for the correct secret |
| `401` or `403` with any other message | The credential has the correct format but was rejected (for example: revoked, expired, or wrong environment) | Ask the user to confirm the environment, or to create a new credential |

5. Do not log, print in full, commit, or store the credential string. Report only the result — valid or invalid, and the reason if invalid.

## Output

- A clear result: the credential is valid, or it is not, with the specific reason.
- If valid, hand the credential back to [[carsky-deploy-preflight]] as the confirmed value for input #3.

## How to apply

Called by [[carsky-deploy-preflight]] when resolving "which credential," and directly by [[car-sky]] whenever a user provides a credential outside of a full deployment request. Produces no task IDs — this is a verification step, not implementation work.
