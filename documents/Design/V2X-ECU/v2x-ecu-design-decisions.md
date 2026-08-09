# V2X ECU — design decisions

> The decision record for [v2x-ecu-hld.md](v2x-ecu-hld.md), which cites these by number. Binding on implementation: a decision is revisited by changing its entry here, never by an implementation that departs from it.

## D1 — Module layering, and the seam made checkable

`src/net/udp_socket.{hpp,cpp}` is the **single socket-API holder**: the only code in this node allowed to include a transport header, and even there only the `.cpp` does. The R7 adapter and the ADA forwarder both consume `net::UdpSocket`; the pipeline, the stub, the codec, the contract bindings and the config stay transport-blind.

- **The ADA-facing link is not under the R7 seam.** The seam mirrors the radio (telux) surface only, while the intra-ego R2 link stays UDP on real hardware. Hence two distinct edges — `adapter/` for the radio, `forward/` for the intra-ego hop — and a hardware port touches neither the pipeline nor the forwarder.
- **A convention nobody can enforce is a convention that decays.** `tools/check_transport_imports.py` fails on any `#include` of `sys/socket.h`, `netinet/*`, `arpa/*` or asio in any spelling from a file under `src/` outside `src/net/`, and re-asserts the F2 ban on the bare `asn1::Cpm` token from the same `sync-manifest.json` that `check_sync.py` reads. It runs locally and as the `contracts-gate` CI step.
- R7's acceptance has two halves and this closes one of them mechanically; the written half is [telux-parity-and-port-plan.md](telux-parity-and-port-plan.md).

## D2 — R7 adapter and R8 stub: an in-process pair behind the seam

`adapter/i_radio_adapter.hpp` freezes `init() · configure(RadioConfig) · subscribeRx(RxCallback) · send(bytes)` with five typed result codes; `StubRadioAdapter` implements it over `stub/modem_stub`. **The stub owns the FSM and every recovery; the adapter owns the socket and the thread.** The adapter therefore adds no retry logic of its own, and a result reaching `main` is already terminal.

- **Fault injection is config-driven**, selected by `FAULT_PLAN`, and every fault has a defined, logged recovery — R8's acceptance is the recovery, not the fault.

| Plan | Defined recovery |
|---|---|
| `init_fail` · `configure_reject` | retry with `RETRY_BACKOFF_MS` between attempts, up to `INIT_RETRY_MAX` retries; a within-budget success emits `Recovery` then the `Ack`. Exhaustion is terminal — the process exits non-zero and container restart is the logged last-resort recovery |
| `subscription_drop` | the call first acks, the subscription then drops once, and re-`subscribeRx` retries **unbounded** with the same backoff. `INIT_RETRY_MAX` bounds bring-up, not a live subscription: a receiver that gives up is a dead node |

- **The illegal-order check runs before any plan.** An out-of-order call is a rejection with no state change regardless of which fault is selected, because the FSM's order is a radio manager's real precondition chain, not a stub artifact.
- **`send` is declared and never called.** R10 is deferred (report §4), so the stub logs one line and returns `NotSupported`. The seam is what makes ego Tx a later implementation rather than a redesign, and the seam text does not change when it arrives.
- **Shutdown needs no self-pipe.** The Rx socket is armed with a short receive timeout, so the blocked receive returns on its own and the loop re-reads the stop flag. `stop()` joins; nothing is detached.

## D3 — R9 Rx pipeline: four stages, each a unit-testable class

`pipeline/rx_pipeline` runs the four stages **synchronously on the Rx thread** — at 10 Hz there is nothing for a queue to absorb, and a queue would add a second place a message can be lost.

1. **decode** — `ICpmCodec::decode`. A `DecodeError` is rejected, counted and logged, never thrown.
2. **validate** — profile ranges and F9, returning a typed reason so counting by reason stays meaningful.
3. **dedupe** — the key `(stationId, objectId, referenceTime + measurementDeltaTime)` over a sliding window. The timestamp component is the **signed sum**, so two CPMs whose parts differ but whose sums agree describe one measurement and are duplicates of each other.
4. **build and forward** — `r2_builder` owns every derivation the codec seam excludes (F1, F6, F7) plus the wire-to-SI conversions; `ada_forwarder` sends the JSON.

