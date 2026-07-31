# Deploying a Blueprint on CarSky — End-to-End Walkthrough

Worked example: the **netcheck** connectivity test ([tools/netcheck/](../../tools/netcheck/)), from source file to running Room. Every manual step **M1–M12** of [baseline-connectivity-smoke-test.md](../../plans/doc/research_notes/baseline-connectivity-smoke-test.md) is covered here in order; that note owns the test's *design* (why each check exists, pass criteria C1–C5), this guide owns the *doing*.

Use it as the template for deploying any container node — the ECU images follow the identical path with different images and env.

---

## 1. Introduction

Getting code onto CarSky is three hand-offs:

```
your code  ──build──▶  container image  ──push──▶  registry  ──pull──▶  CarSky node
 (git repo)            (GitHub Actions)              (Zot)              (Room)
```

Nothing is ever installed onto a node by hand. A node's config *names* an image; the platform pulls it when the Room deploys. Change behaviour → edit config and redeploy. Change code → rebuild, push, redeploy.

The netcheck example proves the network works before any ECU code exists: one image on three nodes, each playing a different role via environment variables, relaying a datagram bench → V2X → ADA → IVI.

---

## 2. Tools and concepts

### 2.1 Docker

**Docker** packages an application with everything it needs to run — OS libraries, runtime, dependencies — into one **image**. A running copy of an image is a **container**. Because the image carries its own environment, it runs identically on a laptop, a CI runner, and a cloud cluster; this is what removes "works on my machine".

A **Dockerfile** is the recipe. Ours ([tools/netcheck/Dockerfile](../../tools/netcheck/Dockerfile)) picks a base OS, installs Python and tcpdump, copies three scripts in, and declares what runs at start.

