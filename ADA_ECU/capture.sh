#!/bin/sh
set -eu

sha256_file() {
    if command -v sha256sum >/dev/null 2>&1; then
        sha256sum "$1" | awk '{print $1}'
    else
        shasum -a 256 "$1" | awk '{print $1}'
    fi
}

export_capture() {
    capture_path=$1
    capture_name=$(basename "$capture_path")
    digest=$(sha256_file "$capture_path")
    printf '[PCAP-BEGIN %s sha256=%s]\n' "$capture_name" "$digest"
    base64 < "$capture_path"
    printf '[PCAP-END]\n'
}

if [ "${1:-}" = "--export-one" ]; then
    export_capture "${2:?pcap path is required}"
    exit 0
fi

pcap_dir=${PCAP_DIR:-/data/capture}
rotate_seconds=${CAPTURE_ROTATE_S:-60}
capture_filter=${CAPTURE_FILTER:-udp}
mkdir -p "$pcap_dir"

tcpdump -i any -n -l -tttt $capture_filter 2>&1 | sed -u 's/^/[CAP] /' &
live_pid=$!
tcpdump -i any -n -U -s 0 -G "$rotate_seconds" -w "$pcap_dir/ada-ivi-%Y%m%d-%H%M%S.pcap" $capture_filter &
writer_pid=$!
trap 'kill "$live_pid" "$writer_pid" 2>/dev/null || true' EXIT INT TERM

while kill -0 "$writer_pid" 2>/dev/null; do
    sleep "$rotate_seconds"
    latest=$(find "$pcap_dir" -name '*.pcap' -type f | sort | tail -n 1)
    for capture_path in "$pcap_dir"/*.pcap; do
        [ -f "$capture_path" ] || continue
        [ "$capture_path" = "$latest" ] && continue
        [ -f "$capture_path.sent" ] && continue
        export_capture "$capture_path"
        touch "$capture_path.sent"
    done
done

printf '[CAP] tcpdump writer stopped\n' >&2
exit 1
