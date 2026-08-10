# Delivery Judgement Format

Governs every judgement [[delivery-judge]] produces via [delivery-judgement](../skills/delivery-judgement/SKILL.md). Fixes **where the verdict lives, what it must contain, and in what order** — the procedure for producing it is the skill.

A judgement is a **simulated Round 2 scoring of the outgoing delivery packet**, from the judge's seat: only the files inside the packet, plus what a diligent reviewer can verify through the packet's own external links. It is not a code review ([pull-request-review-format.md](pull-request-review-format.md) governs those) and not a plan.

## Location, naming & versioning

- The report is `tmp/AI-Judge-v<N>.md`; its presentation is `tmp/ai-judge-v<N>-deck.{md,html}`.
- `<N>` continues the existing sequence in `tmp/` — **a re-judgement is a new version, never an edit of a previous one**. Each version is a point-in-time verdict on one build of the packet; the prior versions are its baseline.
- **These are working artifacts and are not committed.** The prompt that requested the run is saved and committed under [.claude/prompts/](../prompts/), and each run appends one line to that prompt's § Outcome.

## The header block

Lines before the first section: date · team and solution, with the declared tag · the judged object (the packet path and its verified shape — file count, what ships and what does not) · the rubric sources · the judging standard · the method, naming what was inspected and what was verified first-hand · the prior verdicts with their scores.

## Section order is mandatory

| # | Section | Must contain |
|---|---|---|
| 1 | **Executive verdict** | The score, the trajectory, and the one-paragraph argument for it — what moved, what did not, what dominates the gap to the ceiling |
| 2 | **Scorecard** | The six rubric criteria with max, prior scores, current score and delta; sub-category movements listed with their cause |
| 3 | **Findings** | Three groups: verified fixed this round · standing (carried from prior verdicts) · new or sharpened this round |
| 4 | **Template compliance** | The mandatory-intake table from the BTC template, one row per item, verdict per row against the prior verdict's |
| 5 | **Innovation assessment** | The positive weights: cross-track combination, clever technology, scale and future compatibility, documentation, feasibility — each rated with its basis |
| 6 | **Risk assessment** | The negative weights: each risk with severity, finding, and the mitigation already in the packet |
| 7 | **Rival archetypes** | The packet against hypothetical top-quartile teams from the hackathon tracks — estimated scores, per-dimension comparison, and where each side wins |
| 8 | **Recoveries** | Remaining actions ranked by points at stake, each with its estimated worth; the ceiling estimate the list supports |

**Sections 5–7 are never dropped.** A delta re-judgement may shorten them to what changed, but a verdict without the innovation, risk and rival weighing answers only half the question the judgement exists for — the omission is a format defect even when the rubric scoring is complete.

## Scoring conventions

- **Score against the rubric's L0–L3 anchors**, per sub-category, weights from the rubric; no credit for volume or inherent difficulty (the rubric's own rule).
- **Judge only what the packet carries or its links prove.** A claim whose evidence lives outside the packet counts only if the packet's own link reaches it and the link was verified to resolve — publicly, without credentials.
- **Verify first-hand what can be verified**: run the packet's link scan, read the video's metadata, resolve sampled external URLs, execute any in-packet check the packet claims. A judgement that only reads prose inherits the packet's own errors.
- **Carry the standing-defect list forward.** Every defect from the prior verdict is re-checked and reported fixed or standing; a defect that silently disappears from the list was not fixed, only forgotten.
- **State the team's position where it differs** from the judge's weighing (precedent: the video-identity finding), and say why the seat agrees or does not.

## The deck

The presentation follows [deck-authoring-conventions.md](deck-authoring-conventions.md) — markdown source built by the slide tool, asset paths reaching `presentation/assets/` from `tmp/`, condensed-noun-phrase slide titles — and mirrors the report's section order: verdict · scorecard · findings · innovation and risk · rivals · recoveries.

## How to apply

- [[delivery-judge]] applies the section list when outlining the report, and again before finishing — a missing section 5, 6 or 7 is not done.
- The packaging fixes a judgement motivates are the user's to order and other agents' to make; the judgement names them in § Recoveries and stops there.
