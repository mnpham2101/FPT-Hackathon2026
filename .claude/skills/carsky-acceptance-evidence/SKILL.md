---
name: carsky-acceptance-evidence
description: Procedure the car-sky agent follows when asked to investigate a blueprint's deployment status, to build and test code on the platform, or to produce the evidence that closes a task/subtask — walk CI config, commit gates, build verification, registry check, and deployment status in order. Use whenever an acceptance criterion depends on something actually running on CarSky.
---

# CarSky Acceptance Evidence (car-sky)

Trigger: [[car-sky]] is asked to **investigate a blueprint's deployment status**, to **build and test code** on the platform, or to **provide evidence for acceptance / closing a task or subtask**. The steps run in order — each one's output is the next one's input, and a failure stops the walk rather than being worked around.

Endpoint catalog and auth live in [carsky-rest-api-blueprint.md](../../../requirements/car-sky-guide/carsky-rest-api-blueprint.md); runtime inspection lives in [carsky-room-diagnostics](../carsky-room-diagnostics/SKILL.md). This skill is the ordering and the gates, not a second endpoint list.

## 1. Check the CI workflow can build the image

Read [.github/workflows/phase0-ci.yml](../../../.github/workflows/phase0-ci.yml) and confirm the job that builds the target image is actually capable of producing it:

- the job exists for this node's image and its build context points at the right folder;
- **`PLATFORMS` names exactly one platform** — the project requires single-platform `linux/arm64` images ([phase0-smoke-test-run.md § Standing requirement](../../../plans/doc/phase0-smoke-test-run.md)); a multi-platform list is a defect to fix here, not at deploy time;
- `--provenance=false --sbom=false` are present, or attestations silently turn the result back into a multi-entry index;
- the push step's `REGISTRY_HOST`, image tag, and credential secret are set, and the secret exists in the repository.

If the workflow cannot build the image, say so and stop — later steps have nothing to verify.

## 2. Ask permission before committing the workflow

If step 1 required a change to the `.yml`, **request explicit user permission to commit it** before running `git commit`. State what changed and why in one or two lines. Do not bundle unrelated files into that commit.

## 3. Ask permission before committing and pushing code changes

Same gate for any source change: **request explicit permission to commit and to push.** Treat commit and push as two separate permissions — a user who approved a commit has not thereby approved a push.

Say plainly that **nothing builds until the push lands**: the workflow triggers `on: push`, so an uncommitted or unpushed change means the registry still holds the previous image. Commit messages follow [task-planning-conventions.md § Commit message format](../../rules/task-planning-conventions.md#commit-message-format).

## 4. Check the build succeeded

The `gh` CLI is not installed on the dev host. Verify from the registry, which is the outcome that matters anyway — the tag's `LastUpdated` must move to after the push, and the platform list must be the single expected architecture:

```
curl -s -u <user>:<zak_key> -G https://registry.hackathon-2.carsky.io/v2/_zot/ext/search \
  --data-urlencode '{RepoListWithNewestImage{Results{Name LastUpdated DownloadCount Platforms{Os Arch}}}}'
```

An unchanged `LastUpdated` means the build did not run or did not push — check the Actions run before going further. Ask the user to read the Actions page when the registry alone cannot distinguish "build failed" from "push step skipped".

## 5. Request credentials for CarSky and Zot

Ask the user for the two credentials this step needs; they are different keys and are not interchangeable:

| Credential | Format | Used for |
|---|---|---|
| CarSky API key | `a8k_…` | REST API — blueprints, deployments, logs |
| Zot API key | `zak_…` | registry reads with the registry account as username |

Validate format before use per [carsky-credential-verify](../carsky-credential-verify/SKILL.md); obtain one per [carsky-login](../carsky-login/SKILL.md) when none exists. **Never echo, log, persist, or commit either key**, and never paste one into a file that git tracks.

Then confirm the newly built image is present and is the artifact the blueprint will pull — repository in `/v2/_catalog`, tag in `/v2/<repo>/tags/list`, and the manifest resolving to a single-platform image rather than an index.

## 6. Check deployment status; if not deployed, ask the user to deploy

Resolve the Room and read its status per [carsky-room-diagnostics](../carsky-room-diagnostics/SKILL.md) (status → per-node phases → per-container logs).

**If the blueprint is not deployed, ask the user to deploy it — do not deploy it unilaterally.** Deploying consumes a concurrent-deployment slot and may require the manual Nydus-UI ethernet wiring that REST cannot create. Tell the user which blueprint to deploy and onto which device, then wait.

## 7. Re-check deployment status and collect the evidence

After the deploy, re-read status and node phases, and gather what the acceptance criterion actually asks for: node phases, the relevant `user`-container log lines, and any criterion-specific observable named in the task.

Report evidence and verdict together — quote the decisive log line or phase rather than asserting a pass, and name any criterion met only by a weaker fallback method. A subtask closes only when its stated acceptance holds and its commit exists ([task-planning-conventions.md § Subtask discipline](../../rules/task-planning-conventions.md#subtask-discipline-non-negotiable)); this skill supplies the evidence, [[project-planner]] marks the subtask done.

## Output

- A pass/fail verdict per acceptance criterion, each with the quoted evidence behind it.
- The image actually deployed: tag, `LastUpdated`, platform.
- Anything still outstanding — an unpushed commit, a failed build, an undeployed blueprint, a criterion met only by fallback.

## How to apply

Owned by [[car-sky]]. Assigns no task IDs and marks no subtask done — it produces the evidence that lets [[project-planner]] do so.

**[[project-architecture]] and [[project-planner]] hand this work to [[car-sky]] rather than doing it themselves** whenever a task is to check a deployment's status, build/test code on the platform, or produce acceptance evidence — spawn [[car-sky]] with the blueprint, the node/image, and the acceptance criteria to satisfy. The spawning agent keeps ownership of the task ID and the done-marking.
