#!/bin/sh
set -u
# First line = the container itself started (a container has no OS boot beyond this).
echo "[BOOT] entrypoint.sh invoked $(date -u +%Y-%m-%dT%H:%M:%SZ) role=${ROLE:-unset}"
./capture.sh &                  # capture runs alongside, independent of the role program
echo "[BOOT] capture.sh launched (pid $!)"
case "${ROLE:-}" in
  v2x_mock)
    echo "[BOOT] exec python3 /app/mock_v2x.py"
    exec python3 /app/mock_v2x.py   # foreground = the pod's lifetime
    ;;
  ivi_mock)
    echo "[BOOT] exec python3 /app/mock_ivi.py"
    exec python3 /app/mock_ivi.py   # foreground = the pod's lifetime
    ;;
  *)
    # Loud failure: a wrong or unset ROLE exits non-zero, so the node shows a
    # climbing restart count and this line names the cause in its log.
    echo "[ERROR] entrypoint.sh: ROLE='${ROLE:-}' is not one of v2x_mock | ivi_mock - exiting"
    exit 1
    ;;
esac
