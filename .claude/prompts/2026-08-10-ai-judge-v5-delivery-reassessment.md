# Prompt — AI-judge the rebuilt Round 2 delivery bundle (v5, after the v4 findings were fixed)

**Date:** 2026-08-10 **Requested by:** mnpham1986@gmail.com

Follows [2026-08-10-ai-judge-round2-delivery-assessment.md](2026-08-10-ai-judge-round2-delivery-assessment.md), whose v4 report drove the fixes this run re-judges: the documentation-only package, the GitHub code links, the standalone report images, the artifact-retrieval table, and the not-yet-tested provenance marks.

## Prompt text

> Review the Hackathon-Delivery based on only the files contained in that folder.
>
> read 2 files I just added in tmp folder . They include the points guide for this project (at Round 2: Product Development on the Competition Virtual Development Platform in the pdf) and the suggested delivery template (whole docx)
>
> Make report on how my delivery would be judged. Judgement assume 100% successful code run, all tests pass. AND the topic of my projects.
>
> Judgement base on the criteria first and foremost, set by the organizer, as documented in the 2 files
>
> Delivery in Hackathon-Delivery must pass all its claims, provide evidences, and focus on the requirements set by the organizer, as documented in the 2 files
>
> Judgement also are scored against hightly competitive teams, who may different topics. Potential topics are suggested in the hackathon at https://fptautomotive-hackathon.com/challenges-tracks . Think of the innovation aspect of it, NOT the high complexity. Scale, future compatibility is also an innovation factor. Others maybe combination of several track challenges, the goals that the product tries to solve, the clever used of technology, the feasibility (targeting achievable delivery, promising profit). Risks are counted as adverse, negative to judgement. Risks could be high complexity, strict standards that must be met, large requirement set etc.

## Outcome

- `tmp/AI-Judge-v5.md` — the rebuilt bundle judged from its own files only, against the Round 2 rubric (`tmp/Automotive-Hackathon-2026-Regulations.pdf`) and the organizers' delivery template (`tmp/Automotive Hackthon - Final Vòng 2.docx`), with topic/innovation/risk weighing against hypothetical competitive teams from the four hackathon tracks. Working artifact, not committed.
- `tmp/AI-Judge-v6.md` — the same procedure re-run ("perform review on my Hackathon-Delivery tool as before") after the package became HTML-only: no markdown, no code, GitHub links throughout. Working artifact, not committed.
