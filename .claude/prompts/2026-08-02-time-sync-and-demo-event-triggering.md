# Prompt — timing synchronization between ECUs, startup sync flow, demo event triggering

**Date:** 2026-08-02 **Requested by:** mnpham1986@gmail.com

## Prompt text

> project-researcher, research on timing synchronization issue
> - our scenario may need time sync between the ECU.
> - for example, the video shows car B at time 1:1001 sec, the V2X-ECU messages arrives at 1:1002 sec.
>   - Therefore, Bench should have sent the V2X message way before; the V2X-ECU must have enough time to decode the message and forward info to ADA-ECU at 1:1002. The Scenario should knows that, and the ADA shouldn't have receive event to read video, and detect obstruction before 1:1001
>   - Therefore, our test or demo should trigger scenario player to send message early
>   - Therefore, our test or demo should trigger event requiring ADA-ECU to detect video early enough.
>   - The ADA-ECU should hold the authorative centralized clock; we may need to module to sync time between ECU at system startup.
>
> Consider all the above conditions, and research:
> - whether it is needed to provide system start up call flow, where all ECU sends message to one another like the smoke test in phase0, and sync time.
> - whether a demo application or script should be provided to correctly provide Event triggers that ADA-ECU reads a video at time A, and Bench sends CMP messages at time B.
> - how to do those things.
>
> save this prompt

## Outcome

- **Report (uncommitted):** `documents/Requirements/m1-run-timing-and-event-triggering.md` + `m1-run-timing-startup-flow.puml`, `m1-run-timing-alignment.puml`.
- **Answer 1 — no time-sync module and no startup handshake.** Container clocks already agree to within ~55 ms (derived from the Phase 1 comms run log, where the V2X ECU's own `rxTime` inside the datagram body straddles a second boundary against the ADA-side sink's own clock), and nothing in M1 performs arithmetic on another node's timestamp — R4 carries no timestamp field at all, so the AAOS guest, the one node where sync would be genuinely hard, provably needs none. Readiness stays R5's operator Deployment-Viewer check plus a per-node `[EVT] ready` line and a bench `start_delay_s`.
- **Answer 2 — no trigger application.** Each stimulus source self-schedules from its own config against its own process start; the operator's bench-node restart is the GO. The tool worth writing is a post-run checker, `ADA_ECU/tools/check_run_alignment.py`, not a trigger.
- **Answer 3 — how:** pace both sources on `CLOCK_MONOTONIC` deadlines, add four config keys (bench `start_delay_s`; ADA `DETECTOR_REALTIME_PACING` / `DETECTOR_CLIP_FPS` / `DETECTOR_START_DELAY_S`), rule which clock stamps which R3 field, verify five KPIs read entirely off ADA's own clock. No frozen-contract or blueprint change; ~1 day.
- **Premise correction:** the real defect is *pacing*, not clocks — the bench counts ticks with an uncorrected `sleep(period)`, and the detector has no time base at all (free-running, `DETECTOR_FRAME_STRIDE` is decimation not rate). That error term is unbounded; clock offset is 55 ms. The "send early" lead needed is < 55 ms, supplied automatically by the continuous 10 Hz stream, and `default.yaml` already gives 6.4 s of slack with a fresh admission event every 20 s.
- **New requirements R20** (real-time paced stimulus sources) and **R21** (run alignment / cross-source correlation), both at-risk via inherited Phase 3 risk, both demo-*quality* rather than R19-gating — schedule behind Phase 3/4 acceptance.
- **Side findings:** the bench writes Unix epoch ms into CPM `referenceTime` where the frozen R1 profile says `TimestampIts` (non-conformant, behaviour-neutral); `measurementDeltaTime` is always 0 on the wire; the ADA HLD's `now - lastUpdated` never says which clock `now` reads.
