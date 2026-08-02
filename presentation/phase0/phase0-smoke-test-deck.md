---
marp: true
theme: default
paginate: true
title: Phase 0 — Baseline Blueprint Smoke Test
description: Report deck — connectivity smoke test on the bench and ECU nodes of the trial2_minh blueprint, covering method, tested object, the division of labour between agent and operator, and results
---

<!-- _class: lead -->
<!-- _paginate: false -->

![bg](../assets/bg-title-city.jpg)

# Phase 0 — Baseline Blueprint Smoke Test

## Proving the network before the ECUs exist

**Milestone 1 · Cooperative Vehicle Awareness — FPT Hackathon 2026**

connectivity smoke test · blueprint `trial2_minh` · 2026-07-31

Sources: [phase0-smoke-test-run.md](../../plans/doc/phase0-smoke-test-run.md) · [baseline-connectivity-smoke-test.md](../../plans/doc/research_notes/baseline-connectivity-smoke-test.md) · [phase0-trial2-minh-preflight.md](../../plans/doc/phase0-trial2-minh-preflight.md)

---

# Table of contents

1. **Introduction** — the reason for proving the network before any ECU code exists
2. **Test method** — the tool, the pipeline, the platform
3. **Tested object** — what was executed, and how it was configured
4. **Testing agent** — the agent's scope, and the limit it encountered
5. **Manual steps** — the five steps requiring an operator
6. **Results** — pass criteria, evidence, open items

---

<!-- _class: lead -->

![bg](../assets/bg-navy-motif.png)

# 01 · Introduction

---

# The rationale for a smoke test before any ECU code

- **Contracts alone are insufficient.** The ECU-to-ECU message schemas were frozen in Phase 0; this test proves the transport beneath them — Ethernet connectivity and IP routing between the ECUs.
- **Prove the topology, not the payload.** One temporary image and three roles (`bench` → `v2x` → `ada`), with no ECU logic, isolates whether a datagram survives the platform from whether the codec is correct.
- **Detect failure early and at low cost.** If the bridge, the pins or the registry do not function, that is better established on a four-file Python tool than on the production V2X ECU image days later.
- **Precondition for Phase 1.** Comms bring-up assumes the blueprint deploys and the nodes reach each other; this test is the proof of that assumption.

**Objective:** a datagram travels bench `10.99.0.10` → V2X `.11` → ADA `.12` → IVI `.13`, stamped at every hop, observed solely through each node's **View Log**, with no interactive session and no manual command execution.

---

<!-- _class: lead -->

![bg](../assets/bg-navy-motif.png)

# 02 · Test method

---

# The pipeline — one image, five tools

<div class="chain-note">Five tools and one temporary artifact, deployed by the same procedure the production ECU images will use.</div>

| Stage       | Tool                                                    | Function                                                                               |
| ----------- | ------------------------------------------------------- | ---------------------------------------------------------------------------------------- |
| **Write**   | Python (`netcheck.py` + `capture.sh` + `entrypoint.sh`) | Send, receive and relay over UDP; capture traffic independently of the application     |
| **Build**   | GitHub Actions (`phase0-ci.yml`, job `netcheck-image`)  | `docker buildx build`, single-platform `linux/arm64`, on a host without Docker or WSL  |
| **Store**   | Zot registry (`registry.hackathon-2.carsky.io`)         | OCI image storage — the registry the production ECU images also push to                |
| **Deploy**  | CarSky platform (Nydus UI and REST)                     | Blueprint `trial2_minh` → Room → three Container nodes pull the same tag               |
| **Capture** | `tcpdump` within each container                         | `[CAP]`-prefixed lines proving the datagram was observed on the network                |

---

# The rationale for UDP, GitHub Actions and Zot

- **ICMP is not used.** `ping` uses ICMP rather than UDP; this test exercises the UDP path the project itself uses. `netcheck.py` calls `connect()` on a UDP socket, which forces a route lookup, transmits nothing, and fails immediately if the peer is unreachable.
- **CI builds because the development host cannot.** Without Docker or WSL locally, every image in this project is built by GitHub Actions; the netcheck image was the first to prove that path end to end, using QEMU and buildx for cross-architecture builds.
- **Single-platform `linux/arm64` is mandatory.** A multi-platform manifest index is silently rejected: the node remains in `Provisioning`, reporting `waiting to start: trying and failing to pull image`. Every image must be built for arm64.
- **Zot is the only registry that responds.** The default host returns 502; `registry.hackathon-2.carsky.io` serves correctly. This closed open item **O1**, and now applies to every ECU image rather than to this test alone.

