#!/bin/sh
# capture.sh - V2X ECU in-container traffic capture (HLD D5, R6/R19 evidence).
#
# Started in the BACKGROUND by entrypoint.sh (`./capture.sh &`) alongside the
# foreground v2x_ecu app: this script must never take the container down, so it
# never exits non-zero and degrades instead of dying.
#
# Two tcpdump processes, two consumers (HLD D5):
#   (a) live text  - `tcpdump -i any -n -l -tttt $CAPTURE_FILTER`, every line
#       prefixed "[CAP] " on stdout: the human-readable "traffic is flowing"
#       check read directly in the CarSky View Log.
#   (b) saved pcap - `tcpdump -w` into $PCAP_DIR rotating every
#       $CAPTURE_ROTATE_S seconds; each closed file is base64-emitted to stdout
#       between marker lines. View Log is the node's only egress (no platform
#       file/pcap endpoint exists), and base64 makes the export byte-perfect.
#
# EXPORT BLOCK FORMAT - frozen contract, parsed by V2X_ECU/tools/extract_pcap.sh
# (subtask 6.1.5.3) and documented in requirements/car-sky-guide/traffic-capture-wireshark.md:
#
#     [PCAP-BEGIN <basename>]
#     <base64 of the pcap file, one or more lines>
#     [PCAP-END]
#
# where <basename> is the pcap file name only (no directory), the marker lines
# carry nothing else, and everything strictly between them is base64 text.
# Extraction is therefore: cut the lines between the markers, base64 -d them,
# write <basename>. An exported pcap is DELETED after emission - the log copy is
# the retrievable artifact and the container's disk stays bounded.
#
# DUAL MODE (single file, no temp helper script):
#   capture.sh                       - main mode: run both consumers, supervise.
#   capture.sh --export-one <file>    - export mode: emit the marker block for
#                                      <file>, delete it, exit 0.
#   capture.sh <file>                 - same as --export-one <file>. This bare
#       form exists because tcpdump's `-z <cmd>` execs <cmd> as a single program
#       name with the just-closed file as its ONLY argument (no word splitting
#       of the -z value), so `-z "$SELF"` is how consumer (b) re-enters this
#       script. --export-one is the explicit/testable spelling of the same path.
#
# DEGRADED MODES (O2 fallback posture - log and stay alive, never exit):
#   - tcpdump missing or NET_RAW unhonored -> /proc/net/dev packet-counter loop.
#   - $PCAP_DIR not creatable/writable     -> mktemp -d, else consumer (b) off.
#   - capture.sh not executable            -> consumer (b) off (tcpdump -z needs
#                                             to exec this file), live text kept.
#   - both consumers exit                  -> counter-loop fallback.
#
# Configuration: HLD section 6 defaults only, no other tunable literals.
#   CAPTURE_FILTER   (udp)            tcpdump BPF filter, may be multi-word.
#   PCAP_DIR         (/data/capture)  rotating pcap directory.
#   CAPTURE_ROTATE_S (60)             pcap rotation + export period; also the
#                                     supervisor tick and the counter-fallback
#                                     print period (reusing it avoids inventing
#                                     an env var outside HLD section 6).

set -u

CAPTURE_FILTER="${CAPTURE_FILTER:-udp}"
PCAP_DIR="${PCAP_DIR:-/data/capture}"
CAPTURE_ROTATE_S="${CAPTURE_ROTATE_S:-60}"

cap_log() {
  echo "[CAP] $*"
}

usage() {
  echo "[CAP] usage: capture.sh | capture.sh --export-one <pcap-file> | capture.sh <pcap-file>"
}

# Emit the frozen marker block for one closed pcap, then remove it.
export_one() {
  ef="$1"
  if [ ! -f "$ef" ]; then
    cap_log "export skipped - not a regular file: $ef"
    return 0
  fi
  if ! command -v base64 >/dev/null 2>&1; then
    cap_log "export skipped - base64 not available, keeping $ef on disk"
    return 0
  fi
  ename=$(basename "$ef")
  echo "[PCAP-BEGIN $ename]"
  if base64 "$ef"; then
    erc=0
  else
    erc=$?
  fi
  echo "[PCAP-END]"
  if [ "$erc" -ne 0 ]; then
    cap_log "base64 failed (rc=$erc) for $ename - that block is incomplete"
    return 0
  fi
  rm -f "$ef" 2>/dev/null || cap_log "could not delete exported $ename"
  return 0
}

# O2 fallback: no capture handle, so report interface packet counters forever.
counter_fallback() {
  cap_log "counter fallback active - printing /proc/net/dev rx/tx packets every ${CAPTURE_ROTATE_S}s"
  while :; do
    if [ -r /proc/net/dev ]; then
      awk 'NR>2 {gsub(":",""); if ($3+$11 > 0) printf "[CAP] %s rx_pkts=%s tx_pkts=%s\n", $1, $3, $11}' /proc/net/dev
    else
      cap_log "/proc/net/dev unreadable - no capture evidence available"
    fi
    sleep "$CAPTURE_ROTATE_S"
  done
}

# ---- argument dispatch (see DUAL MODE above) --------------------------------

