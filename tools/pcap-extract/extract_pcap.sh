#!/usr/bin/env bash
#
# extract_pcap.sh -- saved node logs in, .pcap files out.
#
# A capturing node emits its rotated pcap to stdout as base64 between markers,
# because the log is the node's only egress:
#
#     [PCAP-BEGIN <basename>]
#     <base64, one or more lines>
#     [PCAP-END]
#
# A saved log interleaves those blocks with unrelated [CAP], [EVT] and [BOOT]
# lines; everything outside a block is ignored. The reader is node-neutral --
# any log carrying the markers is fair input, whichever node wrote it.
#
# The block format is frozen by the producers, V2X_ECU/capture.sh and
# ADA_ECU/capture.sh -- change it there, then in every reader. The reader beside
# this one is Extract-Pcap.ps1, which needs nothing but PowerShell where this
# needs a POSIX shell and coreutils base64; the two are kept behaviourally
# identical.
#
# Deliberate behaviours, none silent:
#   - Output lands NEXT TO THE INPUT unless -o is given. Given a directory, that
#     is the directory itself -- the layout the log collector writes,
#     ./test-report/<test-name>/, so the .pcap files land beside the logs.
#   - The .pcap is named after the INPUT LOG, not the embedded marker name:
#     node-v2x-ecu.txt -> node-v2x-ecu.pcap. The <basename> in [PCAP-BEGIN] is
#     cosmetic only and never reaches the filesystem.
#   - No silent clobber: an existing target gets -2, -3, ... before .pcap, and
#     after -99 the block fails rather than overwriting -- this is also how a
#     second block from the same log gets its own file.
#   - CRLF tolerant, because a log saved from a browser on Windows carries \r.
#   - Both stdout consumers share one stream, so a [CAP]/[EVT]/[BOOT] line can
#     land inside a base64 block. Those are dropped with a warning rather than
#     poisoning the decode -- unambiguous, since base64 has neither brackets nor
#     spaces.
#   - A truncated block -- no [PCAP-END] before the end of the log or before the
#     next [PCAP-BEGIN] -- is reported and NOT written: half a file must not
#     masquerade as a complete capture.
#   - An empty body decodes to nothing, so it is failed rather than written: a
#     zero-byte file is not a capture.
#   - One failing block never hides the healthy ones, in one log or across a
#     directory of them.
#
# Exit status: 0 every block extracted | 1 no block found | 2 usage error |
# 3 at least one block failed.

set -u

