#!/usr/bin/env bash
# extract_pcap.sh - host-side "saved View Log in -> .pcap files out" extractor.
#
# The automatic retrieval path of the ADA->IVI capture, decision D9
# (ADA_ECU/doc/ada-ecu-design-decisions.md); user procedure and the manual
# grep/base64 fallback live in
# requirements/car-sky-guide/traffic-capture-wireshark.md.
#
# HOST TOOL ONLY - never shipped in the m1-ada-ecu image (ADA_ECU/tools/ is
# excluded from the build context by .dockerignore). Runs in Git Bash on Windows
# and on Linux; bash + coreutils `base64` are the only dependencies.
#
# INPUT FORMAT - frozen by the producer ADA_ECU/capture.sh (see its "EXPORT
# BLOCK FORMAT" header):
#
#     [PCAP-BEGIN <basename>]
#     <base64 of the pcap file, one or more lines>
#     [PCAP-END]
#
# A saved View Log interleaves those blocks with unrelated `[CAP]`, `[EVT]` and
# `[BOOT]` lines; everything outside a block is ignored.
#
# BEHAVIOUR DECISIONS (all deliberate, none silent):
#   - Output goes NEXT TO THE INPUT LOG by default - the usage contract in
#     traffic-capture-wireshark.md promises exactly that - and `-o <dir>`
#     overrides it. Nothing is ever written into the repository.
#   - <basename> is taken from the log, i.e. it is untrusted input: it is
#     reduced to a file name (everything up to the last `/` or `\` is dropped)
#     so a crafted `[PCAP-BEGIN ../../evil.pcap]` writes `evil.pcap` inside the
#     output directory and cannot escape it. `.`/`..`/empty degrade to
#     `block-<n>.pcap`. A name not ending in `.pcap` gets the suffix appended.
#   - NO SILENT CLOBBER: if the target file already exists (duplicate names in
#     one log, or a previous run), `-2`, `-3`, ... is inserted before `.pcap`;
#     after `-99` that block is failed rather than overwriting.
#   - CRLF tolerant: a log saved from a browser on Windows carries `\r`, which
#     is stripped from every line before matching and before decoding.
#   - `capture.sh`'s two consumers share one stdout, so a `[CAP]`/`[EVT]`/
#     `[BOOT]` line can land INSIDE a base64 block. Such lines are dropped from
#     the block with a warning (they are unambiguous: the base64 alphabet has
#     neither `[` nor spaces) instead of poisoning the decode.
#   - A truncated final block (`[PCAP-BEGIN]` with no `[PCAP-END]`) is reported
#     and NOT written - a half file must not masquerade as a complete capture;
#     the manual one-liner in the guide recovers the partial bytes if wanted.
#   - One failing block never hides the healthy ones: remaining blocks are still
#     extracted and the failure is reflected in the exit status.
#
# EXIT STATUS
#   0  every block found was extracted
#   1  no `[PCAP-BEGIN ...]` block in the input at all
#   2  usage error (bad option, missing/unreadable input, unusable output dir)
#   3  at least one block failed (decode error, truncation, name collision)

set -u

PROG=$(basename "$0")

usage() {
  cat <<EOF
usage: $PROG <saved-view-log> [-o <outdir>]
       $PROG -h | --help

Extracts every [PCAP-BEGIN <name>] ... [PCAP-END] block of a saved CarSky
View Log into a .pcap file, ready for Wireshark.

  <saved-view-log>  log file saved from the ADA ECU node's View Log ("-" is not
                    supported: the output location is derived from this path)
  -o <outdir>       write the .pcap files here instead of next to the input log
                    (the directory must already exist)
  -h, --help        print this help and exit

Exit: 0 all blocks extracted | 1 no block found | 2 usage error |
      3 a block failed (others still extracted)
EOF
}

# Diagnostics go to stderr so stdout stays a clean list of extracted files.
warn() {
  printf '%s: %s\n' "$PROG" "$*" >&2
}