- **`onDatagram` is `noexcept` as a hard guarantee, not a hint.** R9's acceptance is zero crashes over a malformed corpus, so the whole body is wrapped: nothing a codec, a stage or a throwing sink raises reaches the Rx thread.
- **Counting lives in `event_log` alone.** The counter is bumped by the method that emits the event, so no stage keeps a parallel tally that can disagree with the log.
- **Forwarding is fire-and-forget.** No retry and no queue: a lost R2 message is replaced by the next one 100 ms later, whereas a retry queue would deliver stale positions after a gap.
- Contract bounds — the profile ranges, the scale factors, the schema version — are **constants, not tunables**. Changing one means re-freezing a contract, never reconfiguring a deployment, so governing principle 5 deliberately does not apply to them.

## D4 — The R18 evidence stream carries its payloads

`log/event_log` writes one `[EVT] {json}` line per event, flushed, to stdout always and additionally to `EVENT_LOG_PATH` when set. The vocabulary is closed at nine names: `rx_datagram`, `decode_ok`, `decode_reject`, `validate_reject`, `dedupe_drop`, `r2_forwarded`, `stub_transition`, `fault_injected`, `recovery`.

- **`decode_ok` embeds the decoded `CpmContent` and `r2_forwarded` embeds the forwarded `R2Message`.** The `[EVT]` stream alone then demonstrates message received → event raised → CPM deserialized to JSON → R2 emitted, with no packet capture and no second tool. Rejected alternative: name-and-counter events, which prove a message arrived but not that it decoded to the right thing.
- **The field names are frozen** because `check_v2x_log.py` parses them (D7). Every event carries `event`, `mono_ms`, `epoch_ms` and the six-key `counters` object; `detail` and `bytes` are free-text diagnostics that no consumer parses.
- **The three stub events increment nothing.** They report; they are not a pipeline stage, and a counter that moves for a non-stage event breaks the exact chain pairing the checker relies on.
- **Lines are prefixed `[EVT]`** so they interleave cleanly with `[CAP]`, `[PCAP-…]` and `[BOOT]` text in one View Log stream.

## D5 — R6 traffic capture runs in the container, and exports through the log

There is no platform pcap facility, and this node's interface sees **both** live flows — bench→V2X R1 and V2X→ADA R2 — which makes it the single capture point. `capture.sh` and tcpdump ship in the image and start from `entrypoint.sh` beside the app. The node config must grant `"capabilities": ["NET_RAW"]` flat.

Two tcpdump processes, two consumers:

| Consumer | Mechanism | What it is for |
|---|---|---|
| Live text | `tcpdump -i any -n -l -tttt $CAPTURE_FILTER`, every line prefixed `[CAP]` | the human "traffic is flowing" check, read live in View Log |
| Saved pcap | `tcpdump -w` into `$PCAP_DIR`, rotated every `CAPTURE_ROTATE_S`; each closed file base64-emitted between `[PCAP-BEGIN <name>]` and `[PCAP-END]`, then deleted | a byte-perfect export through the only egress the platform offers, retrieved by `tools/extract_pcap.sh` |

- **Capture is evidence, not the mission.** Every failure degrades instead of exiting: no tcpdump or no honored `NET_RAW` falls back to a `/proc/net/dev` packet-counter loop, an unwritable `PCAP_DIR` falls back to a temp dir and then to live-text-only, and `entrypoint.sh` deliberately omits `set -e` so a dead capture can never take the node down. The residual risk this carries is that `capabilities` may not be honored, in which case R6's captured-traffic evidence lands at counter level rather than packet level.
- **The `-z` re-entry is why `capture.sh` is dual-mode.** tcpdump execs the `-z` value as a single program name with the closed file as its only argument, so the script exports its own rotations rather than needing a second file.
- **Wireshark will not dissect the CPM payloads, and that is expected.** The wire format is raw UPER with no GN/BTP (F5) and Wireshark's ITS dissector keys on that framing, so CPMs show as opaque UDP data. The evidence is the capture correlated to the `[EVT]` stream by timestamp and length, and the payload bytes matched against the golden vectors — not a protocol tree. R2 on port 47200 is plain JSON and reads directly.

