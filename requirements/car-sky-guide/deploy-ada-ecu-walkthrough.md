# ADA ECU Build, Deploy, and Acceptance Walkthrough

This is the authoritative procedure for building the ADA ECU container, publishing it to the custom CarSky registry, deploying it in a Room, and collecting end-to-end acceptance evidence. Node configuration and pin facts remain authoritative in [node-ada-ecu.md](node-ada-ecu.md). The full topology remains authoritative in [carsky-4-node-blueprint.md](carsky-4-node-blueprint.md).

## 1. Prerequisites

### 1.1 Build machine

- Use a repository clone containing `ADA_ECU/Dockerfile`, `ADA_ECU/media/ego-b-occluding-c.mp4`, and `ADA_ECU/models/yolo11n.onnx`.
- Install Docker Desktop with Buildx support.
- Start Docker Desktop before running Docker commands.
- Install Python 3 with the `jsonschema` package for the event-log checker.
- Install Wireshark to inspect the extracted capture.

### 1.2 Platform access

- Obtain a Keycloak account for [the custom CarSky tenant](https://hackathon-2.carsky.io).
- Obtain registry access and create a Zot API key as described in [zot-registry-api-key.md](zot-registry-api-key.md).
- Use the registry host `registry.hackathon-2.carsky.io`.
- Obtain access to a blueprint containing the V2X ECU, ADA ECU, IVI ECU, Scenario Player, and Ethernet Bridge described in [carsky-4-node-blueprint.md](carsky-4-node-blueprint.md).
- Ensure the IVI application is installed and listening for ADA warning messages on UDP port `47300`; follow [deploy-ivi-hmi-walkthrough.md](deploy-ivi-hmi-walkthrough.md) when it is not installed.

### 1.3 Deliverable prerequisite

The ADA deliverable must provide all of the following before this procedure starts:

- A C++ runtime that receives V2X object JSON over UDP port `47200`.
- A Python detector that reads the packaged video and emits own-sensor vehicle B tracks.
- The packaged video and YOLO ONNX model.
- Risk assessment and warning-message emission to the IVI ECU on UDP port `47300`.
- Structured `[EVT]` lines and rotating packet capture export through View Log.
- The host-side checker `ADA_ECU/tools/check_evt_log.py` and extractor `ADA_ECU/tools/extract_pcap.sh`.

## 2. The procedure

### 2.1 Check the build inputs

1. Change to the repository root.

```bash
cd /path/to/FPT-Hackathon2026
```

Expected output: the command returns without an error.

2. Check that Docker can reach its daemon.

```bash
docker info --format '{{.ServerVersion}} {{.Architecture}}'
```

Expected output: one line containing a server version and an architecture, for example `29.0.0 aarch64`.

3. Check that the packaged video and model exist.

```bash
test -s ADA_ECU/media/ego-b-occluding-c.mp4 && test -s ADA_ECU/models/yolo11n.onnx && echo 'ADA image inputs: ready'
```

Expected output:

```text
ADA image inputs: ready
```

### 2.2 Choose an immutable image name

1. Set a unique release tag that will never be reused.

```bash
export ADA_IMAGE_TAG='<unique-release-tag>'
export ADA_IMAGE="registry.hackathon-2.carsky.io/m1-ada-ecu:${ADA_IMAGE_TAG}"
printf '%s\n' "$ADA_IMAGE"
```

Expected output:

```text
registry.hackathon-2.carsky.io/m1-ada-ecu:<unique-release-tag>
```

Use a new tag for every changed image. Do not overwrite an existing tag.

### 2.3 Build the Linux ARM64 image

1. Build a single-platform image without provenance or software-bill-of-material attestations.

```bash
docker buildx build \
  --platform linux/arm64 \
  --provenance=false \
  --sbom=false \
  --load \
  -t "$ADA_IMAGE" \
  ADA_ECU
```

Expected output: the final lines report a successful export and name the image in `$ADA_IMAGE`.

2. Check the locally loaded platform.

```bash
docker image inspect "$ADA_IMAGE" --format '{{.Os}}/{{.Architecture}}'
```

Expected output:

```text
linux/arm64
```

### 2.4 Run local container checks

1. Run the C++ smoke path with packaged samples.

```bash
docker run --rm --platform linux/arm64 "$ADA_IMAGE" \
  --config /app/config/ada-ecu.conf \
  --mock \
  --own-sensor-sample /app/testdata/r3_own_sensor.jsonl \
  --r2-sample /app/testdata/r2_v2x_object.sample.json
```

Expected output: JSON output includes `trackedObjects` entries for `own:B` and `v2x:1201:7`.

2. Run real ML detection against the packaged video.

```bash
docker run --rm --platform linux/arm64 --entrypoint python3 "$ADA_IMAGE" \
  /app/detector/tools/video_detector.py \
  --video /app/media/ego-b-occluding-c.mp4 \
  --backend yolo-onnx \
  --model /app/models/yolo11n.onnx \
  --every-n-frames 20 \
  --limit 5 \
  --confidence 0.20 \
  --log-detections
```

Expected output: one or more JSON lines identify `own:B` with `source` equal to `own_sensor`; no detector line identifies vehicle C.

### 2.5 Log in and push to Zot

1. Log in with the registry username and Zot API key.

```bash
docker login registry.hackathon-2.carsky.io -u '<registry-username>'
```

Expected output after entering the `zak_...` key at the password prompt:

```text
Login Succeeded
```

2. Push the immutable image.

```bash
docker push "$ADA_IMAGE"
```

Expected output: the final line reports the pushed image digest in the form `digest: sha256:...`.

3. Record the remote digest.

```bash
docker buildx imagetools inspect "$ADA_IMAGE"
```

Expected output: `Name` matches `$ADA_IMAGE`, `MediaType` describes one image manifest, and `Digest` contains `sha256:`.

4. Check the remote platform.

```bash
docker buildx imagetools inspect "$ADA_IMAGE" --format '{{json .Image}}' | grep -q '"architecture":"arm64"' && echo 'remote platform: linux/arm64'
```

Expected output:

```text
remote platform: linux/arm64
```

### 2.6 Configure the ADA node in Nydus

1. Open [the custom CarSky tenant](https://hackathon-2.carsky.io) and sign in.

Expected output: the Nydus blueprint list is visible.

2. Open the target blueprint, or import [blueprint-m1-cooperative-awareness.json](blueprint-m1-cooperative-awareness.json).

Expected output: the canvas contains the ADA ECU and Ethernet Bridge nodes. An imported blueprint has no Ethernet pins.

3. Select the ADA ECU node.

Expected output: the node Inspector is visible.

4. Set the image field to the exact value printed from `$ADA_IMAGE` in §2.2.

Expected output: the Inspector shows the immutable custom-registry image name.

5. Set the command to `--config /app/config/ada-ecu.conf` using the command-array editor.

Expected output: the node command contains two arguments: `--config` and `/app/config/ada-ecu.conf`.

6. Apply the canonical environment block from [node-ada-ecu.md § Blueprint node config](node-ada-ecu.md#blueprint-node-config).

Expected output: the Inspector contains the canonical variable names, including `V2X_LISTEN_HOST`, `V2X_LISTEN_PORT`, `IVI_HOST`, `IVI_PORT`, `TRACK_TIMEOUT_MS`, `CONFIRM_HITS`, and `EVENT_LOG_PATH`.

7. Add the `NET_RAW` capability.

Expected output: the node capability list contains `NET_RAW`.

8. Add one Ethernet output pin manually with address `10.99.0.12`, following [node-ada-ecu.md § Pins](node-ada-ecu.md#pins).

Expected output: the ADA ECU node shows one Ethernet output pin with address `10.99.0.12`.

9. Connect the ADA Ethernet pin to the Ethernet Bridge input pin.

Expected output: the canvas shows one edge from ADA ECU to the Ethernet Bridge.

### 2.7 Check the complete topology

1. Check that the Ethernet Bridge subnet and all static addresses match [carsky-4-node-blueprint.md § Topology](carsky-4-node-blueprint.md#1-topology).

Expected output: the bridge uses `10.99.0.0/24`; V2X ECU, ADA ECU, and IVI ECU use their documented static addresses.

2. Check that the V2X ECU sends its object JSON to `10.99.0.12:47200`.

Expected output: the V2X node configuration targets the ADA address and UDP port.

3. Check that the IVI application listens on UDP port `47300`.

Expected output: the IVI application log reports that its UDP listener is active.

### 2.8 Deploy the Room

1. Click empty canvas space to open the blueprint Inspector.

Expected output: the blueprint-level Inspector is visible.

2. Select **New Deployment**.

Expected output: the deployment dialog is visible.

3. Select the target Device, enter a unique deployment name, and select **Deploy**.

Expected output: the Deployment Viewer opens for the new Room.

4. Wait for every required node to reach `Running`.

Expected output: the Deployment Viewer reports every node ready and the ADA ECU restart count remains zero.

### 2.9 Run the end-to-end scenario

1. Open the ADA ECU **View Log** before starting the scenario.

Expected output: startup lines and `[EVT]` records are visible.

2. Open the IVI ECU application log or visible HMI.

Expected output: the IVI receiver is ready to display or log an incoming ADA warning message.

3. Start the Scenario Player case that makes the V2X ECU send vehicle C.

Expected output: the V2X ECU log shows an object message sent to `10.99.0.12:47200`.

4. Observe the ADA log while the packaged video loops.

Expected output: the log contains a tracked own-sensor vehicle B, a tracked V2X-relayed vehicle C, a risk transition, and an `r4_tx` event whose body contains both objects.

5. Observe the IVI ECU.

Expected output: the IVI log or HMI receives a warning message containing both `own:B` and `v2x:1201:7` in `trackedObjects`.

6. Wait for one packet-capture rotation to close.

Expected output: ADA View Log contains one complete block from `[PCAP-BEGIN ... sha256=...]` through `[PCAP-END]`.

### 2.10 Save and check the event log

1. Save the complete ADA **View Log** as `ada-view-log.txt` on the local machine.

Expected output: `ada-view-log.txt` contains `[EVT]`, `[PCAP-BEGIN ...]`, and `[PCAP-END]` lines.

2. Install the local checker dependency in a virtual environment if it is not already available.

```bash
python3 -m venv ADA_ECU/.venv
source ADA_ECU/.venv/bin/activate
python -m pip install -r ADA_ECU/requirements.txt
```

Expected output: pip finishes without an error and reports installed or already satisfied packages.

3. Check the complete event chain and warning-message schema.

```bash
python3 ADA_ECU/tools/check_evt_log.py \
  ada-view-log.txt \
  --r4-schema ADA_ECU/contracts/r4-ada-ivi.schema.json
```

Expected output begins with:

```text
ADA EVT chain: pass
```

### 2.11 Extract and inspect the packet capture

1. Extract the newest complete capture block.

```bash
ADA_ECU/tools/extract_pcap.sh ada-view-log.txt ada-ivi.pcap
```

Expected output begins with `pcap extracted:` and includes the verified SHA-256 digest.

2. Open the extracted file in Wireshark.

```bash
open -a Wireshark ada-ivi.pcap
```

Expected output: Wireshark opens `ada-ivi.pcap`.

3. Apply the display filter for ADA-to-IVI traffic.

```text
udp.port == 47300
```

Expected output: the packet list contains UDP traffic from the ADA address to the IVI address.

4. Inspect a UDP payload in the packet bytes pane.

Expected output: a warning-message JSON payload contains `trackedObjects`, `own:B`, and `v2x:1201:7`.

## 3. Work division between AI and human

| Action | AI / Human | Description |
|---|---|---|
| [Check the build inputs](#21-check-the-build-inputs) | AI | Check local prerequisites and deliverable files. |
| [Choose an immutable image name](#22-choose-an-immutable-image-name) | Human | Choose and record a unique release tag. |
| [Build the Linux ARM64 image](#23-build-the-linux-arm64-image) | AI | Build and inspect the local image. |
| [Run local container checks](#24-run-local-container-checks) | AI | Prove the packaged runtime and detector execute. |
| [Log in and push to Zot](#25-log-in-and-push-to-zot) | Human | Enter the secret Zot key and publish the image. |
| [Configure the ADA node in Nydus](#26-configure-the-ada-node-in-nydus) | Human | Set the node fields and draw the Ethernet pin. |
| [Check the complete topology](#27-check-the-complete-topology) | Human | Inspect peer addresses and listener readiness. |
| [Deploy the Room](#28-deploy-the-room) | Human | Select the Device and deploy from Nydus. |
| [Run the end-to-end scenario](#29-run-the-end-to-end-scenario) | Human | Start the scenario and observe live logs and HMI. |
| [Save and check the event log](#210-save-and-check-the-event-log) | Human | Save View Log and run the checker locally. |
| [Extract and inspect the packet capture](#211-extract-and-inspect-the-packet-capture) | Human | Extract the capture and inspect its UDP payload. |

An agent may perform the Zot push after `docker login` has stored a valid credential in the local keychain. An agent must stop at Nydus canvas steps, credential prompts, Device selection, visual HMI judgement, and browser log download.

## 4. Expected outputs and acceptance

The deployment is accepted only when all of these observable results exist:

- The registry image is an immutable, single-platform `linux/arm64` image at `registry.hackathon-2.carsky.io`.
- Local C++ smoke output contains tracked vehicle B and vehicle C.
- Local YOLO ONNX output detects vehicle B from the packaged video and does not label vehicle C.
- The ADA node reaches `Running` with restart count zero.
- ADA View Log records an own-sensor vehicle B and a V2X-relayed vehicle C as tracked.
- ADA View Log records a risk transition followed by a successful `r4_tx` event.
- The emitted warning message contains `trackedObjects` entries for `own:B` and `v2x:1201:7`.
- The IVI ECU receives that warning message over UDP port `47300` and exposes both objects in its log or HMI.
- `check_evt_log.py` prints `ADA EVT chain: pass` with schema checking enabled.
- `extract_pcap.sh` verifies the embedded SHA-256 digest and writes a readable pcap file.
- Wireshark shows ADA-to-IVI UDP traffic and the warning-message payload on port `47300`.

## 5. Confirm before relying on these

- Confirm that the custom tenant accepts the exact flat or wrapped container configuration shape shown by the current Nydus Inspector; [carsky-rest-api-blueprint.md](carsky-rest-api-blueprint.md) documents the stored REST shape, while [node-ada-ecu.md](node-ada-ecu.md) owns the node values.
- Confirm that `docker buildx imagetools inspect --format '{{json .Image}}'` is supported by the installed Buildx version. If it is not, inspect the manifest with the Zot web UI and record that it contains only `linux/arm64`.
- Confirm that the selected Device has capacity for every node before deploying the Room.
- Confirm that the IVI listener build accepts the current warning-message schema before treating ADA-side transmission as proof of IVI reception.
- Confirm that View Log preserves complete base64 capture blocks. A truncated block cannot pass the extractor's SHA-256 check.
- Confirm the Scenario Player case and V2X ECU configuration produce the expected vehicle C message before diagnosing a missing ADA V2X track.
