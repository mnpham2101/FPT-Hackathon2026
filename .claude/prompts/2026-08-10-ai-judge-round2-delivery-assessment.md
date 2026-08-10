# Prompt — AI-judge the Round 2 delivery bundle against the BTC rubric, three escalating variants

**Date:** 2026-08-10 **Requested by:** mnpham1986@gmail.com

## Prompt text

> read 2 files I just added in tmp folder . They include the points guide for this project (at Round 2: Product Development on the Competition Virtual Development Platform in the pdf) and the suggested delivery template (whole docx)
>
> I will deliver that bundle of websites, the presentations (presentation\system-design\system-design-deck.html and presentation\phase6-systemIntegration\phase6-system-delivery-deck.html) and the video demo the system run.
> Make a report on how my delivery would be judged. Judgement is based on the delivery, and assume 100% successful code run, all tests pass. Save to AI-Judge-v1.md in tmp, and make presentation, also put in tmp.
>
> Make another report on how my delivery would be judged. Judgement is based on the delivery, and assume 100% successful code run, all tests pass. AND consider my delivery on automation test tools, wiki page. Save to AI-Judge-v2.md in tmp, and make presentation, also put in tmp.
>
> Make another report on how my delivery would be judged. Judgement is based on the delivery, and assume 100% successful code run, all tests pass. AND consider my delivery on automation test tools, wiki page AND the topic of my projects. Think of potential topics suggested in the hackathon at https://fptautomotive-hackathon.com/challenges-tracks . Think of the innovation aspect of it, NOT the high complexity. Scale, future compatibility is also an innovation factor. Others maybe combination of several track challenges, the goals that the product tries to solve, the clever used of technology, the feasibility (targeting achievable delivery, promising profit). Risks are counted as adverse, negative to judgement. Risks could be high complexity, strict standards that must be met, large requirement set etc.
> Save to AI-Judge-v3.md in tmp, and make presentation, also put in tmp.
>
> save the prompt!

Mid-run addenda:

> Note that aesthetic aspect of report is not considered, but used of wiki page, mind map could be considered as innovation. Other teams may used search tools, mini AI search, harsh wiki, which are innovative. Consider the possibility of implementing the innovation in out timeline. Save the prompt!

> also considered all teams have same capability to use AI, and achieve the same amount of work in the timeline. What is left is the feasibility of implementing the total scope of their products. So if you make up possible projects to compare mine against, considered that. LIst, and compare the projects and their scores on each aspects vs mine.
>
> You can spawn multiple agents to work on 3 reports, and multiple agent to debate on each report.

## Outcome

Three judge reports and three matching HTML decks in `tmp/` (working artifacts, not committed):

- `tmp/AI-Judge-v1.md` + `tmp/ai-judge-v1-deck.{md,html}` — the base bundle (website hub, two delivery decks, demo video) scored against the Round 2 100-point rubric from `tmp/Automotive-Hackathon-2026-Regulations.pdf`, assuming a 100% successful run and all tests passing.
- `tmp/AI-Judge-v2.md` + `tmp/ai-judge-v2-deck.{md,html}` — v1 plus the automation test tooling (`tools/`, Test-Guides scripts, CI lanes) and the wiki/documentation hub counted as delivered evidence.
- `tmp/AI-Judge-v3.md` + `tmp/ai-judge-v3-deck.{md,html}` — v2 plus topic fit against the four hackathon verticals, innovation (scale, future compatibility, cross-track combination, clever technology use, feasibility/profit) weighed positively and risks (complexity, strict standards, large requirement set) weighed negatively; documentation innovations (wiki, mind map, search) benchmarked against likely competitor deliveries and against the remaining timeline.