## D6 — The R2 observation point is a sink at the ADA address

"R2 messages observed at the ADA ECU" is evidence about **this** node, so it must not depend on the ADA node's own code being correct or even present. The observation point is therefore a sink deployed at `10.99.0.12`: `tools/netcheck/` with `ROLE=ada-sink`, `LISTEN_PORT=47200`, no next hop, and `BODY_PREVIEW=512` so the whole R2 body reaches its `[RX]` line rather than being truncated at the default preview length.

Nothing on this side changes when the real ADA image replaces it — the forwarder addresses a host and a port, never an implementation.

## D7 — A scripted send-and-assert pair, run in CI and on the platform

Two scripts in `tools/comms_check/`, at the repo root rather than in this node folder: they span the bench and the V2X ECU, so they belong to neither.

| Script | Job |
|---|---|
| `send_cpm.py` | sends the frozen golden `.uper` payloads as one datagram each to a target `host:port` — the bench-side stand-in for local and CI runs |
| `check_v2x_log.py` | asserts, per message, `rx_datagram` → `decode_ok` carrying the decoded `CpmContent` → `r2_forwarded` carrying the R2 body, and exits non-zero naming the first missing link |

- **The frozen counters make the chain pairing exact rather than heuristic.** Each stage event carries its own counter, so a lost log line is detected instead of silently re-pairing two unrelated messages.
- **The `v2x-comms-check` CI lane is the scripted acceptance**: build `v2x_ecu`, run it loopback with a UDP sink standing in for the ADA ECU, send the vectors, pipe the captured stdout through the checker. It proves the receive chain on every push, before any Room exists.
- **The identical checker runs on the platform** in stream mode against a saved View Log, with the live Scenario Player as the sender. One assertion, two configurations, no second implementation to keep in step.

## D8 — Clock domains, and `rxTime` as the one cross-node timestamp of record

This node stamps the only timestamp another node records, so its clock discipline is a contract obligation rather than an implementation detail ([m1-run-timing-and-event-triggering.md §6.2](../../Requirements/m1-run-timing-and-event-triggering.md), R21).

| Value | Clock | Stamped by |
|---|---|---|
| R2 `rxTime` | `CLOCK_REALTIME` (`system_clock`), ms epoch | `rx_pipeline`'s injected `now_ms`, at datagram receipt |
| `[EVT] epoch_ms` | `CLOCK_REALTIME` | `event_log` |
| `[EVT] mono_ms` | `CLOCK_MONOTONIC` (`steady_clock`) | `event_log` |
| the dedupe window | `CLOCK_MONOTONIC` | `deduper`'s injected `now_ms` |

- **Intervals never read the wall clock.** A wall-clock jump — the host's NTP daemon stepping the shared clock mid-run — must neither open nor close the dedupe window.
- **Arithmetic mixing two nodes' timestamps is forbidden.** The F1 derivation uses only `referenceTime[n] − referenceTime[n−1]`, a difference inside the bench's own clock domain. `rxTime − referenceTime` is never computed, and would be wrong twice: two clocks, and two epochs — the profile defines `referenceTime` as `TimestampIts`, ms since 2004-01-01 TAI.
- **`referenceTime` stops at this node.** It is consumed as a difference and as a dedupe key component, and is not carried into R2, so no downstream consumer can be tempted into that subtraction.
- **Readiness is announced in plain text, not as an event.** The two `[BOOT]` lines are the node's ready signal for the operator's R5 Deployment-Viewer check; the `[EVT]` vocabulary stays closed at D4's nine names because `check_v2x_log.py` parses it.
- **This node has no scenario clock.** R20's pacing obligation falls on the bench and the ADA detector; nothing here emits on a schedule, and the Rx path is driven entirely by arrivals.