# ---- argument parsing -------------------------------------------------------

LOG=""
OUTDIR=""

while [ "$#" -gt 0 ]; do
  case "$1" in
    -h|--help)
      usage
      exit 0
      ;;
    -o)
      if [ "$#" -lt 2 ]; then
        warn "-o requires a directory argument"
        usage >&2
        exit 2
      fi
      OUTDIR="$2"
      shift 2
      ;;
    --)
      shift
      break
      ;;
    -*)
      warn "unknown option: $1"
      usage >&2
      exit 2
      ;;
    *)
      if [ -n "$LOG" ]; then
        warn "unexpected extra argument: $1"
        usage >&2
        exit 2
      fi
      LOG="$1"
      shift
      ;;
  esac
done

# Trailing operand after `--`.
if [ "$#" -gt 0 ]; then
  if [ -n "$LOG" ]; then
    warn "unexpected extra argument: $1"
    usage >&2
    exit 2
  fi
  LOG="$1"
  shift
  if [ "$#" -gt 0 ]; then
    warn "unexpected extra argument: $1"
    usage >&2
    exit 2
  fi
fi

if [ -z "$LOG" ]; then
  warn "missing <saved-view-log>"
  usage >&2
  exit 2
fi

if [ ! -f "$LOG" ] || [ ! -r "$LOG" ]; then
  warn "not a readable file: $LOG"
  exit 2
fi

if ! command -v base64 >/dev/null 2>&1; then
  warn "base64 (coreutils) not found in PATH - cannot decode"
  exit 2
fi

# Default output dir = the directory holding the log ("next to the log").
if [ -z "$OUTDIR" ]; then
  case "$LOG" in
    */*) OUTDIR=${LOG%/*} ;;
    *)   OUTDIR="." ;;
  esac
  [ -n "$OUTDIR" ] || OUTDIR="/"
fi

if [ ! -d "$OUTDIR" ] || [ ! -w "$OUTDIR" ]; then
  warn "output directory not writable: $OUTDIR"
  exit 2
fi

# ---- helpers ----------------------------------------------------------------

TMPB64=$(mktemp 2>/dev/null) || {
  warn "cannot create a temporary file"
  exit 2
}
TMPBIN=$(mktemp 2>/dev/null) || {
  rm -f "$TMPB64"
  warn "cannot create a temporary file"
  exit 2
}
trap 'rm -f "$TMPB64" "$TMPBIN"' EXIT HUP INT TERM

# Untrusted-name hardening: file name only, always .pcap (see header).
sanitize_name() {
  local raw="$1" fallback="$2" name
  name=${raw##*/}
  name=${name##*\\}
  case "$name" in
    ""|"."|"..") name="$fallback" ;;
  esac
  case "$name" in
    *.pcap) ;;
    *) name="${name}.pcap" ;;
  esac
  printf '%s' "$name"
}

# Never overwrite: cap.pcap -> cap-2.pcap -> cap-3.pcap ... (see header).
free_path() {
  local name="$1" stem path i
  path="$OUTDIR/$name"
  if [ ! -e "$path" ]; then
    printf '%s' "$path"
    return 0
  fi
  stem=${name%.pcap}
  for i in $(seq 2 99); do
    path="$OUTDIR/${stem}-${i}.pcap"
    if [ ! -e "$path" ]; then
      printf '%s' "$path"
      return 0
    fi
  done
  return 1
}

# ---- scan --------------------------------------------------------------------

BLOCKS=0
EXTRACTED=0
FAILED=0
IN_BLOCK=0
RAW_NAME=""
NOISE=0
LINENO_BEGIN=0
LINE_NO=0

