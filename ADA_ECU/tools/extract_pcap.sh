#!/bin/sh
set -eu

log_path=${1:?CarSky log path is required}
output_path=${2:-}
begin_line=$(grep -n '^\[PCAP-BEGIN ' "$log_path" | tail -n 1 || true)
test -n "$begin_line" || { printf 'no PCAP block found in %s\n' "$log_path" >&2; exit 2; }

begin_number=${begin_line%%:*}
header=${begin_line#*:}
capture_name=$(printf '%s\n' "$header" | sed -n 's/^\[PCAP-BEGIN \([^ ]*\) sha256=.*/\1/p')
expected=$(printf '%s\n' "$header" | sed -n 's/.*sha256=\([^]]*\).*/\1/p')
case "$capture_name" in
    ''|*/*|*..*) printf 'unsafe capture name: %s\n' "$capture_name" >&2; exit 3 ;;
esac
test -n "$output_path" || output_path=$(dirname "$log_path")/$capture_name

encoded=$(awk -v start="$begin_number" 'NR > start && /^\[PCAP-END\]$/ {exit} NR > start {print}' "$log_path" | tr -d '\n')
printf '%s' "$encoded" | base64 --decode > "$output_path" 2>/dev/null || printf '%s' "$encoded" | base64 -D > "$output_path"
actual=$(sha256sum "$output_path" 2>/dev/null | awk '{print $1}' || shasum -a 256 "$output_path" | awk '{print $1}')
test "$actual" = "$expected" || { printf 'pcap SHA-256 mismatch\n' >&2; exit 4; }
printf 'pcap extracted: %s sha256=%s\n' "$output_path" "$actual"
