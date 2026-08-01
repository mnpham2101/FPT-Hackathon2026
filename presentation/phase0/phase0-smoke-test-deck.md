---
marp: true
theme: default
paginate: true
title: Phase 0 — Baseline Blueprint Smoke Test
description: Report deck — connectivity smoke test on the bench and ECU nodes of the trial2_minh blueprint, method, tested object, AI/human division of labor, and results
---

<!-- _class: lead -->
<!-- _paginate: false -->

![bg](../assets/bg-title-city.jpg)

# Phase 0 — Baseline Blueprint Smoke Test

## Proving the wire before the ECUs exist

**Milestone 1 · Cooperative Vehicle Awareness — FPT Hackathon 2026**

connectivity smoke test · blueprint `trial2_minh` · 2026-07-31

Source: [phase0-smoke-test-run.md](../../plans/doc/phase0-smoke-test-run.md) · [baseline-connectivity-smoke-test.md](../../plans/doc/research_notes/baseline-connectivity-smoke-test.md) · [phase0-trial2-minh-preflight.md](../../plans/doc/phase0-trial2-minh-preflight.md)

---

# Table of contents

1. **Introduction** — why prove the wire before any ECU code exists
2. **Test method** — the tool, the pipeline, the platform
3. **Tested object** — what actually ran, and how it was wired
4. **Testing agent** — what the AI did, and where it hit a wall
5. **Human in the loop** — the five steps only a person could do
6. **Results** — pass criteria, evidence, open items

---

<!-- _class: lead -->

![bg](../assets/bg-navy-motif.png)

# 01 · Introduction

---

# Why a smoke test before any ECU code

- **Contract-first, but contracts aren't enough.** The ECU-to-ECU message schemas were frozen in Phase 0 — this test proves the *transport* underneath them: Ethernet connectivity and IP routing between the ECUs.
- **Prove the topology, not the payload.** One throwaway image, three roles (`bench` → `v2x` → `ada`), no ECU logic — isolates "does a datagram survive the platform" from "is our codec correct".
- **Fail cheap, fail early.** If the bridge, the pins, or the registry don't work, better to find out on a 4-file Python script than on the real V2X_ECU image days later.
- **Gate for Phase 1.** Comms bring-up assumes the blueprint deploys and nodes reach each other — this test is that assumption's proof.

**Objective:** a datagram travels bench `10.99.0.10` → V2X `.11` → ADA `.12` → IVI `.13`, stamped by every hop, observed purely from each node's **View Log** — no interactive session, no manual exec.

---

<!-- _class: lead -->

![bg](../assets/bg-navy-motif.png)

# 02 · Test method

---

# The pipeline — one image, five tools

<div class="chain-note">Five tools, one throwaway artifact, deployed the same way the real ECU images will be.</div>

| Stage       | Tool                                                    | Job                                                                                    |
| ----------- | ------------------------------------------------------- | -------------------------------------------------------------------------------------- |
| **Write**   | Python (`netcheck.py` + `capture.sh` + `entrypoint.sh`) | Send / receive / relay over UDP; capture traffic on the wire independently of the app  |
| **Build**   | GitHub Actions (`phase0-ci.yml`, job `netcheck-image`)  | `docker buildx build`, single-platform `linux/arm64`, on a dev host with no Docker/WSL |
| **Store**   | Zot registry (`registry.hackathon-2.carsky.io`)         | OCI image storage — the same registry the real ECU images push to                      |
| **Deploy**  | CarSky platform (Nydus UI + REST)                       | Blueprint `trial2_minh` → Room → three Container nodes pull the same tag               |
| **Capture** | `tcpdump` inside each container                         | `[CAP]`-prefixed lines proving the datagram was seen on the wire                       |

---

# Why not `ping`, why GitHub Actions, why Zot