readonly PROG=${0##*/}

usage() {
    cat <<EOF
Usage: $PROG [-o <outdir>] <log-file|log-dir>
       $PROG -h | --help

Extract [PCAP-BEGIN <name>] ... [PCAP-END] base64 blocks from saved node logs
into .pcap files, ready for Wireshark.

Arguments:
  <log-file>    One saved node log.
  <log-dir>     A directory of them: every 'node-*.txt' in it is processed and
                the .pcap files land in that directory -- which is the layout
                the log collector writes, ./test-report/<test-name>/.

Options:
  -o <outdir>   Write the .pcap files here instead of beside the input.
                Must exist. Applies to both the file and the directory form.
  -h, --help    This text.

Examples:
  $PROG ./test-report/system-test
        Extracts from every node log of that run.
  $PROG ./test-report/system-test/node-v2x-ecu.txt
        Extracts from one log.
  $PROG -o /tmp/caps ./test-report/system-test

Exit status:
  0  every block extracted      2  usage error
  1  no block found             3  at least one block failed
EOF
}

# Diagnostics go to stderr so stdout stays the clean list of what was written.
warn() { printf 'WARNING: %s\n' "$*" >&2; }
err()  { printf '%s: %s\n' "$PROG" "$*" >&2; }

die2() { err "$*"; exit 2; }

usage_err() { err "$*"; usage >&2; exit 2; }

# Absolute path of an existing file or directory, so every message and every
# default output location names one unambiguous place.
abspath() {
    local p=$1 d b
    if [[ -d $p ]]; then
        ( cd -- "$p" && pwd )
    else
        d=${p%/*}
        b=${p##*/}
        [[ $d == "$p" ]] && d=.
        [[ -z $d ]] && d=/
        printf '%s/%s' "$( cd -- "$d" && pwd )" "$b"
    fi
}

# 1234567 -> 1,234,567, matching the PowerShell port's {N0} byte counts.
group_digits() {
    local n=$1 out=''
    while (( ${#n} > 3 )); do
        out=",${n: -3}$out"
        n=${n:0:${#n}-3}
    done
    printf '%s%s' "$n" "$out"
}

trim() {
    local s=$1
    s=${s#"${s%%[![:space:]]*}"}
    s=${s%"${s##*[![:space:]]}"}
    printf '%s' "$s"
}

# Never overwrite: insert -2, -3, ... before the suffix, give up after -99.
# Prints the free path, or returns 1 when every candidate is taken.
free_path() {
    local dir=$1 name=$2 stem i try
    if [[ ! -e "$dir/$name" ]]; then
        printf '%s' "$dir/$name"
        return 0
    fi
    stem=${name%.pcap}                     # every caller passes a name ending .pcap
    for (( i = 2; i <= 99; i++ )); do
        try="$dir/$stem-$i.pcap"
        if [[ ! -e $try ]]; then
            printf '%s' "$try"
            return 0
        fi
    done
    return 1
}

# ----------------------------------------------------------------- extract one
#
# Per-file state is global: a bash function cannot return a record, and running
# the scan in a pipeline would put the counters in a subshell that discards them.
f_found=0
f_written=0
f_failed=0
f_dropped=0

in_block=0
raw_name=''
buffer=''
base_name=''

# close_block <truncated 0|1> <file-label> <outdir>
close_block() {
    local truncated=$1 label=$2 dest=$3
    local path bytes

    f_found=$(( f_found + 1 ))

    if (( truncated )); then
        warn "$label block $f_found ($raw_name): no [PCAP-END] - truncated, not written"
        f_failed=$(( f_failed + 1 ))
        return
    fi

    if [[ -z $buffer ]]; then
        warn "$label block $f_found ($base_name): empty body, nothing to decode"
        f_failed=$(( f_failed + 1 ))
        return
    fi

    if ! path=$(free_path "$dest" "$base_name"); then
        warn "$label block $f_found ($base_name): 99 files of that name exist - not written"
        f_failed=$(( f_failed + 1 ))
        return
    fi

    # Decode to the scratch file first, so a failed decode never creates -- nor
    # truncates -- the target it was going to claim.
    if ! printf '%s\n' "$buffer" | base64 -d >"$TMPBIN" 2>/dev/null; then
        warn "$label block $f_found ($base_name): base64 decode failed - not valid base64"
        f_failed=$(( f_failed + 1 ))
        return
    fi

    if ! cat "$TMPBIN" >"$path" 2>/dev/null; then
        warn "$label block $f_found ($base_name): cannot write $path"
        f_failed=$(( f_failed + 1 ))
        return
    fi

    bytes=$(wc -c <"$path")
    bytes=${bytes//[[:space:]]/}
    f_written=$(( f_written + 1 ))
    printf '  wrote %s  (%s bytes)\n' "${path##*/}" "$(group_digits "$bytes")"
}

# extract_one <file> <label> <outdir>
extract_one() {
    local file=$1 label=$2 dest=$3
    local line

    f_found=0; f_written=0; f_failed=0; f_dropped=0
    in_block=0; raw_name=''; buffer=''
    base_name="${label%.*}.pcap"           # node-v2x-ecu.txt -> node-v2x-ecu.pcap

    while IFS= read -r line || [[ -n $line ]]; do
        line=${line//$'\r'/}

        if (( ! in_block )); then
            if [[ $line =~ ^[[:space:]]*\[PCAP-BEGIN[[:space:]]*(.*)\][[:space:]]*$ ]]; then
                in_block=1
                raw_name=$(trim "${BASH_REMATCH[1]}")
                buffer=''
            fi
            continue                       # ordinary log content outside a block
        fi

        if [[ $line =~ ^[[:space:]]*\[PCAP-END\][[:space:]]*$ ]]; then
            close_block 0 "$label" "$dest"
            in_block=0
            continue
        fi

        # A second [PCAP-BEGIN before the END means the first block was truncated.
        if [[ $line =~ ^[[:space:]]*\[PCAP-BEGIN[[:space:]]*(.*)\][[:space:]]*$ ]]; then
            close_block 1 "$label" "$dest"
            raw_name=$(trim "${BASH_REMATCH[1]}")
            buffer=''
            continue
        fi

        # Interleaved log line inside the block. Unambiguous: the base64 alphabet
        # carries neither a bracket nor a space, so any line holding one is the
        # co-writing consumer's, not the capture's.
        if [[ $line == *"["* || $line == *"]"* || $line =~ [[:space:]] ]]; then
            [[ -n $(trim "$line") ]] && f_dropped=$(( f_dropped + 1 ))
            continue
        fi

        buffer+=$line
    done <"$file"

    (( in_block )) && close_block 1 "$label" "$dest"

    if (( f_dropped )); then
        warn "$label: $f_dropped interleaved log line(s) dropped from inside blocks"
    fi

    return 0
}

# ------------------------------------------------------------------------ main

input=''
out_dir=''

while (( $# )); do
    case $1 in
        -h|--help) usage; exit 0 ;;
        -o)
            (( $# >= 2 )) || usage_err "-o requires a directory argument"
            out_dir=$2
            shift 2
            ;;
        --) shift; break ;;
        -*) usage_err "unknown option: $1" ;;
        *)
            [[ -z $input ]] || usage_err "unexpected extra argument: $1"
            input=$1
            shift
            ;;
    esac
done

# Operands after `--`.
while (( $# )); do
    [[ -z $input ]] || usage_err "unexpected extra argument: $1"
    input=$1
    shift
done

[[ -n $input ]] || usage_err "missing <log-file|log-dir>"

command -v base64 >/dev/null 2>&1 || die2 "base64 (coreutils) not found in PATH - cannot decode"

# ------------------------------------------------------------------ resolve in

[[ -e $input ]] || die2 "input not found: $input"
resolved=$(abspath "$input")

inputs=()
if [[ -d $resolved ]]; then
    # The collector names every container node's log node-<slug>.txt; that
    # prefix is the contract between the two tools.
    while IFS= read -r f; do
        [[ -n $f ]] && inputs+=("$f")
    done < <(find "$resolved" -maxdepth 1 -type f -name 'node-*.txt' -print | LC_ALL=C sort)

    (( ${#inputs[@]} )) || die2 "no node-*.txt in $resolved - is this a test-report run folder?"
    default_out=$resolved
else
    [[ -f $resolved && -r $resolved ]] || die2 "not a readable file: $input"
    inputs=("$resolved")
    default_out=${resolved%/*}
    [[ -n $default_out ]] || default_out=/
fi

if [[ -n $out_dir ]]; then
    [[ -d $out_dir ]] || die2 "output directory does not exist: $out_dir"
    out_dir=$(abspath "$out_dir")
else
    out_dir=$default_out
fi

[[ -w $out_dir ]] || die2 "output directory not writable: $out_dir"

TMPBIN=$(mktemp 2>/dev/null) || die2 "cannot create a temporary file"
trap 'rm -f "$TMPBIN"' EXIT HUP INT TERM

# --------------------------------------------------------------------- extract

total_found=0
total_written=0
total_failed=0

for file in "${inputs[@]}"; do
    label=${file##*/}
    if (( ${#inputs[@]} > 1 )); then
        printf '%s:\n' "$label"
    fi
    extract_one "$file" "$label" "$out_dir"
    total_found=$(( total_found + f_found ))
    total_written=$(( total_written + f_written ))
    total_failed=$(( total_failed + f_failed ))
done

if (( total_found == 0 )); then
    warn "no [PCAP-BEGIN ...] block in $resolved"
    printf '%s\n' 'A node captures only with "capabilities": ["NET_RAW"] in its config.'
    printf '%s\n' 'The isolated tests'"'"' stand-in nodes do not capture at all.'
    exit 1
fi

printf '\n'
printf '%s of %s block(s) extracted to %s\n' "$total_written" "$total_found" "$out_dir"

if (( total_failed )); then
    warn "$total_failed block(s) failed - see the messages above"
    exit 3
fi

exit 0