---

# Configuration — every parameter supplied by environment

| Variable                          | Default | Meaning                                                                                              |
| --------------------------------- | ------- | ---------------------------------------------------------------------------------------------------- |
| `ROLE`                            | `node`  | Identifies the node in every log line and in the relay stamp appended to forwarded payloads.         |
| `LISTEN_PORT`                     | unset   | UDP port to bind. Set only on relay and sink nodes (V2X, ADA); its absence marks a pure source node. |
| `NEXT_HOP_HOST` / `NEXT_HOP_PORT` | unset   | Destination address for outgoing traffic. Set on every node except the final sink.                   |
| `HZ`                              | `1`     | Transmission rate in hertz, kept low so that the log remains readable while traffic stays continuous.|
| `PAD`                             | `0`     | Additional padding bytes appended to the payload, used only for the optional packet-size probe.      |
| `START_DELAY_S`                   | `20`    | Delay before the first transmission, allowing downstream nodes to finish starting.                   |

No behaviour is compiled into the image: role, addressing and rate are read from the process environment at start-up, consistent with the project's principle that no tunable value is hardcoded.

---

# Node configuration — `NEXT_HOP_*` mirrors the production topology

| Node        | Role   | Environment set                                                                    |
| ----------- | ------ | ---------------------------------------------------------------------------------- |
| Bench `.10` | source | `ROLE=bench`, `NEXT_HOP_HOST=10.99.0.11`, `NEXT_HOP_PORT=47100`                    |
| V2X `.11`   | relay  | `ROLE=v2x`, `LISTEN_PORT=47100`, `NEXT_HOP_HOST=10.99.0.12`, `NEXT_HOP_PORT=47200` |
| ADA `.12`   | relay  | `ROLE=ada`, `LISTEN_PORT=47200`, `NEXT_HOP_HOST=10.99.0.13`, `NEXT_HOP_PORT=47300` |

- The `NEXT_HOP_*` values duplicate the addresses each node already holds for its production peer — the bench's `NEXT_HOP_HOST` mirrors its existing `V2X_ECU_HOST` — so the test exercises the production addressing rather than a parallel set of constants.
- The ADA node's `LISTEN_PORT` was added alongside its existing `V2X_LISTEN_PORT` key rather than replacing it, so that its production configuration remains intact once the smoke-test values are reverted.
- `capabilities: ["NET_RAW"]` is set identically on all three nodes.
- Values are entered per node in the Nydus Inspector — the panel that also sets `image`, `command` and `capabilities` — and read by `netcheck.py` at start-up.

---

# Transmission and reception — the socket calls

```python
# route_check() — reachability without ICMP
s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
s.connect((NH, int(NP)))

# receiver() — bind and listen
rx = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
rx.bind(("0.0.0.0", int(LISTEN)))
data, src = rx.recvfrom(65535)

# sender() and relay forward — transmit
tx.sendto(out, (NH, int(NP)))
```

- `connect()` on a UDP socket performs a kernel route lookup and transmits nothing; an unreachable peer raises an error immediately.
- The receiver binds to `0.0.0.0` rather than a named interface, because the bridge interface name is not guaranteed by the platform; `recvfrom()` blocks until a datagram arrives.
- A node with both `LISTEN_PORT` and `NEXT_HOP_HOST` set — V2X and ADA — relays every received datagram through the same `sendto()` call the transmission loop uses.

---

# Payload — a stamped string, not an encoded message

- The payload is plain ASCII text, `f"seq={i}|{ROLE}".encode()`, optionally padded with `PAD` filler bytes for the packet-size probe. No serialisation format and no codec is involved.
- Each relay hop appends its own role rather than re-encoding the payload: `data + f"|{ROLE}".encode()`. The accumulated `|bench|v2x` suffix observed at the ADA node is the chain-of-custody evidence for pass criterion C5.
- The payload is deliberately unencoded, to isolate transport from the message codec. Production CPM traffic is ASN.1 UPER produced through the V2X ECU's Vanetza codec seam — a separate, already frozen contract that this test does not exercise.

---

# Traffic capture — `tcpdump`, gated by `NET_RAW`