- **No ICMP reliance.** `ping` uses ICMP, not UDP — we test the actual UDP protocol path that the real project uses instead. `netcheck.py` calls `connect()` on a UDP socket — forces a route lookup, sends nothing, fails loudly if unreachable.
- **CI builds because the dev host can't.** No Docker/WSL locally — every image in this project builds via GitHub Actions; the netcheck image is the first to prove that path end-to-end (QEMU + buildx for cross-arch).
- **Single-platform `linux/arm64`.** A multi-platform manifest index is silently rejected — the node sits in `Provisioning`, `waiting to start: trying and failing to pull image`. Every image must be built for arm64.
- **Zot is the only registry that answers.** The default host returns 502; `registry.hackathon-2.carsky.io` serves correctly — this closed open item **O1**, and now applies to every ECU image, not just this test.

---

# Script configuration — every parameter arrives by environment

| Variable                          | Default | Meaning                                                                                              |
| --------------------------------- | ------- | ---------------------------------------------------------------------------------------------------- |
| `ROLE`                            | `node`  | Identifies the node in every log line and in the relay stamp appended to forwarded payloads.         |
| `LISTEN_PORT`                     | unset   | UDP port to bind. Set only on relay and sink nodes (V2X, ADA); its absence marks a pure source node. |
| `NEXT_HOP_HOST` / `NEXT_HOP_PORT` | unset   | Destination address for outgoing traffic. Set on every node except the final sink.                   |
| `HZ`                              | `1`     | Send rate in hertz, kept low so the log remains readable while traffic stays continuous.             |
| `PAD`                             | `0`     | Extra padding bytes appended to the payload, used only for the optional MTU probe.                   |
| `START_DELAY_S`                   | `20`    | Delay before the first send, giving downstream nodes time to finish booting before traffic starts.   |

No behavior is compiled into the image: role, addressing, and rate are all read from the process environment at start-up, consistent with the project's no-hardcoded-tunables principle.

---

# Node mapping — `NEXT_HOP_*` mirrors the real topology

| Node        | Role   | Env set                                                                            |
| ----------- | ------ | ---------------------------------------------------------------------------------- |
| Bench `.10` | source | `ROLE=bench`, `NEXT_HOP_HOST=10.99.0.11`, `NEXT_HOP_PORT=47100`                    |
| V2X `.11`   | relay  | `ROLE=v2x`, `LISTEN_PORT=47100`, `NEXT_HOP_HOST=10.99.0.12`, `NEXT_HOP_PORT=47200` |
| ADA `.12`   | relay  | `ROLE=ada`, `LISTEN_PORT=47200`, `NEXT_HOP_HOST=10.99.0.13`, `NEXT_HOP_PORT=47300` |

- The `NEXT_HOP_*` values are copies of the addresses each node already carries for its production peer — for example, the bench's `NEXT_HOP_HOST` mirrors its existing `V2X_ECU_HOST` — so the test exercises the real addressing rather than a parallel set of constants.
- ADA's `LISTEN_PORT` is added alongside its existing `V2X_LISTEN_PORT` key rather than renamed, so the node's real configuration remains intact once the smoke-test values are reverted.
- `capabilities: ["NET_RAW"]` is set identically on all three nodes.
- Values are entered per node in the Nydus Inspector — the same panel that sets `image`, `command`, and `capabilities` — and read by `netcheck.py` at start-up.

---

# Sending and receiving — the socket calls

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

- `connect()` on a UDP socket performs a kernel route lookup and transmits nothing; an unreachable peer raises immediately.
- The receiver binds to `0.0.0.0`, never a named interface, since the bridge NIC's name is not guaranteed by the platform; `recvfrom()` blocks until a datagram arrives.
- A node with both `LISTEN_PORT` and `NEXT_HOP_HOST` set — V2X and ADA — relays every received datagram onward through the same `sendto()` call the sender loop uses.

---

# Payload — a stamped string, not an encoded CPM

- The payload is plain ASCII text — `f"seq={i}|{ROLE}".encode()`, optionally padded with `PAD` filler bytes for the MTU probe. No serialization format and no codec are involved.
- Each relay hop appends its own role to the payload rather than re-encoding it: `data + f"|{ROLE}".encode()`. The accumulated `|bench|v2x` suffix observed at ADA is the chain-of-custody evidence behind pass criterion C5.
- The payload is deliberately unencoded: this test isolates transport from the message codec. Real CPM traffic is ASN.1 UPER, produced through the V2X ECU's Vanetza codec seam — a separate, already-frozen contract this test does not exercise.