> Reference: [Docker overview](https://docs.docker.com/get-started/docker-overview/) · [Dockerfile reference](https://docs.docker.com/reference/dockerfile/)

### 2.2 GitHub Actions

**GitHub Actions** runs commands on GitHub's servers when something happens in the repository. We use it because the developer machine has no Docker or Linux — GitHub's Linux machines build and test instead.

### 2.3 Workflow, YAML, events, jobs, runners

A **workflow** is a YAML file in `.github/workflows/`. Ours is [phase0-ci.yml](../../.github/workflows/phase0-ci.yml).

| Concept | Meaning | In our file |
|---|---|---|
| **Event** | What triggers the workflow | `on: push` — every push to any branch |
| **Job** | A named group of steps; jobs run in parallel by default | `netcheck-image`, `v2x-core-build`, `ivi-unit-tests`, … |
| **Step** | One command or reusable action inside a job | "Build netcheck image", "Push to CarSky Zot registry" |
| **Runner** | The machine executing a job | `runs-on: ubuntu-latest` |
| **Secret** | Encrypted value injected at run time, never printed | `secrets.CARSKY_ZOT_API_KEY` |

Each job starts on a clean machine, so it checks the code out first (`actions/checkout`).

> Reference: [GitHub Actions documentation](https://docs.github.com/actions) — workflow syntax, contexts, and expressions in full.

### 2.4 Zot — the container registry

**Zot** is the image warehouse CarSky runs. Upload an image once; any node can pull it by name. Ours: `https://registry.hackathon-2.carsky.io/`.

**Credentials.** Sign in to the Zot web UI through **A8 Keycloak** (single sign-on), then create an **API key** (`zak_…`, shown once). That key is the *password* for `docker login`; the username is the registry account. Full procedure: [zot-registry-api-key.md](zot-registry-api-key.md).

**Host caveat:** use `registry.hackathon-2.carsky.io`. The `registry.carsky.io` host answers 502 and every reference to it fails as if the image did not exist.

**How the push works from GitHub Actions** — three commands, run by the `netcheck-image` job:

```
docker login <registry-host> -u <account> --password-stdin   # key supplied from the secret
docker tag  m1-netcheck:latest  <registry-host>/m1-netcheck:latest
docker push <registry-host>/m1-netcheck:latest
```

The key lives only in GitHub Secrets and is never written to the repository.

### 2.5 CarSky — platform model

CarSky simulates a vehicle's electronic architecture in the cloud.

**Two different credentials, easily confused:**

| Credential | Format | Used for |
|---|---|---|
| **Keycloak login** | email + password | Signing in to the CarSky and Zot web UIs |
| **CarSky API key** | `a8k_…` | REST API calls (blueprints, deployments, logs) |
| **Zot API key** | `zak_…` | `docker login` to the registry only |

**The object model:**

| Term | What it is |
|---|---|
| **Blueprint** | The design of one vehicle: which nodes exist, how they are wired. Edited on the Nydus canvas. Not running. |
| **Node** | One simulated ECU or piece of bench equipment inside a blueprint. Types used here: **Container** (runs an image), **Skycraft** (runs an Android VM), **Ethernet Bridge** (the virtual network). |
| **Pin / edge** | A node's connector and the wire joining it to the bridge. Our four role nodes each have one `ethernet` pin wired to the bridge — a star. |
| **Device** | The Kubernetes resource pool a deployment runs on. **Not** an ECU — just the target machine pool chosen at deploy time. |
| **Deployment / Room** | A running instance of a blueprint. One blueprint can be deployed several times; the account allows 2 concurrent deployments. |

**What Zot provides to CarSky:** the images. A Container node's `image` field is a Zot address; at deploy time the platform pulls that image and starts it. If the image is missing, or the host is wrong, the node hangs in `Provisioning` with `trying and failing to pull image`.

---

## 3. Step-by-step: deploying netcheck (M1–M12)

### M1 — Write the application code

[tools/netcheck/](../../tools/netcheck/) holds four files: `netcheck.py` (send / receive / relay), `capture.sh` (tcpdump), `entrypoint.sh` (starts capture in the background, netcheck in the foreground), and `Dockerfile`.

Two rules make one image serve three roles:

- **Nothing about the topology is in the code** — addresses, ports, and role come from environment variables.
- **The container starts the test itself**, so a deploy alone produces evidence; no shell session is ever needed.

### M2 — Store the registry credential as a GitHub secret

1. Create the Zot API key ([zot-registry-api-key.md](zot-registry-api-key.md)).
2. GitHub repository → **Settings → Secrets and variables → Actions → New repository secret**.
3. Name `CARSKY_ZOT_API_KEY`, value `zak_…`.

The registry account is not secret and lives in the workflow (`REGISTRY_USER`); override it with the `CARSKY_REGISTRY_USER` repository *variable* if the account changes.

### M3 + M4 — Build and push (automatic)

Both are done by the `netcheck-image` job on every push — no local Docker required.

**Verify:** GitHub → **Actions** → newest **phase0-ci** run → job **netcheck-image**:

- *"Build netcheck image"* green → **M3 done**.
- *"Push to CarSky Zot registry"* prints `pushed …/m1-netcheck:latest` → **M4 done**. If it prints `secret not set`, M2 is incomplete.

**Independent check:** the image appears in the Zot UI image list, or via the registry API `GET /v2/_catalog`.

### M5 — Choose the blueprint

Nydus → blueprint list → open the target (here `trial2_minh_netcheck`).

Working on a **clone** keeps the known-good baseline untouched — recommended. Note that deploying also creates a snapshot named `<name>-deploy`; **always edit the original**, never the snapshot.

### M6 — Check the wiring ⚠️

Confirm each of the four role nodes has one `ethernet` pin wired to the Ethernet Bridge, with addresses `10.99.0.10` (bench), `.11` (V2X), `.12` (ADA), `.13` (IVI).

> **Warning — do not add nodes or pins here.** The REST API cannot create `ETHERNET` pins, and a JSON import silently drops them. If a clone lost its pins, re-draw the four edges by hand on the canvas. On our blueprint the wiring is already correct: nothing to do.

### M7 — Configure the three container nodes 🔑

**The most important step.** Click a node, edit in the Inspector, click the canvas to commit.

![Nydus Inspector — V2X ECU node configuration](images/nydus-inspector-v2x-ecu.png)

> Capture that screenshot **only after the values are verified correct** (§4). The first run's screenshot showed `NEXT_HOP_HOST = 10.99.0.2` and `ROLE = V2X` — both wrong — and would teach the mistakes it was meant to prevent.

**Common to all three nodes:**

| Field | Value | Explanation |
|---|---|---|
| Image | `registry.hackathon-2.carsky.io/m1-netcheck:latest` | The Zot address pushed in M4. Host must be `hackathon-2`; `registry.carsky.io` returns 502 and the pull fails. |
| Command | `./entrypoint.sh` | Overrides the container entrypoint. Relative to the image's workdir `/app` — `/entrypoint.sh` (absolute) does not exist and the container dies at start. May be left empty: the Dockerfile already defaults to it. |
| Args | *(empty)* | Not used. |
| Capabilities | `NET_RAW` | Linux privilege for opening raw sockets, required by `tcpdump` in `capture.sh`. Without it capture degrades to packet counters and criterion C4 weakens. |
| Exposed Ports | *(empty)* | Only needed to reach a container from outside the Room. Node-to-node traffic on `10.99.0.x` needs nothing. |
| Pins / Part Prefix | *(unchanged)* | Set by the baseline; touching them breaks the wiring or the log routing. |

**Per node — environment variables.** Names are read verbatim by [netcheck.py](../../tools/netcheck/netcheck.py); a misspelling is silently ignored, and a wrong address produces `[ERR] no route`.

| Node | Field | Value | Explanation |
|---|---|---|---|
| **Bench** `10.99.0.10` | `ROLE` | `bench` | Read as `ROLE`; labels every log line and is appended as the stamp `\|bench`. Lowercase, to match the expected log. |
| | `NEXT_HOP_HOST` | `10.99.0.11` | Read as `NH`; destination of `sendto()`. The V2X node's pin address. |
| | `NEXT_HOP_PORT` | `47100` | Read as `NP`; must equal V2X's `LISTEN_PORT`. |
| | *(no `LISTEN_PORT`)* | — | The bench only sends. Setting it would make it a relay. |
| **V2X ECU** `10.99.0.11` | `ROLE` | `v2x` | Adds the `\|v2x` stamp proving the datagram transited this node (criterion C5). |
| | `LISTEN_PORT` | `47100` | Read as `LISTEN`; the receiver binds `0.0.0.0:47100`. Must equal the bench's `NEXT_HOP_PORT`. |
| | `NEXT_HOP_HOST` | `10.99.0.12` | ADA's pin address. **Type carefully — `10.99.0.2` is a different, non-existent host.** |
| | `NEXT_HOP_PORT` | `47200` | Must equal ADA's `LISTEN_PORT`. |
| **ADA ECU** `10.99.0.12` | `ROLE` | `ada` | Adds the `\|ada` stamp. |
| | `LISTEN_PORT` | `47200` | **Add this name.** The baseline's `V2X_LISTEN_PORT` is a different variable and is not read by this image — keep it, but add `LISTEN_PORT` alongside. |
| | `NEXT_HOP_HOST` | `10.99.0.13` | The IVI node. |
| | `NEXT_HOP_PORT` | `47300` | The IVI hop (see M10). |

Optional, defaults are fine: `HZ` (1 message/second), `PAD` (payload padding, for the MTU check), `START_DELAY_S` (20 s, so receivers are listening first), `CAPTURE_FILTER` (`udp`).

Leftover baseline variables (`ADA_ECU_HOST`, `SCENARIO_CONFIG`, `GATE_ENTER_M`, …) are ignored and harmless.

### M8 — Leave the IVI node alone

The Skycraft node runs an Android VM, not a container — it cannot run these scripts. It only needs its **VM image artifact** attached (artifact `AAOS`, version `0.0.1`, arch `aarch64` — [node-ivi-ecu.md](node-ivi-ecu.md)). Without it the deploy is rejected outright:

> `invalid blueprint: node 'IVI ECU': skycraft requires 'image' config with VM image artifact details`

### M9 — Deploy → criterion C1

**New Deployment** → name it → pick a **Device** from the list (any existing one; a Device is a resource pool, not an ECU — creating extras wastes the 2-deployment budget) → **Deploy**.

Wait until every node badge reads `Running` with restart count 0 — that is **C1**. The Android node takes longer than the containers.

![Deploy Blueprint dialog — deployment name and device selection](images/nydus-deploy-dialog.png)

*Stuck in `Provisioning`* almost always means the image could not be pulled: re-check the M7 image field, host, and that M4 actually pushed. Diagnosis procedure: [carsky-room-diagnostics](../../.claude/skills/carsky-room-diagnostics/SKILL.md).

### M10 — Read the logs → criteria C2–C5

Click each node → **View Log**. The programs are already running; nothing to type.

| Node | Expect |
|---|---|
| Bench | `[NET] bench route to 10.99.0.11:47100 OK, egress address 10.99.0.10` then `[TX] bench #0 …` |
| V2X | `[RX] v2x #1 from 10.99.0.10 … body=seq=0\|bench` then `[TX] v2x #1 relayed to 10.99.0.12:47200` |
| ADA | `[RX] ada #1 … body=seq=0\|bench\|v2x` — the accumulated stamps are **C5** |
| any | `[CAP] … IP 10.99.0.10.x > 10.99.0.11.47100: UDP` — tcpdump on the wire, **C4** |

Zero `[ERR]` lines is **C2**; a live, readable log per node is **C3**.

**The IVI hop (hop 3)** — the Android node has no APK yet, so verify it one of two ways, and record which was used:

1. **Preferred:** the node's **ADB Shell** widget → `nc -u -l -p 47300` (run `toybox` first to confirm `nc` exists). Traffic arrives within a second or two.
2. **Fallback:** ADA's `[TX] … relayed to 10.99.0.13:47300` plus its `[CAP]` line — proves the datagram was put on the wire, not that it was received.

### M11 — Optional: MTU headroom

Set `PAD=1400` on the bench node and redeploy. If large datagrams do not arrive while small ones do, bisect the value to find the ceiling — the bridge is a tunnelled fabric, so 1500 bytes is not guaranteed. Feeds the CPM message-size budget.

### M12 — Tear down

**Delete Deployment** when finished; the blueprint is untouched and redeployable. Only 2 deployments may run at once, so releasing one matters.

---

## 4. Mistakes already made — check these first

Every row below cost real time on the first run (2026-07-31). They are ordered by how expensive they were.

| # | Mistake | Symptom | Fix |
|---|---|---|---|
| 1 | **Deployed before doing M7.** The clone carried the baseline's ECU images (`registry.carsky.io/m1-v2x-ecu:latest` …), which do not exist yet. | All three container nodes stuck in `Provisioning`; the bridge and IVI reach `Running`. Log API reports `waiting to start: trying and failing to pull image`. | Apply M7 to all three nodes, delete the failed deployment, redeploy. |
| 2 | **Wrong registry host** — `registry.carsky.io` instead of `registry.hackathon-2.carsky.io`. | Identical to #1: the pull fails and the node never starts. | Use the `hackathon-2` host everywhere: CI, image tags, node config. |
| 3 | **Typo in an address** — `NEXT_HOP_HOST = 10.99.0.2` instead of `10.99.0.12`. | Node runs and logs, but `[ERR] no route to 10.99.0.2:47200`; the chain stops at that hop, so C5 never appears downstream. | Re-read each address digit by digit; they differ by one character. |
| 4 | **`ROLE` in uppercase** (`V2X` instead of `v2x`). | Runs, but log lines and the datagram stamp read `\|V2X`, so the expected `seq=0\|bench\|v2x` never matches and C5 cannot be confirmed by eye. | Lowercase: `bench`, `v2x`, `ada`. |
| 5 | **Absolute command path** — `/entrypoint.sh` instead of `./entrypoint.sh`. | Container exits immediately; restart count climbs. The script lives in the image workdir `/app`, not at the filesystem root. | Use `./entrypoint.sh`, or clear the field and let the image's own default run. |
| 6 | **IVI node missing its VM artifact.** | Deploy rejected outright: `skycraft requires 'image' config with VM image artifact details`. | Attach artifact `AAOS` v`0.0.1`, arch `aarch64` (M8). |
| 7 | **Renaming instead of adding on ADA** — the baseline's `V2X_LISTEN_PORT` is not read by this image. | ADA never binds a socket, so it receives nothing. | **Add** `LISTEN_PORT=47200`; leave the old variable in place. |
| 8 | **Editing the `-deploy` snapshot.** Deploying creates a copy named `<blueprint>-deploy`. | Edits appear to save but the next deploy ignores them. | Always edit the original blueprint. |

**Before deploying, verify by reading the config back** rather than trusting the Inspector's truncated fields — `GET /api/v1/blueprints/{id}` returns each node's stored `config` exactly, and catches #2, #3, #4 and #5 in one look.

## 5. Quick reference

| Thing | Value |
|---|---|
| Registry host | `registry.hackathon-2.carsky.io` |
| Image | `registry.hackathon-2.carsky.io/m1-netcheck:latest` |
| GitHub secret | `CARSKY_ZOT_API_KEY` (`zak_…`) |
| Node addresses | bench `.10` · V2X `.11` · ADA `.12` · IVI `.13` on `10.99.0.0/24` |
| Ports | bench→V2X `47100` · V2X→ADA `47200` · ADA→IVI `47300` |
| Pass criteria | C1 Running · C2 no `[ERR]` · C3 live log · C4 `[CAP]` capture · C5 accumulated stamps |

*Screenshots referenced above live in `requirements/car-sky-guide/images/`; capture them from Nydus when exporting this guide to slides.*
