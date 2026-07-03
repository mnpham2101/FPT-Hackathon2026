# Prompt — Optimize `.claude` agents/skills/rules

**Date:** 2026-07-04
**Requested by:** mnpham1986@gmail.com

## Prompt text

> read CLAUDE.md, agent files, plants, rules, skills.
> * optimize them, make them succint, reduce verbose language, remove ambiguity, duplicate.
> * rename, reorganise them as required by claude, so that claude can use agents, skills, rules fluently in this project.
> * ignore plants, and prompt files.
> * must show thinking process clearly.
> * save this prompt

## Outcome

Found the root cause of "not fluent": `.claude/agent/` (singular) and `.claude/skill/` (singular,
flat files) don't match Claude Code's real discovery conventions — confirmed empirically, since
none of the three custom agents or two custom skills appeared in the available agent-type/skill
lists. Fixed by:

- Renaming `.claude/agent/` → `.claude/agents/` (no content restructuring needed — frontmatter
  shape was already correct).
- Restructuring `.claude/skill/*.md` → `.claude/skills/<skill-name>/SKILL.md` (one directory per
  skill, matching the convention), and dropping the non-standard `metadata: type: skill` /
  `metadata: type: rule` frontmatter blocks (redundant once directory location signals the type).
- Updating every cross-reference across CLAUDE.md, the 3 agent files, the 2 skill files, and the 3
  rule files to the new paths (agent files unaffected in relative depth; skill files went one
  level deeper, so `../rules/...` → `../../rules/...` etc.).

Trimmed duplication:

- Removed the trailing "How to apply project rules" section from all three agent files
  (project-researcher, project-architecture, project-planner) — each one restated, in slightly
  different words, what its own "Scope of work" bullets already said. Folded the few genuinely new
  details (e.g. planner's "trace acceptance criteria to phase" rule) into Scope of work instead.
- Merged `task-planning-conventions.md`'s two separately-headed "How to apply" sections (one
  mid-file scoped to commit format, one at the end scoped generally) into a single "How to apply"
  section at the end.
- Shortened CLAUDE.md's opening `.CLAUDE`-vs-`CLAUDE.md` note from a 3-line paragraph to one
  sentence, and fixed its repository-layout description of `.claude/skill/` (previously said
  "empty until concrete skills emerge," which was stale — two skills already existed there).

Left `.claude/plans/` and `.claude/prompt/` untouched per instruction, including the historical
reference to `.claude/agent/` inside the earlier scaffold prompt record (correct to leave as a
historical artifact, not a live link).
