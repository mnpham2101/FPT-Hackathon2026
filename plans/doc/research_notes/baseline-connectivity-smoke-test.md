# Baseline Blueprint — Node Connectivity Smoke Test

> **Status:** research note (scratch). Not authoritative — authority is [m1-cooperative-awareness.md](../../../documents/Requirements/m1-cooperative-awareness.md) R5/R6 and its [baseline topology](../../../documents/Requirements/m1-cooperative-awareness.md#baseline-propose-topology); platform mechanics are [carsky-4-node-blueprint.md](../../../requirements/car-sky-guide/carsky-4-node-blueprint.md).

## 1. Objective

Prove a datagram travels **bench `10.99.0.10` → V2X `.11` → ADA `.12` → IVI `.13`** on the real ports (`47100`/`47200`/`47300`), before any ECU code exists.

Method: one small image, baked with two programs, deployed to the three Container nodes. The node starts → the programs run by themselves → the user clicks the node → **View Log** → reads the result. No interactive session, no API scripting.

| Program | Started by | Job |
|---|---|---|
| `capture.sh` | entrypoint, **background** | traffic capture — `tcpdump` on the wire, independent of the app |
| `netcheck.py` | entrypoint, **foreground** | the network check — send / receive / relay, and log every datagram |

## 2. Pass criteria

| # | Check | Where seen |
|---|---|---|
| C1 | Node is functional: reaches **Running** and stays there (restart count stays 0) | Deployment Viewer |
| C2 | No error: **zero `[ERR]` lines** in any node's log | View Log |
| C3 | Log is observable and live per node | View Log |
| C4 | Traffic captured: `[CAP]` tcpdump lines show the UDP datagrams on the wire | View Log |
| C5 | Chain proven: the token reaching the last hop carries the `\|v2x\|ada` stamps each relay added | View Log on ADA/IVI |

C5 is the real objective — it is what distinguishes "the chain works" from "three nodes each happen to be alive".

## 3. Why not `ping`

Containers get no `NET_RAW` by default (the platform doc calls it out as *"required so the container may open raw sockets"*), so ICMP can fail on a perfectly healthy UDP path. `netcheck.py` instead uses **`connect()` on a UDP socket** — that forces a kernel route lookup and sends no packet, so an unreachable peer fails instantly and loudly, with no ICMP involved.

`NET_RAW` is granted explicitly in the node config (§ 6, step M7) so that `tcpdump` works — verified as a real, honored field: CarSky's own [blueprint-KIS.json](../../../requirements/development-platform-doc/blueprint-KIS.json) has a container node carrying `"capabilities": ["NET_RAW", "NET_ADMIN", …]` flat alongside `image`/`env`. If it turns out not to be granted, `capture.sh` falls back to `/proc/net/dev` packet counters, which need no privilege at all — C4 is then met at counter level instead of packet level.

## 4. Tool implementation

### 4.1 Where the code goes

Create a new folder **`tools/netcheck/`** at the repo root and write all four files below into it.

It is deliberately **not** one of the four node folders. `Scenario_Player/`, `V2X_ECU/`, `ADA_ECU/`, and `IVI_ECU/` hold node product code only ([node-code-layout.md](../../../.claude/rules/node-code-layout.md)); this tool is throwaway test equipment that is deleted, or left unbuilt, once the real ECU images exist.

```
tools/netcheck/
├── Dockerfile        # recipe that packages the three files below into one image
├── entrypoint.sh     # starts capture.sh in the background, then netcheck.py in the foreground
├── capture.sh        # program 1 — traffic capture (tcpdump, or /proc counters as fallback)
└── netcheck.py       # program 2 — the network check (send / receive / relay)
```

### 4.2 Program 1 — `capture.sh`

Captures traffic on the wire, independently of whether the check program works. Prefixes every line `[CAP]` so it is distinguishable in the mixed log stream.

```sh
#!/bin/sh
if tcpdump -D >/dev/null 2>&1; then
  echo "[CAP] tcpdump active, filter: ${CAPTURE_FILTER:-udp}"
  tcpdump -i any -n -l -tttt "${CAPTURE_FILTER:-udp}" 2>&1 | sed 's/^/[CAP] /'
else
  echo "[CAP] no NET_RAW - falling back to /proc/net/dev packet counters"
  while :; do
    awk 'NR>2 {gsub(":",""); if ($3+$11 > 0) printf "[CAP] %s rx_pkts=%s tx_pkts=%s\n", $1, $3, $11}' /proc/net/dev
    sleep "${CAPTURE_INTERVAL_S:-5}"
  done
fi
```

### 4.3 Program 2 — `netcheck.py`

The network check. One file serves all three roles; which role it plays is decided **only** by the environment variables the node config supplies (§ 6, step M7) — nothing about the topology is compiled in (CLAUDE.md governing principle 5).

```python
#!/usr/bin/env python3
"""Baseline connectivity check. Env: ROLE, LISTEN_PORT?, NEXT_HOP_HOST?, NEXT_HOP_PORT?, HZ, PAD, START_DELAY_S."""
import os, socket, threading, time

ROLE   = os.environ.get("ROLE", "node")
LISTEN = os.environ.get("LISTEN_PORT")                  # relay/sink nodes only
NH, NP = os.environ.get("NEXT_HOP_HOST"), os.environ.get("NEXT_HOP_PORT")
HZ     = float(os.environ.get("HZ", "1"))               # 1 Hz: log stays readable and live
PAD    = int(os.environ.get("PAD", "0"))                # payload padding, for the MTU check
DELAY  = float(os.environ.get("START_DELAY_S", "20"))   # let the downstream nodes come up first

def log(tag, msg): print(f"[{tag}] {ROLE} {msg}", flush=True)

def route_check():
    """Reachability without ICMP: connect() forces a route lookup and sends nothing."""
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        s.connect((NH, int(NP)))
        log("NET", f"route to {NH}:{NP} OK, egress address {s.getsockname()[0]}")
    except OSError as e:
        log("ERR", f"no route to {NH}:{NP} - {e}")
    finally:
        s.close()

def receiver():
    rx = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    rx.bind(("0.0.0.0", int(LISTEN)))     # never bind a named interface - the bridge NIC's name is not guaranteed
    log("NET", f"listening on udp/{LISTEN}")
    fwd, n = socket.socket(socket.AF_INET, socket.SOCK_DGRAM), 0
    while True:
        data, src = rx.recvfrom(65535); n += 1
        log("RX", f"#{n} from {src[0]}:{src[1]} len={len(data)} body={data[:96].decode('ascii','replace')}")
        if NH:                            # relay: stamp, then forward
            out = data + f"|{ROLE}".encode()
            fwd.sendto(out, (NH, int(NP)))
            log("TX", f"#{n} relayed to {NH}:{NP} len={len(out)}")

def sender():
    time.sleep(DELAY)
    tx, i = socket.socket(socket.AF_INET, socket.SOCK_DGRAM), 0
    while True:                           # runs forever, so the log is alive whenever it is opened
        out = f"seq={i}|{ROLE}".encode() + b"x" * PAD
        try:
            tx.sendto(out, (NH, int(NP)))
            log("TX", f"#{i} to {NH}:{NP} len={len(out)}")
        except OSError as e:
            log("ERR", f"send failed - {e}")
        i += 1; time.sleep(1 / HZ)

if NH: route_check()
if LISTEN: threading.Thread(target=receiver, daemon=True).start()
if NH and not LISTEN: sender()            # source node
while True: time.sleep(3600)              # keep the pod Running so View Log stays open
```

### 4.4 `entrypoint.sh`

This is what makes the node *functional*: the container has something to run, so the pod stays up and produces a log. It launches the capture program in the background and the check program in the foreground.

```sh
#!/bin/sh
set -u
echo "[BOOT] role=${ROLE:-unset} listen=${LISTEN_PORT:-none} next=${NEXT_HOP_HOST:-none}:${NEXT_HOP_PORT:-none}"
./capture.sh &                  # capture runs alongside, independent of the traffic program
exec python3 -u netcheck.py     # foreground = the pod's lifetime
```

### 4.5 `Dockerfile`

The Dockerfile is the recipe that turns the three files into the deployable image: it picks a base OS, installs Python and tcpdump, copies the scripts in, and names the entrypoint.

```dockerfile
FROM alpine:3.20
RUN apk add --no-cache python3 tcpdump
WORKDIR /app
COPY netcheck.py capture.sh entrypoint.sh ./
RUN chmod +x capture.sh entrypoint.sh
CMD ["./entrypoint.sh"]
```

Docker references: [Dockerfile syntax](https://docs.docker.com/reference/dockerfile/) for the instructions above · [Car-Sky-Platform.html](../../../requirements/development-platform-doc/Car-Sky-Platform.html) § *"Log In to Zot Registry & Get API Key"* for the platform's own `docker login` / `tag` / `push` walkthrough · [node-scenario-player.md](../../../requirements/car-sky-guide/node-scenario-player.md#build--push-the-image) for the same three commands in this project's shape.

## 5. How the tool reaches the nodes

**Nothing is installed onto a node by hand.** The flow is: build **one** image → push it **once** to the CarSky registry → each node's blueprint config names that image → the platform pulls it when the Room deploys.

- **One image, three nodes.** All three Container nodes reference the *same* tag. Do not build three images. The bench, V2X, and ADA nodes differ only by the env vars in their node config, which is exactly how the real ECU nodes are configured.
- **Build once, redeploy to change behaviour.** Editing env (e.g. `PAD=1400`) needs no rebuild — change the node config and redeploy. Editing the scripts needs a rebuild, a push under the same tag, and a redeploy.
- **The IVI node is not part of this.** It is a Skycraft (AAOS) node: it runs the Android VM artifact, not a container image, so it cannot run these scripts at all (§ 7).

## 6. Manual steps

Everything the user does, start to finish. Steps M1–M4 happen once on a machine with Docker; M5–M12 happen in the Nydus UI.

| # | Step |
|---|---|
| **M1** | Create `tools/netcheck/` and write the four files from § 4 into it. |
| **M2** | Confirm the registry hostname with the organizers, then log in: `docker login <registry-host> -u <username>`, using your Zot API key as the password. |
| **M3** | Build the image: `docker build -t m1-netcheck:latest tools/netcheck/` — add `--platform linux/<arch>` for the cluster's architecture if your machine differs. |
| **M4** | Tag and push it: `docker tag m1-netcheck:latest <registry-host>/m1-netcheck:latest` then `docker push <registry-host>/m1-netcheck:latest`. |
| **M5** | Open the baseline blueprint in Nydus. Optionally clone it first (`trial2_minh-netcheck`) so the committed topology stays untouched — if the clone loses its `ethernet` pins, re-wire the four edges to the bridge by hand. |
| **M6** | Check that the four `ethernet` pins and their four edges into the bridge are present. **Do not add a new node** for this test — the REST `addPin` enum has no `ETHERNET`, so a new node cannot be wired to the bridge except by hand in the canvas. |
| **M7** | In each Container node's Inspector, set `image`, `command`, `capabilities`, and the env vars per the table below. This is a UI step: node config cannot be edited over REST (`PATCH /nodes/{id}` 404s, and `/batch` is add-only). |
| **M8** | Leave the IVI node's AAOS artifact reference as it is, and confirm it has one — without it the Room never reaches all-nodes-ready. |
| **M9** | Click **New Deployment**, then wait until every node badge reads `Running` and the restart count stays 0 — that is **C1**. |
| **M10** | Click each node → **View Log** and read it against § 6.2 — that covers **C2–C5**. |
| **M11** | *(optional)* Set `PAD=1400` on the bench node and redeploy to check MTU headroom, then bisect if 1400 does not arrive while small datagrams do. |
| **M12** | Click **Delete Deployment** when finished — the account allows only 2 concurrent deployments. |

### 6.1 Node config for M7

| Node | `image` | `command` | env |
|---|---|---|---|
| Bench `.10` | `<registry-host>/m1-netcheck:latest` | `["./entrypoint.sh"]` | `ROLE=bench`, `NEXT_HOP_HOST=10.99.0.11`, `NEXT_HOP_PORT=47100` |
| V2X `.11` | same | same | `ROLE=v2x`, `LISTEN_PORT=47100`, `NEXT_HOP_HOST=10.99.0.12`, `NEXT_HOP_PORT=47200` |
| ADA `.12` | same | same | `ROLE=ada`, `LISTEN_PORT=47200`, `NEXT_HOP_HOST=10.99.0.13`, `NEXT_HOP_PORT=47300` |

Plus `"capabilities": ["NET_RAW"]` on all three, flat in `config` next to `image` (§ 3).

- The next-hop values are copies of the addresses already in each node's blueprint env, so the test exercises the real addressing rather than a parallel set of constants.
- V2X already carries `LISTEN_PORT=47100`. ADA's existing key is `V2X_LISTEN_PORT` — **add** `LISTEN_PORT=47200` alongside it rather than renaming, so the real config stays intact.

### 6.2 What a passing log looks like (M10)

**Bench** — boot, capture starts, route check, first send:

```
[BOOT] role=bench listen=none next=10.99.0.11:47100
[CAP]  tcpdump active, filter: udp
[NET]  bench route to 10.99.0.11:47100 OK, egress address 10.99.0.10
[TX]   bench #0 to 10.99.0.11:47100 len=12
[CAP]  … IP 10.99.0.10.51234 > 10.99.0.11.47100: UDP, length 12
```

The `[NET] … egress address 10.99.0.10` line is the address-binding check: it proves the bridge NIC is the one carrying the traffic, with no need to inspect interfaces.

**V2X** — receives from the bench and relays onward:

```
[RX] v2x #1 from 10.99.0.10:51234 len=12 body=seq=0|bench
[TX] v2x #1 relayed to 10.99.0.12:47200 len=16
```

**ADA** — the `|v2x` stamp is **C5**: the datagram provably transited the V2X node rather than arriving some other way.

```
[RX] ada #1 from 10.99.0.11:40112 len=16 body=seq=0|bench|v2x
[TX] ada #1 relayed to 10.99.0.13:47300 len=20
```

Traffic is continuous at 1 Hz and the bench waits `START_DELAY_S=20` before its first send, so the log is alive and complete whenever it is opened — nothing has to be caught in the moment.

## 7. The IVI hop

The IVI is an AAOS guest: it cannot run `netcheck.py`, and its R4 UDP listener (`R4ListenerService`, a Phase 5 deliverable) is not built yet. Options, strongest first:

1. **ADB Shell widget** on the Skycraft node → `nc -u -l -p 47300` (run `toybox` first to confirm the applet exists). Direct proof of hop 3.
2. **ADA-side evidence** — ADA's `[TX] … relayed to 10.99.0.13:47300` plus its `[CAP]` tcpdump line showing the datagram on the wire. Proves the packet was sent onto the bridge, not that it was received.
3. **Wait for the APK.** Once `4.5.1.3` lands, hop 3 is verified by the real R4 path and this test retires.

Hops 1 and 2 are fully proven by the container logs regardless — record which of the three was used for hop 3.

## 8. Troubleshooting network issue

| Symptom | Cause | Action |
|---|---|---|
| Node never leaves Pending / `ImagePullBackOff` | wrong registry host (M2) | fix `image`, re-push |
| Restart count climbing | script crashed at start | read the log — `netcheck.py` keeps the pod alive on purpose, so a restart means the entrypoint itself failed |
| `[ERR] no route to …` | pin not wired to the bridge, or wrong address | re-check the node's `ethernet` pin and edge in the canvas |
| `[NET] … egress address` shows an unexpected IP | traffic is leaving via the K8s CNI NIC, not the bridge | re-check the pin's `properties.address` |
| Sender logs `[TX]`, receiver logs nothing | port mismatch, or the receiving node started late | compare `NEXT_HOP_PORT` against the peer's `LISTEN_PORT`; raise `START_DELAY_S` |
| `[CAP] no NET_RAW` | `capabilities` not applied | C4 falls back to counter level; accept, or fix the node config |

## 9. Open items

| # | Item | Closes at |
|---|---|---|
| O1 | Correct registry host — the baseline says `registry.carsky.io`, which returns **502** live, while `registry.hackathon-2.carsky.io` answers with a `401` auth challenge. Affects the real ECU images too, not just this test. | M2, before the first push |
| O2 | Does the flat node config honor `capabilities`? | M10 — a `[CAP] tcpdump active` line answers it |
| O3 | Bridge MTU ceiling — the bridge is a tunnelled userspace fabric, so 1500 is not guaranteed | M11 — feeds the R1 CPM size budget |
| O4 | Does the AAOS guest have `nc`? | § 7 option 1 |

Also owed to the committed guides, found while researching this note: [carsky-4-node-blueprint.md §4 step 10](../../../requirements/car-sky-guide/carsky-4-node-blueprint.md#4-steps) refers to a *"platform packet-capture facility"* that does not exist (there is no pcap endpoint anywhere — capture is in-container, as above), and its step 3 sets `bridgeMode`/`subnet` on the bridge node although every real export has bridge `config: null`.
