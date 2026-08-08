# Traffic Capture & Wireshark Retrieval (R6 / R19 evidence)

How bridge traffic is captured on a CarSky Container node and turned into a `.pcap` the user opens in Wireshark. Design rationale: [V2X ECU decision D5](../../documents/Design/V2X-ECU/v2x-ecu-design-decisions.md#d5--r6-traffic-capture-runs-in-the-container-and-exports-through-the-log); capture technique origin: [baseline-connectivity-smoke-test.md](../../plans/doc/research_notes/baseline-connectivity-smoke-test.md). User directive 2026-07-30: capture via tcpdump; saved traffic read by tool/script or analyzed in Wireshark.

## How capture works in the node

- There is **no platform pcap facility** — capture runs in-container: the node image ships `tcpdump` + `capture.sh`, started by the entrypoint alongside the app. Needs `"capabilities": ["NET_RAW"]` flat in the node `config` (falls back to `/proc/net/dev` packet counters without it).
- `capture.sh` runs two tcpdump processes:
  - **Live text** — `[CAP]`-prefixed lines on stdout: read directly in **View Log** for the live "traffic is flowing" check.
  - **Saved pcap** — `tcpdump -w` into `$PCAP_DIR`, rotated every `CAPTURE_ROTATE_S` seconds; each closed file is emitted to stdout as base64 between `[PCAP-BEGIN <name>]` and `[PCAP-END]` markers — View Log is the node's only egress, and base64 makes the export byte-perfect.
- Capture point: the **V2X ECU node** — its interface sees both live Phase 1 flows (bench→V2X R1 CPMs, V2X→ADA R2 JSON). Env knobs (`CAPTURE_FILTER`, `PCAP_DIR`, `CAPTURE_ROTATE_S`): [node-v2x-ecu.md](node-v2x-ecu.md).

## Retrieving a .pcap (user steps)

1. Deployment Viewer → V2X ECU node → **View Log** → save the log to a local file (e.g. `v2x.log`).
2. Run the extraction script: `V2X_ECU/tools/extract_pcap.sh v2x.log` — it cuts each `[PCAP-BEGIN]…[PCAP-END]` block, base64-decodes it, and writes `<name>.pcap` next to the log.
3. Open the `.pcap` in Wireshark; filter `udp.port == 47100 || udp.port == 47200`.

Manual fallback (one block): `sed -n '/\[PCAP-BEGIN/,/\[PCAP-END\]/p' v2x.log | grep -v '\[PCAP' | base64 -d > capture.pcap`.

## Reading the capture in Wireshark

- **CPM payloads will not dissect as ITS.** The M1 wire format is raw UPER `CollectivePerceptionMessage` per UDP datagram with **no GeoNetworking/BTP envelope** (R1 profile convention F5) — Wireshark's ITS dissector keys on GN/BTP framing, so the payload shows as opaque UDP data. This is expected, not a defect.
- Evidence method instead: correlate datagrams with the node's `[EVT]` JSONL log by timestamp and byte length, and match payload bytes against the golden vectors (`contracts/golden-vectors/*.uper`) — that is the "V2X PDUs correctly sent/received" demo check.
- R2 (port 47200) payloads are plain JSON — readable directly in Wireshark's packet bytes pane.