case "${1:-}" in
  --export-one)
    if [ "$#" -lt 2 ] || [ -z "$2" ]; then
      usage
      exit 2
    fi
    export_one "$2"
    exit 0
    ;;
  -h|--help)
    usage
    exit 0
    ;;
  "")
    : # main mode
    ;;
  -*)
    cap_log "unknown option: $1"
    usage
    exit 2
    ;;
  *)
    # tcpdump -z convention: single bare argument is the just-closed pcap.
    export_one "$1"
    exit 0
    ;;
esac

# ---- main mode --------------------------------------------------------------

# Absolute path to this script: tcpdump -z must exec it, and it runs with an
# inherited cwd we do not control.
case "$0" in
  /*) SELF="$0" ;;
  *)  SELF="$(cd "$(dirname "$0")" 2>/dev/null && pwd)/$(basename "$0")" ;;
esac

# Line-buffer the live-text pipeline when stdbuf exists (probe, never assume);
# tcpdump -l already asks for line-buffered output, stdbuf covers the prefixer.
if command -v stdbuf >/dev/null 2>&1; then
  BUF="stdbuf -oL"
else
  BUF=""
fi

if ! command -v tcpdump >/dev/null 2>&1; then
  cap_log "tcpdump not installed in this image - live capture and pcap export disabled"
  counter_fallback
fi

# Capture-handle probe (the Phase 0 tools/netcheck/capture.sh form, deployed-verified).
if ! tcpdump -D >/dev/null 2>&1; then
  cap_log "cannot open a capture handle (NET_RAW not honored?) - degrading, app unaffected"
  counter_fallback
fi

cap_log "tcpdump active, filter: ${CAPTURE_FILTER}, pcap dir: ${PCAP_DIR}, rotate: ${CAPTURE_ROTATE_S}s"

PCAP_OK=1
if ! mkdir -p "$PCAP_DIR" 2>/dev/null || [ ! -w "$PCAP_DIR" ]; then
  ALT_DIR=$(mktemp -d 2>/dev/null || echo "")
  if [ -n "$ALT_DIR" ] && [ -w "$ALT_DIR" ]; then
    cap_log "PCAP_DIR ${PCAP_DIR} unusable - falling back to ${ALT_DIR}"
    PCAP_DIR="$ALT_DIR"
  else
    cap_log "PCAP_DIR ${PCAP_DIR} unusable and no temp dir available - pcap export disabled"
    PCAP_OK=0
  fi
fi

if [ "$PCAP_OK" -eq 1 ] && [ ! -x "$SELF" ]; then
  chmod +x "$SELF" 2>/dev/null || :
  if [ ! -x "$SELF" ]; then
    cap_log "${SELF} is not executable - tcpdump -z cannot re-enter it, pcap export disabled"
    PCAP_OK=0
  fi
fi

PID_TEXT=""
PID_PCAP=""

# Consumer (a): live [CAP] text. $CAPTURE_FILTER is intentionally unquoted so a
# multi-word BPF filter (e.g. "udp port 47100") splits into tcpdump arguments.
# shellcheck disable=SC2086
$BUF tcpdump -i any -n -l -tttt $CAPTURE_FILTER 2>&1 | $BUF sed 's/^/[CAP] /' &
PID_TEXT=$!
cap_log "live-text consumer started (pid ${PID_TEXT})"

# Consumer (b): rotating pcap; -z re-enters this script per closed file.
if [ "$PCAP_OK" -eq 1 ]; then
  # shellcheck disable=SC2086
  tcpdump -i any -n -w "${PCAP_DIR}/cap-%Y%m%d-%H%M%S.pcap" \
    -G "$CAPTURE_ROTATE_S" -z "$SELF" $CAPTURE_FILTER >/dev/null 2>&1 &
  PID_PCAP=$!
  cap_log "pcap consumer started (pid ${PID_PCAP}), export every ${CAPTURE_ROTATE_S}s as [PCAP-BEGIN <name>] .. [PCAP-END]"
fi

# Supervise: report a consumer's death once, keep the survivor running, and drop
# into the counter fallback only when nothing is capturing any more.
TEXT_REPORTED=0
PCAP_REPORTED=0
while :; do
  sleep "$CAPTURE_ROTATE_S"

  ALIVE_TEXT=0
  if [ -n "$PID_TEXT" ] && kill -0 "$PID_TEXT" 2>/dev/null; then
    ALIVE_TEXT=1
  fi
  if [ "$ALIVE_TEXT" -eq 0 ] && [ "$TEXT_REPORTED" -eq 0 ]; then
    cap_log "live-text consumer exited - not restarted, app unaffected"
    TEXT_REPORTED=1
  fi

  ALIVE_PCAP=0
  if [ -n "$PID_PCAP" ] && kill -0 "$PID_PCAP" 2>/dev/null; then
    ALIVE_PCAP=1
  fi
  if [ "$ALIVE_PCAP" -eq 0 ] && [ -n "$PID_PCAP" ] && [ "$PCAP_REPORTED" -eq 0 ]; then
    cap_log "pcap consumer exited - no further [PCAP-BEGIN] blocks, app unaffected"
    PCAP_REPORTED=1
  fi

  if [ "$ALIVE_TEXT" -eq 0 ] && [ "$ALIVE_PCAP" -eq 0 ]; then
    cap_log "no capture consumer left alive"
    counter_fallback
  fi
done
