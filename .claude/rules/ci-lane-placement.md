# CI Lane Placement

Which `.github/workflows/phase<N>-ci.yml` holds each CI job. Authoritative for the CI table in every node HLD's §11, and for any subtask brief that adds, moves or edits a lane.

A **lane** is one job in a GitHub Actions workflow. Every workflow file in this repo carries identical `on:` triggers — `push` and `pull_request` to `main` — so **every lane runs on every push regardless of which file holds it**. Placement decides where a lane is *maintained*, never whether it executes. A lane is therefore never duplicated into a second file to make it run.

## The rule: a lane lives with the node it exercises

**A lane belongs to the workflow file of the phase that develops the node the lane exercises** — not the phase that happened to create the lane, and not the phase that last edited it.

Phase 0 scaffolded all four nodes, so filing by creation date collects every node's build and test lane in `phase0-ci.yml`. A reader looking for the IVI test lane then has to open the file named for the smoke-test phase. Filing by node means the file named for a phase contains that phase's work.

`phase0-ci.yml` keeps only Phase 0's **own** artifacts: the R1–R6 contract gate and the netcheck smoke-test image. Neither exercises a node — the gate spans all four, and netcheck is test equipment retired after the smoke test.

## Designated layout

| File | Lanes | What the file is |
|---|---|---|
| `phase0-ci.yml` | `contracts-gate` · `netcheck-image` | Phase 0's own artifacts: the frozen-contract gate and the smoke-test image |
| `phase1-ci.yml` | `v2x-core-build` · `v2x-comms-check` · `v2x-ecu-image` · `sp-unit-tests` · `sp-codec-helper` · `scenario-player-image` | The V2X ECU and the bench |
| `phase2-ci.yml` | `ada-core-build` · `ada-loopback-check` | The ADA scaffold |
| `phase3-ci.yml` | `ada-detector-wheels` · `ada-detector-tests` · `ada-detector-run` · `ada-zero-c` | The ADA detector |
| `phase4-ci.yml` | `ada-e2e-loopback` · `ada-ecu-image` · `ada-bench-image` · `ada-bench-selfcheck` | ADA fusion, the node image and its bench |
| `phase5-ci.yml` | `ivi-unit-tests` · `ivi-assemble` · `r4-sim-image` | The IVI ECU and its ADA simulator |

The ADA node spans three files because it is developed across three phases; each lane sits with the phase whose work it verifies. A node image lane sits with the phase that **completes** the node, since the image carries every phase's contribution — which is why `ada-ecu-image` is Phase 4's rather than Phase 2's, where the lane was written.

## Job names are stable

**Moving a lane never renames its job.** Acceptance records across `plans/` cite CI runs by job name (`Closed: CI run 30697863324 green (v2x-core-build)`), and a rename orphans every one of them. The job name is the lane's identity; the file is only its address.

The one case that forces a name change is a job **split** across two files, where one name cannot describe both halves. Then the old name is retired and both halves are named for what they now run.

## Moving a misplaced lane

1. Move the job block verbatim, with its comments, its `env:` and any `actions/cache` entry it owns.
2. Leave the job name unchanged (§ Job names are stable).
3. Update every document that names the lane **together with its file** — the node HLD §11 CI table, the plan briefs whose write scope or acceptance names it, and any guide that cites it. A document naming the lane alone needs no edit.
4. Leave historical `**Status:**` and `Closed: CI run …` lines untouched — they record what happened, not where the lane lives.
5. One commit per lane moved, `[<taskID>] chore: …`. Re-filing several lanes at once, as one application of this rule, is a single commit.

A lane sharing an `actions/cache` key with a lane in another file is a coupling the move should dissolve, not carry: co-locating both in one file makes a cache-key change visible where it is made.

## How to apply

- [[project-architecture]] writes the §11 CI table of a node HLD from the designated layout above, and files any lane it configures under [CLAUDE.md § Repository layout](../../CLAUDE.md) node build rules into that node's phase file.
- [[project-planner]] names the target file in every subtask brief that adds or edits a lane, taking it from this document rather than from the phase the subtask sits in. A brief that would put a lane in a file this rule assigns elsewhere is flagged back, not written.
- Implementation subagents write the lane into the file their brief names, and never create a second workflow file for a lane that already has a designated home.
