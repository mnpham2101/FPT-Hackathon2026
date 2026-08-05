# Phase 5 IVI–ADA troubleshooting — v2

> **Finding: v1's verdict stands and is now localized — the R4 warning chain is healthy from bench to ADA in every captured round, and delivery fails in the platform's Skycraft node wiring: the destination `10.99.0.13` lives on the IVI pod's sidecar TAP, the sidecar declares no UDP route into the AAOS VM (its only VM route is TCP 5555 over vsock, for ADB), and the guest itself sits on a `10.0.2.x` NAT network that never sees the bridge.** The app is affirmatively cleared: its socket has been bound and healthy since 19:37:31Z, in-guest loopback probes reach it and log `[DROP]`, and zero of ≥ 85 ADA-counted warnings produced an `[RX]` in ~33 minutes of instrumented listening.

> **Version:** v2 — 2026-08-05/06. Extends [phase5-ivi-ada-troubleshooting-v1.md](phase5-ivi-ada-troubleshooting-v1.md); v1 is not modified.
> **Evidence base:** four instrumented producer-restart rounds against the m1-system-test Room `zvpi8tzdj8s08wmxaaye2` (deployment `cU9rZj_wuhFWIfxFPW18e`, snapshot blueprint `o6XI8rL0wKZc2YMNsHPYV`), captured to the git-ignored working files `tmp/round{1..4}-*.{log,md,json}` plus two guest logcat captures (`tmp/ivi-logcat-full-194509.log`, `tmp/round3-ivi-logcat-live.log`). Guest access over the organizers' reach ADB tunnel ([tools/apk uploader/README.md](../../tools/apk%20uploader/README.md)); every adb command pinned `-s localhost:5555`.
> **Verification:** the per-hop analysis was cross-checked by an adversarial pass (three independent lenses: app-side, producer-side, capture-methodology); none refuted the finding. Their corrections and residual gaps are folded into § Evidence limits.

## Since v1

- v1 established: warnings die before the wire (ADA tcpdump saw zero packets toward `10.99.0.13` across 67 `r4_tx` events, read as unanswered ARP), the app's receive path is good (in-guest loopback `nc -u 127.0.0.1 47300` → `[DROP]` on `IVI_V2X`), and the guest has no `10.99.0.x` presence.
- v2 adds the *why* behind both observations from the platform's own logs and config: the sidecar route table, the skycraft node config, and the guest's NAT routing (§ The delivery gap). It also adds the four-round producer record proving warnings re-fire continuously (§ Producer chain), the provenance of the installed APK (§ The IVI side), the second — now fixed — defect in the app's own receive path (§ The launch-gap defect), the anomalies the restart rounds surfaced (§ Unexpected behaviors), and the solution options (§ Solutions).

## The capture record

| Round | Restart performed | T0 (all producer nodes Running) | Files |
|---|---|---|---|
| 1 | Fresh deploy (ADA image fixed mid-attempt) | ADA `[BOOT]` 19:03:44Z | `tmp/round1-*` |
| 2 | Bench+V2X+ADA restarted in place | 19:11:15Z | `tmp/round2-*` |
| 3 | Bench+V2X+ADA restarted (pod replacement — § Unexpected behaviors) | stated 19:56:04Z; actual pod waves diverge | `tmp/round3-*` |
| 4 | Same three restarted again | stated 20:02:49Z; actual pod waves diverge | `tmp/round4-*` |

The IVI/Skycraft node was never restarted; the guest and its app ran continuously from 19:37Z through the end of round 4.

## Producer chain — healthy in every captured window

| Hop | Evidence across rounds |
|---|---|
| Bench `[TX]` → V2X `:47100` | 58-byte CPMs at 10 Hz, seq strictly monotonic, `scenario_time_s` wrapping at 10.0 s (`loop: true`), zero `[ENC-SKIP]`/`[SND-ERR]`/`[FATAL]` in any round |
| V2X → ADA `:47200` | `rx_datagram = decode_ok = r2_forwarded` in every snapshot (6541 in round 2; 2775 in round 4), all reject/dedupe counters 0, `[CAP]` shows each 58 B in paired 1:1 with a ~338 B forward within ~2 ms |
| ADA risk → `r4_tx` | `r4_tx == risk_transition` exactly, every round: 71 (r1) · 180 (r2) · 8 (r3 wave) · 77 (r4). First warning: T0+9.1 s (r1), T0+36.1 s (r2, ~11 s after the bench resumed). Risk cycles `low→medium→high→low` each ~10 s bench loop — escalations rationale `range`, drops rationale `no_tracked_c` at loop wrap — on the single relayed track `v2x:1201:7`, `source: v2x_relayed`. Zero `parse_reject`, zero send failures |
| ADA send result | Every `r4_tx` payload captured (rounds 1–2, 112+ lines) carries `"dest":"10.99.0.13:47300","send_ok":true`, 516 B. Rounds 3–4 captured counters only (§ Evidence limits) |

