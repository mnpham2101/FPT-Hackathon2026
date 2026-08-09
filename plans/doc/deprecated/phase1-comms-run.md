# Phase 1 Comms Bring-Up — Run Record

Evidence for [phase1_tasks.md § Task Group 1.10](../../phase1_tasks.md). Each section closes one subtask; sections stay in the plan's order and are filled as the work happens. Deployment procedure is not restated here — it lives in [deploy-walkthrough-netcheck.md](../../requirements/car-sky-guide/deploy-walkthrough-netcheck.md), which the ECU nodes follow with different images and env.

## What is still open

The outstanding subtasks, their executors and what each still needs are [phase1_tasks.md § Remaining work](../../phase1_tasks.md#remaining-work) — the authority for the work list, including which subtask ID owns each step. This record holds only the evidence those subtasks produce, section by section below.

One fact belongs here rather than there: **the `default.yaml` baseline recorded under § `2.1.10.3` was taken against the pre-R22 scenario geometry.** The committed file now carries the R22 demo cycle ([SP D7](../../documents/Design/SCENARIO-PLAYER/scenario-player-design-decisions.md#d7--the-demo-cycle-is-one-clip-length-and-its-geometry-is-solved-backwards-from-the-first-warning)), so the bench image the next Room pulls emits a different approach. A fresh `default.yaml` baseline is captured before `11.1.10.4` compares the swapped stream against it.

## `5.1.10.1` — Images in the registry

Push path was the CI lanes, not the [[car-sky]] agent: `CARSKY_ZOT_API_KEY` is present as a repo secret, so the secret-gated push step in each lane ran instead of skipping. Registry host `registry.hackathon-2.carsky.io` (the O1 live host — the node guides' `registry.carsky.io` login lines remain stale).

| Image | Lane |
|---|---|
| `m1-v2x-ecu:latest` | `v2x-ecu-image`, 18m 54s |
| `m1-scenario-player:latest` | `scenario-player-image`, 20m 27s |
| `m1-netcheck:latest` | `netcheck-image`, 40s |

First confirmed push: CI run `30698630956` on `7a02fb5` (10 lanes green). Both ECU lanes logged `Notice: pushed registry.hackathon-2.carsky.io/<image> (linux/arm64)` after a single `exporting manifest` → `pushing manifest` sequence — **one manifest per image, not an index**, satisfying the single-platform arm64 standing requirement ([phase0-smoke-test-run.md](phase0-smoke-test-run.md)).

**No digests are recorded here on purpose.** All three tags are mutable and every push to the branch re-runs all three lanes, so a digest captured from one run stops describing what `:latest` resolves to as soon as the next commit lands. Identify the deployed image from the registry at deploy time, not from a run log.

**All three tags verified by deployment**, which is stronger evidence than a catalog listing: every node pulled and ran. `m1-netcheck:latest` specifically carries `2.1.9.1` — the deployed sink emits full 339-character `[RX]` bodies, so `BODY_PREVIEW=512` is being read and the image is not the 96-char build.

**Split-tag hazard — resolved by the merge.** While `2.1.9.1` (`1925ce8`) was branch-only, `main` and the feature branch pushed `m1-netcheck:latest` from different code, so any commit to `main` silently reverted the registry copy to the hardcoded-96 build with no visible change in Nydus. Merging Phase 1 to `main` (`aa21f34`) put `BODY_PREVIEW` on both, closing the divergence. The general rule still holds for any future branch-only change to an image's source: the tag is shared, so whichever branch pushed last wins.

### Benign error in both push steps

Both ECU lanes print, mid-push:

```
#28 ERROR: failed to parse error response 400: <h2>Our services aren't available right now</h2>… invalid character '<' looking for beginning of value
```

This is the **GitHub Actions cache service** returning an HTML outage page to the `type=gha` cache exporter (`> exporting to GitHub Actions Cache:`), not Zot — registry auth and the manifest push both succeed after it. Non-fatal by construction: `--cache-to` carries `ignore-error=true` ([phase1-ci.yml](../../.github/workflows/phase1-ci.yml)) precisely so a cache fault cannot fail a multi-hour emulated build.

Consequence for the next run: the cache **export** failed while the import succeeded, so a re-run of either lane may pay the full ~20 min emulated Vanetza compile again rather than hitting warm layers.

## `5.1.10.2` — Blueprint config + deploy

Deployment `phase1_Minh_test-deploy`, 2026-08-01. All four container/bridge nodes pulled their images and ran; the traffic below is the proof they are wired correctly.

Node config per [phase1_tasks.md § Task Group 1.10](../../phase1_tasks.md), with three corrections the guides do not state:

- The Inspector's **Command** field is **space-separated**, not the JSON-array form the node guides print: bench takes `python main.py`, V2X and the ADA sink take `./entrypoint.sh` (relative — workdir is `/app` in both images).
- `FAULT_PLAN` may be omitted on V2X; [config.cpp](../../V2X_ECU/src/config/config.cpp) treats unset-or-empty as `FaultPlan::None`.
- Baseline netcheck leftovers (`ROLE`, `GATE_*_M`, `V2X_LISTEN_PORT`, `IVI_ECU_*`) are inert on all three nodes — no image reads them. `IVI_ECU_HOST`/`IVI_ECU_PORT` in particular cannot turn the sink into a relay, because netcheck relays only on `NEXT_HOP_HOST`/`NEXT_HOP_PORT`.

**Residual:** the Deployment Viewer header read `Pending — 0/0 nodes ready` while logs streamed normally. Per-node `Running` + restart 0 is `5.1.10.6`'s read-back, and the R5 box's deploy clause is not closed until it records them.

## `2.1.10.3` — R2 observed at the ADA ECU

> **R2 is the V2X→ADA object message**: the JSON the V2X ECU emits to the ADA ECU for each CPM it successfully decodes, one UDP datagram per message, shaped by [contracts/r2-v2x-object.schema.json](../../contracts/r2-v2x-object.schema.json). The `body={…}` payloads quoted below *are* R2 messages. (For orientation: R1 is the CPM wire profile arriving from the bench, R6 the bridge network they travel over, R9 the decode pipeline that produces R2.)

**Closed on the live Room.** ADA sink log, five consecutive messages (bodies complete, not preview-truncated):

```
[RX] 13:33:36 ada-sink #5899 … len=339 body={"object":{"classification":"vehicle","confidence":0.95,"distance":50.26432631598677,"objectId":7,"position":{"confidence":0.9,"x":50.25,"y":1.2},"speed":2.5,"timeOfMeasurement":0},"rxTime":1785591216646,"schemaVersion":1,"sender":{"heading":90.0,"lat":21.028510999999998,"lon":105.804817,"speed":0.0},"stationId":1201,"type":"v2x_object"}
[RX] 13:33:36 ada-sink #5900 … "distance":50.014397926996985 … "x":50.0  … "rxTime":1785591216745
[RX] 13:33:36 ada-sink #5901 … "distance":49.764470257403524 … "x":49.75 … "rxTime":1785591216846
[RX] 13:33:36 ada-sink #5902 … "distance":49.51454331809999  … "x":49.5  … "rxTime":1785591216946
[RX] 13:33:37 ada-sink #5903 … "distance":49.26461712020099  … "x":49.25 … "rxTime":1785591217046
```

Four independent checks pass on this excerpt:

- **Not constants** (the R2 box's actual wording): `distance` and `position.x` change every message, decreasing monotonically 50.26 → 45.27 over the observed window.
- **Kinematics match the scenario as deployed**: exactly **0.25 m per message**, and `rxTime` advances ~100 ms — i.e. 2.5 m/s at 10 Hz, precisely the `closing_speed_mps: 2.5` and `cpm_rate_hz: 10` the bench image carried on this run. `cpm_rate_hz` is unchanged in the committed [default.yaml](../../Scenario_Player/scenarios/default.yaml); the closing speed and start distance are now the R22 values, 5.0 m/s from 70.0 m, so a re-run reads 0.5 m per message.
- **The F7 derivation is arithmetically correct.** *F7 is a numbered freeze note in [contracts/r1-cpm-profile.md](../../contracts/r1-cpm-profile.md): `R2 object.distance = hypot(object.position.x, object.position.y)` in metres — a value the CPM never carries, computed at the V2X ECU because the wire format only gives x/y offsets.* Check: `hypot(50.25, 1.2) = 50.26432631…`, matching the logged `distance` to every printed digit. So the field is genuinely derived from the decoded position, not copied through.
- **Every scenario field survives the round trip**: `objectId 7`, `y 1.2` (`lateral_offset_m`), `confidence 0.95` (from wire 95), `classification "vehicle"` (from wire code 5), `stationId 1201`, sender pose `21.028511 / 105.804817 / heading 90.0`.

V2X ECU counters over the same run — the D7 chain with nothing lost or rejected. *D7 is the bench↔V2X comms check in the [V2X decision record](../../documents/Design/V2X-ECU/v2x-ecu-design-decisions.md): the rule that every received datagram must show `rx_datagram` → `decode_ok` (carrying decoded CpmContent) → `r2_forwarded` (carrying the R2 body), asserted by `check_v2x_log.py` rather than by eye.*

```
[EVT] {"counters":{"decode_ok":885,"decode_reject":0,"dedupe_drop":0,"r2_forwarded":885,"rx_datagram":885,…
```

`rx_datagram` = `decode_ok` = `r2_forwarded`, `decode_reject: 0`, `dedupe_drop: 0`. Sequence numbers ran contiguously past **#6500** with no gaps, sustained for minutes at 10 Hz.

**Residual:** `check_v2x_log.py` has not been run against a saved View Log export — that is `9.1.10.7` — and the `[EVT] stub_transition` bring-up sequence scrolled past before capture, which is `8.1.10.8`'s evidence. Both read the V2X node's log, so one restart plus one long enough download serves them.

## `11.1.10.4` — Scenario swap

*Pending.* The comparison needs a `default.yaml` baseline from the same bench image as the swapped run. The approach recorded above predates the R22 retime, so it is not that baseline: the committed file now closes from 70.0 m at 5.0 m/s, 0.5 m per message at 10 Hz. Capture the fresh baseline first, then the `c-out-of-range.yaml` stream, and compare the pair.

## `6.1.10.5` — Capture retrieval → Wireshark

**Capture is running, and the V2X node sees both flows** — the single-capture-point premise of D5, confirmed live. *D5 is the [V2X decision record](../../documents/Design/V2X-ECU/v2x-ecu-design-decisions.md) decision on how R6's traffic capture works: tcpdump inside the V2X image emits live `[CAP]` lines to the View Log and rotates saved `.pcap` files out through base64 markers, because the platform offers no file-download path.*

V2X ECU log — one inbound CPM and its outbound R2, 150 µs apart:

```
[CAP] 2026-08-01 13:25:13.412744 e-eth In  IP 10.99.0.10.43982 > 10.99.0.11.47100: UDP, length 58
[CAP] 2026-08-01 13:25:13.412895 e-eth Out IP 10.99.0.11.46702 > 10.99.0.12.47200: UDP, length 339
```

`In`/`Out` at one interface is exactly what D5 predicted. It also yields a **measured processing latency**: the In→Out gap held at **142–151 µs** across every consecutive pair sampled (`.412744→.412895`, `.511745→.511892`, `.612755→.612904`, `.712748→.712899`, `.812747→.812889`, `.913755→.913898`) — the whole decode → validate → dedupe → build → forward path, sub-millisecond and stable.

The bench→V2X CPM is a fixed **58-byte** UPER payload; the V2X→ADA R2 JSON varies **337–340 bytes** with the digit count of the changing `distance`. That size-tracks-content relationship is the payload-byte correlation D5 relies on, since raw UPER without GN/BTP framing dissects only as UDP data — Wireshark's ITS dissector will not produce a protocol tree.

**Reading direction flags on the *ADA* node** is different, and worth recording so the pcap is not misread later:

```
[CAP] 13:33:36.404483 e-eth P   IP 10.99.0.10.43982 > 10.99.0.11.47100: UDP, length 58
[CAP] 13:33:36.485912 e-eth In  IP 10.99.0.11.46702 > 10.99.0.12.47200: UDP, length 339
```

`P` is promiscuous — bench→V2X frames merely overheard on the shared bridge, not addressed to `.12`. `In` means actually destined for `.12`. Only `In` is delivery evidence, and the paired `[RX] len=` line confirms the socket handed those bytes to the application.

**Residual:** no `.pcap` has been extracted — needs a saved View Log through `V2X_ECU/tools/extract_pcap.sh` and a Wireshark read.