- `capture.sh` runs in the background, started by `entrypoint.sh` independently of the check program, so that a capture failure never blocks the connectivity test.
- It probes its own privilege first: `tcpdump -D` succeeds only if the process can enumerate capture-capable interfaces, which requires the `CAP_NET_RAW` capability the platform grants through the node's `capabilities: ["NET_RAW"]` configuration field.
- On success it runs `tcpdump -i any -n -l -tttt "${CAPTURE_FILTER:-udp}"`, piping every line through `sed 's/^/[CAP] /'` so that packet lines remain distinguishable in the combined log stream.
- On failure it falls back to polling the `/proc/net/dev` packet counters, which require no elevated privilege; pass criterion C4 is then met at counter level rather than packet level.

---

# Entry point — `entrypoint.sh` starts both programs

- `entrypoint.sh` is the image's `CMD` (`Dockerfile`: `CMD ["./entrypoint.sh"]`); the container runtime invokes it as the pod starts, with no manual execution, satisfying the self-start requirement of every run subtask.
- It starts `capture.sh` first, in the background, so that capture begins before any application traffic exists.
- It then replaces itself with the check program through `exec python3 -u netcheck.py`. `exec` transfers the process identifier to Python rather than spawning a child, so the container lifecycle and its restart count track the check program directly.
- Every step emits a `[BOOT]` line, so a log without a `[BOOT] … exec python3` entry is itself diagnostic: it indicates that the interpreter failed to start.

---

<!-- _class: lead -->

![bg](../assets/bg-navy-motif.png)

# 03 · Tested object

---

# What was executed

- **Three CarSky Container nodes**, running one temporary image (`m1-netcheck:latest`) on all three — bench, V2X and ADA — differing only by environment variables (`ROLE`, `LISTEN_PORT`, `NEXT_HOP_HOST` and `NEXT_HOP_PORT`), exactly as the production ECU nodes are configured.
- **An `arm64` machine.** The cluster's runnable evidence is uniformly `aarch64`; every node pulls a `linux/arm64` image without exception.
- **The `NET_RAW` capability**, granted explicitly in each node's configuration, which enabled real `tcpdump` capture rather than the `/proc/net/dev` counter fallback. This resolved open item **O2**.
- **The Ethernet-bridge network** — all four `ethernet` pins (`10.99.0.10`–`.13`) wired as a star topology into one `eth-bridge` node: a single flat domain, `10.99.0.0/24`, shared by the bench, V2X, ADA and IVI nodes.
- **The IVI node is outside the scope of the container test.** It is a Skycraft (AAOS) node running an Android VM artifact rather than a container image, and therefore cannot execute `netcheck.py`.

---

<!-- _class: lead -->

![bg](../assets/bg-navy-motif.png)

# 04 · Testing agent

---

# The agent's scope

- **Credentials — two, both supplied by the operator.** A CarSky API key (`a8k_…`) for REST calls covering deployment status, node phases and logs, and a Zot registry key (`CARSKY_ZOT_API_KEY`, `zak_…`) held as a GitHub Actions secret for the image push. Neither was assumed or generated; the agent's login procedure halts and requests a key when none is available.
- **Authored the test artifact.** All four `tools/netcheck/` files (`Dockerfile`, `entrypoint.sh`, `capture.sh`, `netcheck.py`) and the CI job (`.github/workflows/phase0-ci.yml`, `netcheck-image`), committed as subtasks `6.0.8.1` and `5.0.8.2`.
- **Verified over REST rather than by inspection.** `GET /deployments/{roomId}/status`, `.../nodes` for per-node phase, `.../logs/{nodeKey}?container=user|sidecar`, `.../pods`, and `GET /devices` to resolve the target Room — the same calls the room-diagnostics procedure uses to diagnose a failed deployment.

---

# The limit the agent encountered — the IVI node

- **No observation channel to the Skycraft node.** Screenshot (`GET /vms/{roomId}/{nodeKey}/screenshot`), accessibility tree (`.../accessibility`) and ADB shell (`.../shell`) all return **502, unavailable on this deployment**.
- **Direct proof of the third hop was unobtainable.** The strongest available check — an ADB shell `nc -u -l -p 47300` listener on the AAOS guest — requires the shell API this deployment does not serve.
- **An indirect check was used instead.** The ADA node's own `[TX] … relayed to 10.99.0.13:47300` log line, together with its `[CAP]` capture entry, proves the datagram left the network addressed to the IVI node: evidence that the packet was transmitted, not that it was received.
- **The gap closes in Phase 5.** Once the IVI node's production listener for ADA messages lands, the third hop gains a direct check and this substitute is withdrawn.

