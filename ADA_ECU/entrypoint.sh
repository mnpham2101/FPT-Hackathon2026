#!/bin/sh
# entrypoint.sh - ADA ECU container entrypoint (ADA HLD D9; blueprint
# `command: ["./entrypoint.sh"]` per requirements/car-sky-guide/node-ada-ecu.md).
#
# Two processes, one container:
#   background - ./capture.sh, the ADA->IVI traffic evidence stream (R15/R19); the
#                script itself lands in Phase 4 (6.4.4.1). The -x guard below is why
#                this file and the blueprint command NEVER change when it arrives:
#                until then the guard is simply false and nothing is launched.
#   foreground - ./ada_ecu, the node application. It IS the container's lifetime.
#
# Deliberately NO `set -e`: capture is evidence, not the mission. A missing or
# failing capture.sh must never take the node down.
#
# `exec` on the last line is load-bearing, not style: it REPLACES this shell, so
# ada_ecu becomes the container's PID 1 and receives the platform's SIGTERM
# directly - that is what makes the composition root's clean-shutdown path run,
# and it means the app's exit code is the container's exit code.
#
# No tunables are read or defaulted here: every value is env-injected by the
# blueprint and consumed by the app (src/config/config.cpp), which owns the HLD
# section 6 defaults.

set -u

echo "[BOOT] entrypoint $(date -u +%Y-%m-%dT%H:%M:%SZ) pid=$$ cwd=$(pwd)"

# Backgrounds the whole guarded list: launches capture.sh only if it is present
# and executable, silently a no-op otherwise (see header).
[ -x ./capture.sh ] && ./capture.sh &

echo "[BOOT] exec ./ada_ecu - no further app output means the binary failed to start"
exec ./ada_ecu