---

# Traffic capture — `tcpdump` gated by `NET_RAW`

- `capture.sh` runs in the background, started by `entrypoint.sh` independently of the check program, so a capture failure never blocks the connectivity test.
- It probes its own privilege first: `tcpdump -D` succeeds only if the process can enumerate capture-capable interfaces, which requires the `CAP_NET_RAW` capability the platform grants through the node's `capabilities: ["NET_RAW"]` config field.
- On success it runs `tcpdump -i any -n -l -tttt "${CAPTURE_FILTER:-udp}"`, piping every line through `sed 's/^/[CAP] /'` so packet lines are distinguishable in the mixed log stream.
- On failure it falls back to polling `/proc/net/dev` packet counters, which need no elevated privilege — pass criterion C4 is then met at counter level instead of packet level.

---

# Entry point — `entrypoint.sh` starts both programs

- `entrypoint.sh` is the image's `CMD` (`Dockerfile`: `CMD ["./entrypoint.sh"]`); the container runtime invokes it the instant the pod starts — no manual exec, satisfying the self-run guarantee every run subtask requires.
- It starts `capture.sh` first, in the background (`&`), so wire capture begins before any application traffic exists.
- It then replaces itself with the check program via `exec python3 -u netcheck.py`: `exec` hands the process's PID to Python rather than spawning a child, so the container's lifecycle — and its restart count — tracks the check program directly.
- Every step logs a `[BOOT]` line first, so a log with no `[BOOT] … exec python3` entry is itself diagnostic: it means the interpreter failed to start.

---

<!-- _class: lead -->

![bg](../assets/bg-navy-motif.png)

# 03 · Tested object

---

# What actually ran

- **Three CarSky Container nodes**, one throwaway image (`m1-netcheck:latest`) on all three — bench, V2X, ADA — differing only by env vars (`ROLE`, `LISTEN_PORT`, `NEXT_HOP_HOST/PORT`), exactly how the real ECU nodes are configured.
- **`arm64` machine.** The cluster's runnable evidence is uniformly `aarch64` — every node pulls a `linux/arm64` image, no exceptions.
- **`NET_RAW` capability**, granted explicitly in each node's config — real `tcpdump` capture, not the `/proc/net/dev` counter fallback (this answered open item **O2**).
- **Ethernet-bridge network** — all four `ethernet` pins (`10.99.0.10`–`.13`) wired as a **star/spoke topology** into one hub `eth-bridge` node: a **single flat domain**, `10.99.0.0/24`, shared by bench, V2X, ADA, and IVI.
- **The IVI node is out of scope for the container test** — it's a Skycraft (AAOS) node running an Android VM artifact, not a container image, so it cannot run `netcheck.py` at all.

---

<!-- _class: lead -->

![bg](../assets/bg-navy-motif.png)

# 04 · Testing agent

---

# What the AI did

- **Credentials — two, both prompted from the human.** A CarSky API key (`a8k_…`) for REST calls (deploy status, node phases, logs), and a Zot registry key (`CARSKY_ZOT_API_KEY`, `zak_…`) as a GitHub Actions secret for the image push. Neither was assumed or guessed — the agent's login procedure stops and asks when no key is on hand.
- **Wrote the test artifact.** All four `tools/netcheck/` files (`Dockerfile`, `entrypoint.sh`, `capture.sh`, `netcheck.py`), the CI job (`.github/workflows/phase0-ci.yml`, `netcheck-image`), and pushed both as normal commits (`6.0.8.1`, `5.0.8.2`).
- **Verified over REST, not by eye.** `GET /deployments/{roomId}/status`, `.../nodes` (per-node phase), `.../logs/{nodeKey}?container=user|sidecar`, `.../pods`, and `GET /devices` to resolve the target Room — the same calls [[carsky-room-diagnostics]] uses to diagnose a stuck deploy.