---

<!-- _class: lead -->

![bg](../assets/bg-navy-motif.png)

# 05 · Manual steps

---

# Five steps requiring an operator

- **Create and configure the blueprint.** The REST `addPin` enumeration contains no `ETHERNET` value, and `PATCH /nodes/{id}` returns 404, so wiring the four pins and setting each node's `image`, `command`, `capabilities` and environment is possible only in the Nydus UI. Both steps would otherwise have been performed by the agent.
- **Review the test tool.** The netcheck tool and its CI job were reviewed before activation and before any registry push.
- **Create the credentials and supply them on request.** The Zot API key and the CarSky API key are both created by an operator and supplied when the agent requests them; the agent never generates or infers a key.
- **Perform the deployment.** Selecting **New Deployment** in Nydus once node configuration is set (M9). The platform permits only two concurrent deployments, so teardown is also an operator decision.
- **Inspect the logs.** Reading each node's **View Log** against the expected pattern (M10) — the step that supplies the IVI node's only evidence, since the agent's REST path cannot reach that node.

---

<!-- _class: lead -->

![bg](../assets/bg-navy-motif.png)

# 06 · Results

---

# C5 — the chain, proven by its accumulated stamps

Each hop appends its role to the payload. The accumulated stamp at the ADA node proves the datagram transited every node, rather than merely arriving from an unspecified source.

<div class="chainviz">
  <div class="link">bench<small>seq=287</small></div>
  <div class="arr">→</div>
  <div class="link hot">v2x<small>seq=287&#124;bench</small></div>
  <div class="arr">→</div>
  <div class="link">ada<small>seq=287&#124;bench&#124;v2x</small></div>
</div>

```
bench  [TX] #285 to 10.99.0.11:47100          len=13
v2x    [RX] #288 from 10.99.0.10  body=seq=287|bench
       [TX] #288 relayed to 10.99.0.12:47200  len=17
ada    [RX] #289 from 10.99.0.11  body=seq=288|bench|v2x
       [TX] #289 relayed to 10.99.0.13:47300  len=21
```

---

# Pass criteria — all five met, 2026-07-31

| #      | Check               | Evidence                                                                      |
| ------ | ------------------- | ----------------------------------------------------------------------------- |
| **C1** | All nodes `Running` | 5 of 5 nodes, restart count 0, stable across a 10-minute window               |
| **C2** | Zero errors         | 0 `[ERR]` lines across the bench, V2X and ADA nodes                           |
| **C3** | Live per-node log   | 100 lines each, streaming                                                     |
| **C4** | Network capture     | 80 / 66 / 66 `[CAP]` lines on `10.99.0.x` — real `tcpdump`, `NET_RAW` honoured|
| **C5** | Chain proven        | The ADA log carries `body=seq=288\|bench\|v2x`                                |

**Deployed as** `trial2_minh_netcheck` · Room `27gs83k3oeju2mbywu1j8` · the deployment alone started every program, with no manual execution at any point, meeting the self-start requirement.

---

# Open items carried forward

- **Registry host resolved (O1).** `registry.hackathon-2.carsky.io` now applies to every ECU image, not to this test alone.
- **Platform tenancy gap.** `GET /blueprints` returns every owner's blueprints unfiltered, including another team's private topology and inline scripts. Reported to the organisers; unrelated to Milestone 1 delivery.
- **Unreliable REST routes.** `restart` returns 500 and `container-exec` returns 503; teardown and redeployment is preferable to either.
- **O3, packet-size headroom.** The optional `PAD=1400` probe was not executed; it informs the message-size budget later.
- **O4, AAOS `nc` availability.** Still unknown; the indirect ADA-side check substituted for it in this run.

**Milestone 1 impact:** the final Phase 0 acceptance criterion — blueprint topology documented and validated — is closed. Phase 1, comms bring-up, is unblocked.

---

<!-- _class: lead -->
<!-- _paginate: false -->

![bg](../assets/bg-fpt-tower.jpg)

# Thank you

**Phase 0 Smoke Test — Baseline Blueprint Connectivity** · FPT Hackathon 2026
