#!/bin/sh
# entrypoint.sh - V2X ECU container entrypoint (HLD sections 4/9; blueprint
# `command: ["./entrypoint.sh"]` per requirements/car-sky-guide/node-v2x-ecu.md).
#
# Two processes, one container:
#   background - ./capture.sh, the D5 traffic-capture evidence stream ([CAP] live
#                text + rotating pcap exported as base64 blocks).
#   foreground - ./v2x_ecu, the node application. It IS the container's lifetime.
#
# Deliberately NO `set -e`: capture is evidence, not the mission. If capture.sh is
# missing, not executable, or dies immediately, the shell reports it on stderr and
# the background job exits non-zero - that must never take the node down. (capture.sh
# itself is written to degrade rather than exit, HLD D5; this is the outer guard for
# the cases it cannot reach, e.g. the file not being executable at all.)
#
# `exec` on the last line is load-bearing, not style: it REPLACES this shell, so
# v2x_ecu becomes the container's PID 1 and receives the platform's SIGTERM
# directly. That is what makes main.cpp's clean-shutdown path (std::signal ->
# g_stop_requested -> stop Rx thread -> exit 0) actually run; behind a non-exec
# parent shell the signal would land on the shell, the app would be killed on the
# grace-period timeout instead, and the [SHUTDOWN] evidence line would be lost.
# exec also means the app's exit code is the container's exit code - nothing here
# swallows or rewrites it.
#
# No tunables are read or defaulted here: every value is env-injected by the
# blueprint and consumed by the app (src/config/config.cpp) or by capture.sh, each
# of which owns its own HLD section 6 defaults.

set -u

echo "[BOOT] entrypoint $(date -u +%Y-%m-%dT%H:%M:%SZ) pid=$$ cwd=$(pwd)"

./capture.sh &
echo "[BOOT] capture.sh launched (pid $!) - non-fatal if it fails"

echo "[BOOT] exec ./v2x_ecu - no further app output means the binary failed to start"
exec ./v2x_ecu
