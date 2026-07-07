# Prompt — Scaffold `.claude` project structure

**Date:** 2026-07-04 **Requested by:** mnpham1986@gmail.com

## Prompt text

> * create claude folder to store context rules for this project.
> * create subfolder prompt to store important prompts when requested as markdown file. This prompt is stored.
> * create subfolder agent, skill, rules to store respective agent , skill, rules for this project.
> * read milestone1.md for project goals, and scope, and current plans. Move everything into appropriate claude's sub folder.
> * create agents with clear responsibility, scope of work, and no overlapping scope.
>   1. project-researcher to analyse project requirements, study feasibility, optimize requirements, suggest changes, enummerate requirements, choose most appropriate tech stacks, tools, tool chains to achieve goals within 2 months. Also analyse the trades off, probability of extending project to similar requirements.
>   2. project-planner to create implementation plan that has phases, input and output (acceptance criteria) for each phase, task and subtasks. Each sub task must have strict single objective, atomic commit without any out-of-scope code, pass build, pass unit tests. Tasks could be done in parallel or sequentials, groups related subtasking to achieve single feature or technical solution and must be mapped to a requirement or phase. Project-planner can spawn subagents to implement and test subtasks. Subagents are not yet defined now until project-architecture finalise designs. Project-planner mark tasks done when subagents finish tasks objective and make commit. Use taskID X.Y.Z.W to map the task with requirement X, phase Y, task Z, subtask W. Project planner **must** reads code base to make tasks and subtasks. Project planner provides all necessary information to in subtask and to subagent. Subagent should not need to re-read code, unless there is implementation issue.
>   3. project-architecture to analyse the chosen tech stacks, tooks, tool chains, and create architecture design, high level design, project folder structure, configure dependencies, tool chains. Project-architecture **may** read code to design new module, high level design, but doesn't do low-level design, tasks or subtask analysis.
> * create .CLAUDE to store project constitution.

## Outcome

Established `.claude/{prompt,agent,skill,rules}/` structure, migrated `milestone1.md` content (plus its diagrams) into `.claude/rules/`, authored three non-overlapping agent specs (`project-researcher`, `project-planner`, `project-architecture`) under `.claude/agent/`, and wrote the project constitution to `.CLAUDE` at the repo root.

## Grading: 
* 3/10
* incorrectly created redundant rules. No rule on taskID, commit, project planning requirement.
* agent creation ok, need optimized.