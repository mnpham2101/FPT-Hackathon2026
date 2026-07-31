---
name: markdown-writing-style
description: Writing-style rules every agent must follow when authoring or editing any markdown document in this repo (research notes, reports, plans, rules, agent specs) — no hard line-wrapping, concise bulleted language, ambiguity/duplication checks, reference instead of repeat, existing terminology only, conclusions not history, and the right format for the audience (agent reference, human guide, or slide deck).
---

# Markdown Writing Style (all agents)

Trigger: any agent writing or editing a markdown file in this repo. Apply all six rules before saving, then pick the format under § Audience.

## 1. No hard line-wrapping

- Never wrap text at a fixed column width. One paragraph, bullet, or blockquote = one source line; let the editor soft-wrap to the full horizontal space.
- Exceptions: fenced code blocks, and formats that are line-structured by nature (tables — one row per line; PlantUML — one label per line).

## 2. Condensed, concise language

- Prefer bullets over prose paragraphs. List ideas; summarize main points in bullets.
- One idea per bullet. Cut filler, hedging, and restatement — if a sentence adds no new fact or decision, delete it.
- Tables for enumerable facts (options, mappings, verdicts); a short lead-in sentence for context.

## 3. Check ambiguity and duplication

- Before saving, re-read for: vague terms (translate to precise, testable wording — per [requirement-quality-criteria.md](../../rules/requirement-quality-criteria.md) when the doc states requirements), terms used with two meanings, and the same fact stated twice.
- Each fact, decision, or number lives in exactly one place; fix duplicates by keeping the authoritative statement and referencing it (rule 4).

## 4. Reference, don't repeat — or move

- When content already exists in this or another document, link to it (`[file](path)`, or `§n` for a section in the same file) instead of restating it.
- If content fits better in another document (wrong altitude, wrong owner, wrong lifecycle), move it there and leave a link at the original location.

## 5. Use existing terminology — never invent a new concept or label

- Name things the way the canonical source already does: [Car-Sky-Platform.html](../../../requirements/development-platform-doc/Car-Sky-Platform.html) for platform concepts, the relevant developer documentation (Docker, GitHub Actions, the language/framework docs in play) for tooling concepts.
- An error message, log line, or one-off platform quirk is evidence, not license to name a mechanism. If no source defines a term for what's observed, describe the behavior plainly instead of coining one — don't turn an unexplained string into a proper noun the doc then treats as understood.

## 6. Conclusions only — no history, forensic trace, or verbose evidence

- State what is true now: what works, what doesn't, what to do about it. Never narrate how it was found — no investigation timelines, no "verified live on [date]" / "confirmed via" / "re-tested", no step-by-step reasoning trail, no preserved list of eliminated hypotheses.
- A fact needs a date only when the fact itself is time-bound (a deadline, a version cutover); it never needs one to prove it was checked.
- If that kind of analysis is wanted for the record, save it under `tmp/` (git-ignored) instead of the committed doc — only when explicitly requested, never by default.
- **Exception — subtask `**Status:**` lines** ([task-planning-conventions.md § Status tracking](../../rules/task-planning-conventions.md)): these carry the auditable proof of work a subtask closed with — completion date, commit hash, CI run number, and any deviation from the brief. That's engineering traceability, not investigation narrative, and stays exempt from the no-date/no-history rule above. Still write it as a conclusion (what shipped, what it proves) — evidence citations, not a discovery story.

## Audience: pick the format for who reads it

- **Agent-context reference** (default — research notes, rule docs, HLDs, per-node guides, API references): rules 1–6 as-is. Dense, scannable, no prose padding.
- **Human how-to guide** (a walkthrough read once and followed): simple language, numbered steps, one action per step. Still no hard line-wrap, still no invented terms.
- **Human-facing presentation** (meant to be read or exported as slides): author as a Marp deck — `marp: true` frontmatter, `---` slide separators, `<!-- _class: lead -->` section-divider slides, one idea per slide. Follow [m1-proposal-deck.md](../../../presentation/m1-proposal-deck.md) as the template. Before authoring or exporting to static HTML, read [deck-design-system.md](../../../presentation/template/deck-design-system.md) — colors, background images, the reusable CSS class catalog, slide-canvas mechanics, and nav JS, so the design isn't re-derived from scratch each time.

## How to apply

All agents ([[project-researcher]], [[project-architecture]], [[project-planner]], and implementation subagents) apply this to every markdown deliverable — research notes, reports, HLDs, plans, rules, agent specs. A document violating these rules is not done.
