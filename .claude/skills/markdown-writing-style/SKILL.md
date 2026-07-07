---
name: markdown-writing-style
description: Writing-style rules every agent must follow when authoring or editing any markdown document in this repo (research notes, reports, plans, rules, agent specs) — no hard line-wrapping, concise bulleted language, ambiguity/duplication checks, reference instead of repeat.
---

# Markdown Writing Style (all agents)

Trigger: any agent writing or editing a markdown file in this repo. Apply all four rules before saving.

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

## How to apply

All agents ([[project-researcher]], [[project-architecture]], [[project-planner]], and implementation subagents) apply this to every markdown deliverable — research notes, reports, HLDs, plans, rules, agent specs. A document violating these rules is not done.
