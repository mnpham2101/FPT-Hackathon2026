#!/bin/sh
set -eu

if [ "${1:-}" = "--export-one" ]; then
    capture_path=${2:?pcap path is required}
    if command -v sha256sum >/dev/null 2>&1; then
        digest=$(sha256sum "$capture_path" | awk '{print $1}')
    else
        digest=$(shasum -a 256 "$capture_path" | awk '{print $1}')
    fi
    encoded=$(base64 < "$capture_path" | tr -d '\n')
    printf '[CAP] sha256=%s base64=%s\n' "$digest" "$encoded"
    exit 0
fi

pcap_dir=${PCAP_DIR:-/tmp/ada-pcap}
rotate_seconds=${CAPTURE_ROTATE_S:-60}
capture_filter=${CAPTURE_FILTER:-udp dst port 47300}
mkdir -p "$pcap_dir"
exec tcpdump -i any -U -s 0 -G "$rotate_seconds" -w "$pcap_dir/ada-ivi-%Y%m%d-%H%M%S.pcap" $capture_filter
