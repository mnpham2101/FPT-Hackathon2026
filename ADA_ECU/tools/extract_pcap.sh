#!/bin/sh
set -eu

log_path=${1:?CarSky log path is required}
output_path=${2:?output pcap path is required}
cap_line=$(grep '^\[CAP\] ' "$log_path" | tail -n 1)
expected=$(printf '%s\n' "$cap_line" | sed -n 's/.*sha256=\([^ ]*\).*/\1/p')
encoded=$(printf '%s\n' "$cap_line" | sed -n 's/.*base64=\(.*\)$/\1/p')
printf '%s' "$encoded" | base64 --decode > "$output_path" 2>/dev/null || printf '%s' "$encoded" | base64 -D > "$output_path"
actual=$(sha256sum "$output_path" 2>/dev/null | awk '{print $1}' || shasum -a 256 "$output_path" | awk '{print $1}')
test "$actual" = "$expected"
printf 'pcap extracted: %s sha256=%s\n' "$output_path" "$actual"
