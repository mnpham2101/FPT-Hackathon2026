#!/bin/sh
if tcpdump -D >/dev/null 2>&1; then
  echo "[CAP] tcpdump active, filter: ${CAPTURE_FILTER:-udp}"
  tcpdump -i any -n -l -tttt "${CAPTURE_FILTER:-udp}" 2>&1 | sed 's/^/[CAP] /'
else
  echo "[CAP] no NET_RAW - falling back to /proc/net/dev packet counters"
  while :; do
    awk 'NR>2 {gsub(":",""); if ($3+$11 > 0) printf "[CAP] %s rx_pkts=%s tx_pkts=%s\n", $1, $3, $11}' /proc/net/dev
    sleep "${CAPTURE_INTERVAL_S:-5}"
  done
fi