The warning is **not** a fire-once event in this deployment: the looping bench re-arms the risk edge every cycle, so the ADA re-fires continuously (~2–4 warnings per 10 s cycle, mean interval ~3.5 s in round 4). A late-starting consumer always has a next warning coming.

## The IVI side — bound, listening, and running the fix build

- **Installed APK provenance:** `tools/apk uploader/app-debug.apk`, installed 19:37:22Z, is the `fix/ivi-warning-screen` CI artifact — its dex contains `com.hackathon.v2x.ivi.model.R4Contract`, a class that exists only on that branch. The replay-fix build is what runs on the guest.
- App launched 19:37:30Z; `R4ListenerService: UDP socket open on port 47300` at 19:37:31.564Z; `/proc/net/udp` and `ss -uln` show `*:47300` bound. Process pid 5017 alive with no crash/ANR/kill through 20:27Z.
- **Zero `[RX]` ever.** Across ~33 minutes of instrumented listening spanning rounds 3–4 (≥ 85 ADA-counted warnings in-window), not one datagram arrived from the network.
- **The socket and parser demonstrably work:** the two `[DROP] Skipping bad packet` lines (20:12:13.694Z, 20:14:26.537Z) are v1's in-guest loopback probes (`probe-not-json` via `nc -u 127.0.0.1 47300`) — datagrams that reach the guest are received, parsed, and logged.

## The delivery gap — where and why

The Skycraft node's own logs and config close the question v1 left open:

- The IVI pod's sidecar declares exactly two routes (quoted from `tmp/round{3,4}-ivi-ecu.log`):
  - `route 'eth-eth': mode=EthTunnel … upstream=tcp:…-n3:40000` — then `TAP 'e-eth' configured: 10.99.0.13/24`, `AF_PACKET attached`, `connected to bridge`. **The pin address `10.99.0.13` terminates on the pod-side TAP.**
  - `route 'adb': mode=Tcp listen=0.0.0.0:5555 upstream=hybrid-vsock-connect:/run/vsock/vm.vsock:5555` — **the only declared path into the VM, TCP-only, for ADB.**
- The skycraft node config (`tmp/round4-snapshot-blueprint.json`) carries image/prefix/display/GPU fields only — **no port-forward field exists**; the eth pin's `port` property is `null`.
- Inside the guest: interfaces are `buried_eth0` `10.0.2.15/24` and `wlan0` `10.0.2.96/24` (cuttlefish NAT); `ip route get 10.99.0.12` resolves **via the NAT gateway `10.0.2.2`**; the ARP table knows only `10.0.2.2`. The guest has no bridge presence and can only originate outbound flows through NAT.

Two candidate break points remain upstream of the guest, and the captures do not yet discriminate them — both are platform wiring, and the guest cannot receive in either case:

