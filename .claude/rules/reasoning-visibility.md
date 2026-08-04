# Reasoning Visibility

Governs the user-facing output of every agent in this repo — the main session and each subagent it spawns.

Two mechanisms enforce it, and they cover different things. [settings.json](../settings.json) keeps the model's thinking on and on screen: `alwaysThinkingEnabled`, `showThinkingSummaries`, `effortLevel`, plus `viewMode` and `defaultView` so the transcript opens expanded rather than behind a keypress. No setting can make the **written reply** carry its own reasoning — that is what this document fixes.

## The reasoning goes in the reply, not only in the thinking block

A reader who never opens the thinking block must still be able to tell why the work came out the way it did.

- **State the conclusion, then what drove it.** "Settings already carry the thinking flags, so the missing piece is the written reply" — the decision and its cause in one sentence.
- **Name what was ruled out, and why.** An alternative considered and rejected is a fact the reader needs; silently discarding it hides the shape of the choice.
- **Cite the evidence that settled it** — the file and section, the command output, the requirement number. A claim with no source reads as a guess whether or not it is one.
- **Say where the uncertainty is** before it becomes the reader's problem: the assumption made, the step not verified, the thing that would change the answer.
- **Report a departure from the request as it happens.** Narrowed scope, a substituted approach, a blocked step — stated in the reply, never left for the reader to infer from the diff.

## Length is not the point

Visible reasoning is a few sentences carrying the load, not a transcript of the search.

- **Two or three sentences per decision.** A paragraph that restates the diff adds nothing the diff does not already say.
- **Skip it for trivial mechanical work** — a rename, a typo fix, a file the user named and a command they specified.
- **Never re-narrate what was already established** in this conversation, and never inflate a correction into an account of the mistake. The conciseness and correction rules elsewhere still bind; this document adds what must be said, never permission to say it twice.

## How to apply

Every agent applies this to its own output. A subagent's final text is read by the agent that spawned it, so the same rule governs it: return the reasoning with the result, not the result alone.
