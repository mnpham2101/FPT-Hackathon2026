---
name: delivery-judgement
description: Procedure delivery-judge follows to score the outgoing Hackathon-Delivery packet as the Round 2 judging panel would — bundle-only inspection, first-hand verification of the packet's external links and artifacts, rubric scoring with L0–L3 anchors, innovation/risk/rival weighing, and a versioned report plus deck in tmp/. Use whenever the user asks to judge, review, or score the delivery package.
---

# Delivery Judgement

Trigger: the user asks how the delivery would be judged, to "do another judge", or to review `Package-Delivery-tool/Hackathon-Round2-Delivery` from the judge's seat. The output format is fixed by [delivery-judgement-format.md](../../rules/delivery-judgement-format.md); this skill is the procedure that fills it.

## 1 · Establish the inputs

- **The judged object**: `Package-Delivery-tool/Hackathon-Round2-Delivery/` — rebuild is the user's call; judge what stands. Note the build's own gate output if a build just ran.
- **The rubric**: `tmp/regs-p7-18.txt` (extract of `tmp/Automotive-Hackathon-2026-Regulations.pdf` — the Round 2 scoring guide with L0–L3 anchors) and `tmp/template-extract.txt` (extract of the BTC submission-template docx). If an extract is missing, re-extract from the source file first.
- **The baseline**: the highest existing `tmp/AI-Judge-v<N>.md`. Its score, sub-scores, standing-defect list and recovery list are this run's comparison base; the new report is `v<N+1>`.
- **The saved prompt** governing the series under [.claude/prompts/](../../prompts/) — this run appends to its § Outcome.

## 2 · Inspect the bundle (subagent, bundle-confined)

Spawn a general-purpose subagent restricted to the bundle folder — the judge receives nothing else. First run: full inventory (top-level shape, landing-page targets, per-surface claims, link scan, claims-vs-evidence table, template-intake table, defects). Later runs: delta inspection — verify each fix claimed since the prior verdict, re-check every standing defect from the prior report's list, and scan for what the changes introduced. Always: the full relative-link scan with counts, and the exact text of any contradiction.

## 3 · Verify first-hand what the bundle cannot prove about itself

In parallel with step 2, from the main session:

- **The repository link**: anonymous HTTP against the GitHub repo — public or the whole code-by-link story fails.
- **Sampled rewritten links**: a handful of raw-file URLs across kinds (code, schema, markdown doc, folder, CI run) — resolve or 404.
- **The declared version**: the tag via the anonymous API; whether its commit's tree matches the shipped documents.
- **The video's identity**: duration and size from file metadata, against what the report claims.
- Anything else the packet claims that one command can check.

## 4 · Score

Per rubric criterion and sub-category, against the L0–L3 anchors, with the prior verdict as baseline: what level, why, what moved and by how much. Apply the judging premise the user set (assume the code runs and tests pass; topic weighed; risks adverse). Then the three weighing sections — innovation, risk, rival archetypes — updated for the current state, never dropped ([delivery-judgement-format.md § Section order](../../rules/delivery-judgement-format.md)).

## 5 · Write, present, record

- Write `tmp/AI-Judge-v<N+1>.md` per the format rule, all eight sections.
- Author `tmp/ai-judge-v<N+1>-deck.md` mirroring it and build the HTML with the slide tool.
- Append the run to the governing prompt's § Outcome.
- Reply with the score, the trajectory, what moved, the sharpest finding, and the top recoveries — the report carries the detail.

## What this skill never does

- Never modifies the bundle, the packager, or any document the packet renders — findings are named, fixes are ordered by the user.
- Never commits the tmp artifacts.
- Never scores from prose alone what a command could verify.
