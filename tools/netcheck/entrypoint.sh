#!/bin/sh
set -u
echo "[BOOT] role=${ROLE:-unset} listen=${LISTEN_PORT:-none} next=${NEXT_HOP_HOST:-none}:${NEXT_HOP_PORT:-none}"
./capture.sh &                  # capture runs alongside, independent of the traffic program
exec python3 -u netcheck.py     # foreground = the pod's lifetime
