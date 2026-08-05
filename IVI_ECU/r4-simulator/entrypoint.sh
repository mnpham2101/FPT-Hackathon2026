#!/bin/sh
# In-Room entrypoint of m1-r4-sim (blueprint command: ["./entrypoint.sh"], workdir /app).
# Configuration comes entirely from the node env (IVI_ECU_HOST, IVI_ECU_PORT, R4_SCENARIO,
# R4_RATE_HZ, START_DELAY_S); the start delay is honored inside the simulator, not here.
set -eu
echo "[BOOT] r4-simulator entrypoint $(date -u +%Y-%m-%dT%H:%M:%SZ) target=${IVI_ECU_HOST:-unset}:${IVI_ECU_PORT:-47300} scenario=${R4_SCENARIO:-scenarios/approach.json}"
exec /app/bin/r4-simulator