1. **On the bridge (v1's reading):** ADA's tcpdump saw zero packets toward `10.99.0.13` across 67 sends — consistent with ARP for `10.99.0.13` going unanswered on the ADA side, the datagrams dying in ADA's kernel. Yet the IVI sidecar holds that address on a bridge-attached TAP, so *something* should answer; whether the EthTunnel answers ARP across the bridge is a platform behavior the logs do not show (the hub logs carry connect/disconnect events only, no frames).
2. **At the VM boundary:** even a datagram that reaches the pod TAP has no declared conduit into the NATed guest.

## The launch-gap defect — the second, now-fixed issue

Independent of delivery, the codebase had a real receive-path defect: ADA warnings are edge-triggered single datagrams (ADA HLD D5 — no repeat, no heartbeat; `STATE_RATE_HZ=0` with no producer), while both hops of the app's flow chain had `replay = 0` — a warning arriving before the service→repository attach, or before a recreated ViewModel subscribed, was silently dropped and the HMI stayed on HomeView. Fixed on `fix/ivi-warning-screen` (commit `17806bd`: `replay = 1` on `R4ListenerService._r4EventFlow` and `R4Repository._warningEvents`, plus regression tests that send the frozen `r4-warning` sample *before* the UI attaches). This defect alone did not cause the in-Room silence — with the bench looping, warnings recur — but it would have eaten the first cycle's edges on every app launch. The same branch delivers the designed `:r4-simulator` test equipment (image `m1-r4-sim:latest`) and its CI lanes; PR pending at `fix/ivi-warning-screen`.

## Unexpected behaviors observed

Found by the cross-checked sweep of all round files; none changes the verdict, several matter for future captures:

- **"Restart" is pod replacement, and its timing is deceptive.** The round-3/4 restarts created new ReplicaSets; stated T0s (all-nodes-Running polls) match no actual pod boot. Round 3 proper (19:56–20:01Z) is essentially uncaptured — its pods died and were replaced ~20:01:50Z before any pull, so whether ADA fired during those 6 minutes is unknowable from the files.
- **An extra, unstated bench pod replacement mid-round-4** (`b97496b99` → `5bc7f45c6`, seq reset, first TX ~20:04:08Z) — pod churn beyond the operator's two restarts.
- **A fifth bridge client existed for 98 s** (19:42:59–19:43:32Z, `10.42.0.126`) against four blueprint tunnel-client nodes — unexplained, transient.
- **Each restart wave loses ~20–49 startup CPMs** before the bench sidecar's tunnel attaches — harmless here (the stream loops), relevant to any startup-latency measurement.
- **The logs API returns ~100-line server-capped tails**, so short pull windows miss per-event lines between snapshots — rounds 3–4 have counter evidence but no `r4_tx` payload lines. Cumulative `[EVT]` counters bridge the gaps.
- **The VM log route is dead on this deployment:** `GET /api/v1/vms/{room}/{node}/logs` → 502 `Conduit service not configured` (also true in rounds 1–2, meaning no prior round ever had guest-side observability — the "sent" evidence was never before paired with a receive check).
- **The bridge and skycraft pods have no `user` container** (`sidecar` / `skycraft` only) — a `container=user` log request against them fails by design, not by fault.
- The known v1 finding stands: the bind line logs as `R4ListenerService: UDP socket open on port 47300`, not the designed `[LINK] state=bound port=47300` on `IVI_V2X` (walkthrough rung V1 deviation).

## Evidence limits

Corrections and residual gaps the adversarial pass insisted on recording:

- Rounds 3–4 carry no direct `send_ok`/`dest` observations (counter parity + rounds 1–2 payloads carry that claim), and no capture window ever caught an R4 datagram on the wire — the ADA-side `[CAP]` windows (~6 s total) never coincided with a send. A long-window capture spanning two 10 s cycles would close this.
- The two `[DROP]` probes' injection path is not recorded *in the capture files* (v1 records it as in-guest loopback); re-running one probe from a Room container and one via adb, logged, would make the discrimination self-contained.
- The guest probes (`ip addr`, `ip route`, ARP, `ss`) were transcribed into this record but their raw transcripts are not saved as files.
- A ~7-minute guest logcat blind spot exists (19:45–19:52Z, between the two captures); no round T0 falls inside it and pid continuity spans it.

## Solutions

Ranked per [solution-selection-criteria.md](../../.claude/rules/solution-selection-criteria.md) — likelihood of working end-to-end first, speed second.

### S1 — Platform fix: ask BTC how the Skycraft eth pin delivers UDP into the guest *(primary; the only one that closes R6/R19 as designed)*

The blueprint promises the guest at `10.99.0.13` (the pin), but the platform terminates that address on the pod TAP with no conduit onward. The precise question for the organizers:

> *The Skycraft node's ethernet pin is configured `10.99.0.13`, and our ADA node sends UDP to `10.99.0.13:47300` (send_ok, correct env). The node's sidecar shows the TAP configured and bridge-attached, but the AAOS guest's only interfaces are `10.0.2.x` (cuttlefish NAT) and its app's bound socket on 47300 never receives. The sidecar declares only one route into the VM — TCP 5555 over vsock for ADB. How is inbound UDP supposed to reach a Skycraft guest: does the sidecar need a UDP route/port-forward configured (is that the pin's `port` field, currently null?), or must the VM NIC be bridged differently?*

Evidence package: this document, v1, and the `tmp/round*` captures.

### S2 — Guest-originated NAT traversal test, then an app-side keepalive *(cheap discriminator, ~10 min; a real fix if the NAT is reachable from the bridge)*

From the guest (over adb), send a UDP datagram **from source port 47300** toward the ADA (`10.99.0.12:47200`). Read two things: whether it arrives (the ADA logs a `parse_reject` — proving guest→bridge egress works and seeding ARP/NAT state), and whether ADA's subsequent warnings then flow back through the NAT mapping into the app (`[RX]` appears). If they do, the durable fix is ~5 lines in `R4ListenerService`: a periodic keepalive datagram from the already-bound socket toward the producer, keeping the inbound mapping alive — standard UDP NAT traversal, no platform change. Either outcome also discriminates the two break points in § The delivery gap.

### S3 — ADB relay: real ADA output on the real screen, today *(demo fallback; test-equipment-assisted evidence)*

A ~30-line host-side loop: poll the ADA node's log via REST, extract each new `r4_tx` `payload.body` (the full R4 JSON), push it into the guest via `adb shell nc -u 127.0.0.1 47300`. The app receives, parses, and renders genuine ADA-produced warnings with ~1–2 s added latency. Provenance must be labeled: the payloads are the ADA's real output relayed by test equipment, not bridge-delivered — acceptable as an interim demo, not as the R19 closing evidence.

### Applicability note

The mini-blueprint route (`m1-r4-sim:latest` on the ADA node) hits the same wall — the simulator sends to the same unreachable address. S1/S2 fix it identically; S3 works there too.