# Decode the buffered base64 of the block that just closed.
finish_block() {
  local name path size
  name=$(sanitize_name "$RAW_NAME" "block-${BLOCKS}.pcap")
  if [ "$name" != "$RAW_NAME" ]; then
    warn "block $BLOCKS: name '$RAW_NAME' sanitized to '$name'"
  fi

  if [ "$NOISE" -gt 0 ]; then
    warn "block $BLOCKS ($name): dropped $NOISE interleaved log line(s) from the base64 body"
  fi

  if [ ! -s "$TMPB64" ]; then
    warn "block $BLOCKS ($name): empty body, nothing to decode"
    FAILED=$((FAILED + 1))
    return
  fi

  if ! path=$(free_path "$name"); then
    warn "block $BLOCKS ($name): 99 name collisions in $OUTDIR - refusing to overwrite"
    FAILED=$((FAILED + 1))
    return
  fi

  if ! base64 -d <"$TMPB64" >"$TMPBIN" 2>/dev/null; then
    warn "block $BLOCKS ($name): base64 decode failed - block corrupt or truncated, skipped"
    FAILED=$((FAILED + 1))
    return
  fi

  if ! cat "$TMPBIN" >"$path" 2>/dev/null; then
    warn "block $BLOCKS ($name): cannot write $path"
    FAILED=$((FAILED + 1))
    return
  fi

  size=$(wc -c <"$path" 2>/dev/null | tr -d ' ')
  printf 'extracted %s (%s bytes)\n' "$path" "$size"
  EXTRACTED=$((EXTRACTED + 1))
}

while IFS= read -r line || [ -n "$line" ]; do
  LINE_NO=$((LINE_NO + 1))
  line=${line%$'\r'}

  if [[ $line =~ ^[[:space:]]*\[PCAP-BEGIN[[:space:]]+(.+)\][[:space:]]*$ ]]; then
    if [ "$IN_BLOCK" -eq 1 ]; then
      warn "line $LINE_NO: new [PCAP-BEGIN] while block $BLOCKS (opened at line $LINENO_BEGIN) is still open - previous block truncated, skipped"
      FAILED=$((FAILED + 1))
    fi
    BLOCKS=$((BLOCKS + 1))
    RAW_NAME="${BASH_REMATCH[1]}"
    IN_BLOCK=1
    NOISE=0
    LINENO_BEGIN=$LINE_NO
    : >"$TMPB64"
    continue
  fi

  if [ "$IN_BLOCK" -eq 0 ]; then
    continue # ordinary log content outside any block
  fi

  if [[ $line =~ ^[[:space:]]*\[PCAP-END\][[:space:]]*$ ]]; then
    IN_BLOCK=0
    finish_block
    continue
  fi

  # Inside a block: strip surrounding whitespace, drop blanks, and drop lines
  # the co-writing consumers interleaved (no `[` or space exists in base64).
  line=${line#"${line%%[![:space:]]*}"}
  line=${line%"${line##*[![:space:]]}"}
  [ -n "$line" ] || continue
  case "$line" in
    \[*)
      NOISE=$((NOISE + 1))
      continue
      ;;
    *[[:space:]]*)
      NOISE=$((NOISE + 1))
      continue
      ;;
  esac
  printf '%s\n' "$line" >>"$TMPB64"
done <"$LOG"

if [ "$IN_BLOCK" -eq 1 ]; then
  warn "block $BLOCKS ('$RAW_NAME', opened at line $LINENO_BEGIN) has no [PCAP-END] - log truncated mid-export, not written"
  FAILED=$((FAILED + 1))
fi

if [ "$BLOCKS" -eq 0 ]; then
  warn "no [PCAP-BEGIN ...] block found in $LOG - is this a saved View Log of a node running capture.sh?"
  exit 1
fi

printf '%s: %s of %s block(s) extracted into %s\n' "$PROG" "$EXTRACTED" "$BLOCKS" "$OUTDIR"

if [ "$FAILED" -gt 0 ]; then
  warn "$FAILED block(s) failed - see messages above"
  exit 3
fi

exit 0
