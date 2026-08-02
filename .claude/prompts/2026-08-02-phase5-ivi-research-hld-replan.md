# Prompt — Phase 5 IVI research → HLD → independent re-plan → gap check

**Date:** 2026-08-02 **Requested by:** mnpham1986@gmail.com

## Prompt text

> 1. you are project-researcher, study phase5 and make markdowns:
> * approach to create mini-blueprint that has ADA_ECU, IVI_ECU, and ethernet bridge.
> * how a test tool can simulate msg from ADA_ECU and invoke IVI_ECU logic.
> * technical approach to parse R4 message (msg from ADA_ECU to IVI_ECU.
> * any other neccesary info to implement IVI_ECU.
>
> 2. then spawn project-architecture to create HLD, module design for IVI_ECU. It should have indepent module:
> - reusable library, built as a submodule using nolhmann to parse json.
> - a serializer to parse ethernet message, strip down its header, get only the payload used by IVI application. I think it is Kotlin Serialization.
> - an observer to watch for events, knowing when the message arrives, and the event must be raise.
> - IVI front end module.
>
> 3. then spawn project-planner to perform task planning, as if nothing has been done. The task plan file should be phase5_minh_tasks.md
>
> 4. Then compare the phase5_minh_tasks.md with phase5_task.md if there is any gap. Fix in in phase5_minh_tasks.md. If phase5_tasks.md lack anything, just flag it to at the end. Don't fix any thing in the file phase5_task.md
>
> 5. Analyze current code implementation, check if the tasks have been done. Don't perform code implementation
>
> (mid-run) save the previous prompt!

## Outcome

- *(to be filled at the end of the run)*
