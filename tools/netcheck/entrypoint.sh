#!/bin/sh
set -u
# First line = the container itself started (a container has no OS boot beyond this).
echo "[BOOT] entrypoint.sh invoked $(date -u +%Y-%m-%dT%H:%M:%SZ)"
echo "[BOOT] role=${ROLE:-unset} listen=${LISTEN_PORT:-none} next=${NEXT_HOP_HOST:-none}:${NEXT_HOP_PORT:-none}"
./capture.sh &                  # capture runs alongside, independent of the traffic program
echo "[BOOT] capture.sh launched (pid $!)"
echo "[BOOT] exec python3 netcheck.py - no [BOOT] python line after this means python3 failed to start"
exec python3 -u netcheck.py     # foreground = the pod's lifetime
