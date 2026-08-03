# Phase 4 Fusion + Warning Runtime

Phase 4 consumes live R2 traffic, updates the R3 store, runs NLOS risk assessment, and emits edge-triggered R4 warnings to IVI over UDP.

## Current Runtime Behavior

- `--max-r2 <n>` receives up to `n` R2 UDP datagrams.
- A warning R4 is emitted only on risk transitions.
- Repeated in-range C updates do not spam duplicate warnings.
- Leaving the gate or timing out emits a low-risk transition.
- R2 timeout expires only `v2x_relayed` tracks; `own_sensor` B remains in the store so clear events still carry A→B geometry.
- Receive timeouts expire only `v2x_relayed` tracks on a tick controlled by `r2_receive_timeout_ms`.
- R4 uses frozen geometry names `vehicleB` and `vehicleC`; it also contains debug/additive `trackedObjects`, including `own:B` with A→B distance and `v2x:1201:7` for relayed C.
- CRA is selected through `CRA_ENABLED`; the built-in registry currently exposes
  `nlos_obstruction`. It classifies low/medium/high at configurable near/critical thresholds and
  applies `RISK_DWELL_MS` before a transition.
- Event JSONL contains assessment, track transition, risk transition, complete attempted R4 body,
  and whether UDP transmission succeeded.

## Multi-Packet Smoke — Self-Contained

Terminal 1 — IVI receiver:

```sh
python3 ADA_ECU/tools/mock_ivi_receiver.py --host 127.0.0.1 --port 46004 --timeout 10 --count 2
```

Terminal 2 — ADA runtime with internal mock R2 sequence:

```sh
ADA_ECU/build/ada_ecu --config ADA_ECU/config/ada-ecu.conf --mock-distances 40,25,24,36
```

## Multi-Packet Smoke — External Mock V2X

```sh
ADA_ECU/build/ada_ecu --config ADA_ECU/config/ada-ecu.conf --max-r2 4
python3 ADA_ECU/tools/mock_v2x_sender.py --host 127.0.0.1 --port 46002 --distances 40,25,24,36
```

Expected behavior:

| R2 distance | Track/risk behavior |
|---:|---|
| 40 m | C outside gate, no warning |
| 25 m | C enters near zone, R4 `riskState = medium` |
| 24 m | C stays in gate, no duplicate R4 |
| 36 m | C exits near zone, R4 `riskState = low` |

## Timeout Clear Smoke

This emits one medium-risk warning when C enters the near zone, then emits one low-risk transition after `miss_limit_ms` because no more R2 messages arrive:

```sh
python3 ADA_ECU/tools/mock_ivi_receiver.py --host 127.0.0.1 --port 46004 --timeout 10 --count 2
ADA_ECU/build/ada_ecu --config ADA_ECU/config/ada-ecu.conf --mock-distances 25
```

## Acceptance evidence

```sh
LOG_PATH=/tmp/ada-phase4-events.jsonl \
  ADA_ECU/build/ada_ecu --config ADA_ECU/config/ada-ecu.conf --mock \
  --own-sensor-sample ADA_ECU/testdata/r3_own_sensor.jsonl
python3 ADA_ECU/tools/check_evt_log.py /tmp/ada-phase4-events.jsonl
python3 ADA_ECU/tools/event_report.py /tmp/ada-phase4-events.jsonl
```

The checker requires tracked own-sensor B, tracked relayed C, a CRA transition, and a complete R4
body containing both objects and `geometry.vehicleB`. `payload.sent` distinguishes contract proof
from successful Ethernet delivery. The CarSky acceptance additionally requires a capture at IVI;
see `requirements/car-sky-guide/node-ada-ecu.md`.

Local acceptance on 2026-08-03 passed with 10 events, one risk transition, and one complete R4
transmission record. The ARM64 container smoke reported `sent=true`; its R4 carried tracked
`own:B`, tracked `v2x:1201:7`, and composed B/C geometry.
