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

## Multi-Packet Smoke — Self-Contained

Terminal 1 — IVI receiver:

```sh
python3 ada-ecu/tools/mock_ivi_receiver.py --host 127.0.0.1 --port 46004 --timeout 10 --count 2
```

Terminal 2 — ADA runtime with internal mock R2 sequence:

```sh
ada-ecu/build/ada_ecu --config ada-ecu/config/ada-ecu.conf --mock-distances 40,25,24,36
```

## Multi-Packet Smoke — External Mock V2X

```sh
ada-ecu/build/ada_ecu --config ada-ecu/config/ada-ecu.conf --max-r2 4
python3 ada-ecu/tools/mock_v2x_sender.py --host 127.0.0.1 --port 46002 --distances 40,25,24,36
```

Expected behavior:

| R2 distance | Track/risk behavior |
|---:|---|
| 40 m | C outside gate, no warning |
| 25 m | C enters gate, R4 `riskState = high` |
| 24 m | C stays in gate, no duplicate R4 |
| 36 m | C exits gate, R4 `riskState = low` |

## Timeout Clear Smoke

This emits one high-risk warning when C enters the gate, then emits one low-risk transition after `miss_limit_ms` because no more R2 messages arrive:

```sh
python3 ada-ecu/tools/mock_ivi_receiver.py --host 127.0.0.1 --port 46004 --timeout 10 --count 2
ada-ecu/build/ada_ecu --config ada-ecu/config/ada-ecu.conf --mock-distances 25
```
