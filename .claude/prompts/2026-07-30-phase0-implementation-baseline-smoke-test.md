# Prompt — Phase 0 implementation with baseline-blueprint smoke test (trial2_minh)

**Date:** 2026-07-30 **Requested by:** mnpham1986@gmail.com

## Prompt text

> you are project-architecture, work on implementation of phase 0, invoke necessary agent to do so.
>
> ensure that smoke test on baseline blueprint is planned and done on phase 0. On Car Sky , it is saved as "trial2_minh". Refer to research document for smoke test. Ensure the code are save in correct location per architectural guideline.
>
> Save this prompt

## Outcome

- Mid-run correction (2026-07-30) reshaped the flow: project-architecture must source `doc/research_notes/`, designate every deliverable's file location, may pause for approval, and must end every HLD run by invoking project-planner — codified in `0cfa90d` and refined by an adversarial review (13 confirmed findings) in `20fc336`.
- Phase 0 HLD committed `d807c37` (plans/doc/phase0-contract-freeze-hld.md + call-flow .puml): contracts source of truth at top-level `contracts/` (D1, user-approved 2026-07-30), per-node byte-synced copies + `check_sync.py`, codec seam `ICpmCodec` over Vanetza r2 CPM with golden-vector tool in V2X_ECU/, R4 additive-version test both ends; smoke test adopted wholesale by reference from plans/doc/research_notes/baseline-connectivity-smoke-test.md with `tools/netcheck/` as the user-endorsed location.
- Amendment `70796c0` (user requirement 2026-07-31): container-node start ⇒ test scripts self-run via entrypoint ⇒ C1–C5 observable in View Log from a Room deploy alone.
- Planner handoff delivered: `5e0f862` plans/phase0_tasks.md — 8 groups, 26 subtasks (23 agent, 1 car-sky, 2 user-manual M5–M12 on blueprint `trial2_minh`), branch `feat/phase0-contract-freeze`; phase5 overlap annotated in `c9885d9`.
- User decisions: branch created; Linux verification via GitHub Actions CI (no Docker/WSL on host); CI registry push pending the Zot API key as a repo secret (user manual step).
