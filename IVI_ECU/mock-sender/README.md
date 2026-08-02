# Mock R4 Sender — 2-node Task 5.1 Test Node

Container node that fires scripted R4 UDP warning events at the IVI ECU node.

> Lead note: production stimulus is Kotlin `:r4-simulator` (group 5.6). This Python sender is interim only — defaults match CarSky blueprint `10.99.0.13:47300`.

## Build & Push

```bash
docker build -t registry.carsky.io/m1-mock-r4-sender:latest .
docker push registry.carsky.io/m1-mock-r4-sender:latest
```

## Environment Variables

| Variable | Default | Description |
|---|---|---|
| `IVI_ECU_HOST` | `10.99.0.13` | IVI Skycraft ethernet pin (CarSky blueprint) |
| `IVI_ECU_PORT` | `47300` | UDP port of R4ListenerService (`BuildConfig.R4_UDP_PORT`) |
| `INTERVAL_MS` | `2000` | Milliseconds between packets |
| `CYCLES` | `5` | Number of full approach/leave/unknown cycles |
| `SCHEMA_VERSION` | `1` | R4 schema version to embed in packets |

## Test Scenario (per cycle)

1. **C approaching** — 5 packets, distance 40m → 20m, `warningType=nlos_obstruction`
2. **State heartbeat** — 1 `type=state` packet
3. **C leaving** — 3 packets, distance 20m → 50m, risk=low
4. **Additive-version test** — 1 packet `warningType=future_unknown_type` (IVI must not crash)

## Local test (without CarSky)

```bash
# Terminal 1 — listen to see what's being sent
nc -ulv 47300

# Terminal 2 — run sender
IVI_ECU_HOST=127.0.0.1 IVI_ECU_PORT=47300 CYCLES=2 python mock_r4_sender.py
```
