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

- **Research (`75c9373`)** — four notes in `documents/Design/IVI-ECU/`: mini-blueprint (3 nodes, clone-then-delete because REST/import cannot create `ethernet` pins), R4 simulator (injection points I1–I4; `adb forward` has no UDP), R4 parsing (**no application header exists** — de-framing is `DatagramPacket` offset/length slicing), and the implementation-facts inventory.
- **User decision mid-run:** the reusable JSON submodule is kotlinx.serialization in a pure-JVM Gradle module; nlohmann/json stays C++/ADA-side (it cannot parse JSON in a Kotlin APK without NDK/JNI).
- **HLD (`85387b5`)** — `IVI_ECU/doc/phase5-ivi-hld.md` + component and call-flow `.puml`. Five modules `:contract` ← `:serializer` ← `:observer` ← `:app`, plus `:r4-simulator`; decisions D1–D11; Hilt removed for a manual composition root.
- **Plan (`de1f939`, gap-fixed `f4c3b1a`)** — `plans/phase5_minh_tasks.md`, 9 groups / 45 subtasks (39 agent, 6 user-manual), written from zero without reading `phase5_tasks.md`, then diffed against it.
- **Discovery that reframed step 5:** a near-complete Phase 5 implementation exists on the unmerged `origin/feat/phase5-ivi-hmi-complete`. Two blocking defects in it: the receive loop never resets `DatagramPacket` length (silent truncation), and `WarningViewModel` passes `geometry` through without the R3 snapshot, leaving the R19 provenance guard inert. Mapped into the plan's § Prior work as salvage.
