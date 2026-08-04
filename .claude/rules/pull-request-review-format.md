# Pull-Request Review Format

Governs every review [[project-reviewer]] produces via [pull-request-review](../skills/pull-request-review/SKILL.md). Fixes **where the review lives, what it must contain, and in what order** — the procedure for producing it is the skill, and the writing style is [markdown-writing-style](../skills/markdown-writing-style/SKILL.md).

A review is a **verdict on work already done**. It is not a plan, not a design, and not a task list. The rule that keeps it that way is § The conclusion is encouragement, never a plan.

## Location & naming

Reviews live in the **reviewed node's own `doc/`**, beside the HLD that judged them, at:

```
<Node_Folder>/doc/<node-slug>-pr<N>-review-v<K>.md
```

Worked example: [ivi-ecu-pr2-review-v1.md](../../IVI_ECU/doc/ivi-ecu-pr2-review-v1.md).

| Element | Value |
|---|---|
| `<Node_Folder>/doc/` | the `doc/` of the node the PR principally develops, per [node-code-layout.md § Per-folder doc/](node-code-layout.md#per-folder-doc) |
| `<node-slug>` | the same slug the node's HLD uses — `ivi-ecu`, `ada-ecu`, `v2x-ecu`, `scenario-player` |
| `<N>` | the pull-request number |
| `<K>` | the review version, from `1` |

- **The review sits with the design it was judged against.** An agent reading a node folder's `doc/` for context gets the HLD, the decisions, and what the last review of that node found — in one place, without knowing a review happened.
- **A PR spanning several node folders gets one document**, in the `doc/` of the node carrying most of the change; cross-node findings stay in it rather than being split. A PR with no principal node goes to [plans/doc/](../../plans/doc/).
- **Reviews are committed and pushed to `main`**, not to the branch under review. The branch is rewritten, rebased, squashed or deleted as the work lands; the review is the record of what was found and must outlive it.

## Versions are enumerated, never overwritten

**A re-review is a new file at the next `<K>`.** The previous version is never edited and never deleted.

- Each version is a **point-in-time verdict against one commit**. Editing v1 in place destroys the only evidence of what the branch looked like when it was judged, and with it any way to see whether the next round improved.
- **Every version from v2 carries a § Since v*K−1* section**, immediately after the header block: what was fixed, what still stands, and what is newly broken. A reader opening the latest version alone must be able to follow the arc.
- **Version numbers are stable.** They are cited the way task IDs are, and are never renumbered or reused.

## The header block

A blockquote before §1 carrying, one per line: the review version and the date, the PR number and title, the branch, the reviewed commit SHA, the base commit the diff was taken against, the documents the review judged the work against, and — from v2 — the previous version's file and commit. A review whose reader cannot tell **which commit was read** is not reproducible.

## Section order is mandatory

| # | Section | Required | Must contain |
|---|---|---|---|
| 1 | **Architecture review** | always | The 3-column module table of § The architecture table |
| 2 | **Low-level review** | always | Exactly one of the two variants in § The low-level table |
| 3 | **Conclusion** | always | The verdict, the themes worth improving, and nothing that schedules work |

Findings that fit no section — a misplaced file, a stray build artifact, a commit-format miss — go in the comment cell of the row they affect, or in the conclusion's themes. A review does not grow sections to hold them.

## The architecture table

Three columns, one row per **module in the design** — taken from the node's HLD (§3 component architecture and §6 internal components), never invented by the reviewer.

| Column | Content |
|---|---|
| **Module in design** | The component's name as the HLD writes it, with its designated path |
| **% completion** | How much of that component's designed responsibility exists in the branch |
| **Comment** | Whether it abides by the architecture, and whether its relations to other components are correct — the OOP relation (realizes / uses / owns), the API it provides, the seam it sits behind, the layer it belongs to |

- **Rows come from the design, not from the diff.** A designed module the branch never created is a row at 0%, not an omission. This is what makes the table a completion measure rather than a file listing.
- **A component the branch added that the design does not define gets a row too**, marked as unsanctioned, with what it collides with.
- **The percentage is justified in the comment**, never left bare. "60% — parses and routes, but nothing collects its output" is a review; "60%" is a number.
- **Judge the relation, not only the existence.** A component that exists but is wired to nothing, bypasses its seam, or reaches across a layer is a defect the comment must name even at high completion.

## The low-level table

Two variants. **Pick by whether the commits' task IDs resolve against the repo's authoritative plan** — the plan on the base branch, not a plan the PR brought with it.

### Variant A — the commits match the plan

Use when the commit IDs resolve to subtasks of the authoritative plan. Three columns:

| Column | Content |
|---|---|
| **Task ID** | The `X.Y.Z.W` from the commit tag |
| **Commit title** | The commit's subject line |
| **Review** | Whether the subtask's stated goal is completely implemented, and what deviated |

### Variant B — the commits do not match the plan

Use when the commits carry no task IDs, or when their IDs are off from the authoritative plan — including the common case of the branch carrying **its own** plan whose IDs collide with the authoritative one. Judge the delivered features against the requirements instead. Three columns:

| Column | Content |
|---|---|
| **Requirement** | `R1`, `R2`, … from [m1-cooperative-awareness.md §2](../../requirements/m1-cooperative-awareness.md) |
| **% completion** | Against that requirement's **acceptance** clause, not its headline |
| **What is missing or deviated** | Named plainly, with the file or contract field it concerns |

- **State which variant was chosen and why**, in one sentence before the table. A reader must be able to see that the choice was forced by the evidence.
- **Variant B still lists the commits**, as a short inventory below the table — it is the evidence for the ID mismatch, and it is what shows commit-discipline defects ([task-planning-conventions.md](task-planning-conventions.md#commit-message-format)).
- **Only requirements the PR touches get rows.** A requirement no part of the branch serves is out of scope, not a 0% row.

## Evidence, not impression

Every claim in a review is checkable by the person reading it.

- **Cite the file and line** for a code finding, the contract field for a contract finding, the requirement's acceptance clause for a completion finding.
- **Run what can be run.** Build output, unit-test counts and CI status are facts; "the tests look thin" is not. Where the reviewer could not run something, say so rather than inferring the result.
- **Separate what is broken from what is absent.** A component that exists and misbehaves and a component that was never written are different findings with different fixes.
- **No speculation about intent.** Describe what the code does and what the design requires; the gap between them is the finding.

## The conclusion is encouragement, never a plan

The conclusion tells the author their work needs improvement, and does it in a way they can act on without being discouraged.

- **Open with what genuinely works.** Real strengths, named specifically — not a softening preamble before the criticism.
- **Group the gaps into a few themes**, not a re-list of every row above. Three or four themes is the budget.
- **Address the author as a colleague.** Plain, warm, direct. No grading language, no scolding, no praise the evidence does not support.
- **Do not plan the work.** No task IDs, no subtask breakdown, no phases, no ordering, no estimates, no "next steps" list, no branch names. Naming *what* falls short is the review's job; deciding *how and when* it gets fixed belongs to [[project-planner]], and a reviewer who writes it has taken the planner's decision away from them.

Stating a theme and stating a plan differ in kind, not in politeness. "The receive loop never reaches the screen — nothing constructs the ViewModel" is a finding. "Add `MainActivity`, then wire `IviGraph`, then re-run the lane" is a plan, and does not belong in the document however it is phrased.

## How to apply

- [[project-reviewer]] applies the section list when outlining the review, and again before committing it. A review missing a mandatory section, or carrying a plan in its conclusion, is not done.
- [[project-planner]] may read a committed review as input when the user asks for the follow-up work to be planned — the review states the gaps, the planner decides the tasks.
