#!/bin/sh
set -eu

if [ "${ENABLE_PCAP:-false}" = "true" ]; then
    /app/capture.sh &
fi

exec /app/ada_ecu "$@"