---

# Where the AI hit a wall — the IVI

- **No eyes on the Skycraft node.** Screenshot (`GET /vms/{roomId}/{nodeKey}/screenshot`), UI-tree (`.../accessibility`), and ADB shell (`.../shell`) all return **502 — unavailable on this deployment**.
- **Direct hop-3 proof was out of reach.** The strongest check — an ADB shell `nc -u -l -p 47300` listener on the AAOS guest — needs the shell API this deployment doesn't serve.
- **Indirect check instead.** ADA's own `[TX] … relayed to 10.99.0.13:47300` log line plus its `[CAP]` tcpdump entry prove the datagram left the bridge for the IVI's address — proof the packet was *sent*, not that it was *received*.
- **The gap closes later, not now.** Once the IVI's real listener for ADA messages lands in Phase 5, hop 3 gets a direct check and this workaround retires.

---

<!-- _class: lead -->

![bg](../assets/bg-navy-motif.png)

# 05 · Human in the loop

---

# Five steps only a person could do

- **Create & configure the blueprint** — the REST `addPin` enum has no `ETHERNET`, and `PATCH /nodes/{id}` 404s, so wiring the four pins and setting each node's `image`/`command`/`capabilities`/`env` is UI-only in Nydus. *Both would have been AI's job if that gap didn't exist.*
- **Check the test script** — reviewed the netcheck tool and CI job before they went live, ahead of any registry push.
- **Create the key, hand it to the AI when prompted** — the Zot API key and the CarSky API key are both minted by a human and handed over on request; the agent never generates or guesses them.
- **Manual deployment** — click **New Deployment** in Nydus once node config is set (M9); the platform allows only 2 concurrent deployments, so teardown is a human call too.
- **Manual log check** — read each node's **View Log** against the expected pattern (M10) — the step that supplies the IVI's only evidence, since the AI's REST path can't reach that node.

---

<!-- _class: lead -->

![bg](../assets/bg-navy-motif.png)

# 06 · Results

---

# C5 — the chain, proven by its own stamps

Each hop appends its role to the payload — the accumulated stamp at ADA is the proof the datagram **transited every node**, not just arrived from somewhere.

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
| **C1** | All nodes `Running` | 5/5 nodes, restart count 0, stable across a 10-minute window                  |
| **C2** | Zero errors         | 0 `[ERR]` lines across bench / V2X / ADA                                      |
| **C3** | Live per-node log   | 100 lines each, streaming                                                     |
| **C4** | Wire capture        | 80 / 66 / 66 `[CAP]` lines on `10.99.0.x` — real `tcpdump`, `NET_RAW` honored |
| **C5** | Chain proven        | ADA log carries `body=seq=288\|bench\|v2x`                                    |

**Deployed as** `trial2_minh_netcheck` · Room `27gs83k3oeju2mbywu1j8` · deploy alone started every script — **no manual exec used anywhere**, meeting the self-run guarantee.

---

# Open items carried forward

- **Registry host resolved (O1)** — `registry.hackathon-2.carsky.io` is now standing for every ECU image, not just this test.
- **Platform tenancy gap** — `GET /blueprints` returns every owner's blueprints unfiltered, including another team's private topology and inline scripts. Reported to BTC; unrelated to M1 delivery.
- **Unreliable REST routes** — `restart` returns 500, `container-exec` returns 503; prefer teardown + redeploy over either.
- **O3 (MTU headroom)** — optional `PAD=1400` probe not run; feeds the CPM message size budget later.
- **O4 (AAOS `nc` availability)** — still unknown; the indirect ADA-side check stood in for it this run.

**Milestone-1 impact:** Phase 0's last acceptance box — blueprint topology documented + validated — is now closed. Phase 1 (comms bring-up) is unblocked.

---

<!-- _class: lead -->
<!-- _paginate: false -->

![bg](../assets/bg-fpt-tower.jpg)

# Thank you!

**Phase 0 Smoke Test — Baseline Blueprint Connectivity** · FPT Hackathon 2026
