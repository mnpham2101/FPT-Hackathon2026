#!/usr/bin/env bash
# collect-logs.sh -- the bash port of Collect-Logs.ps1.
#
# One-shot evidence collector: pick a test, pull every node log off CarSky and every
# guest-side log off the AAOS IVI over ADB, into ./test-report/<test-name>/.
#
# Automates Step 3 "Evidence collection" of documents/Delivery/testing-guide.md,
# which stays the authority. Same behaviour, same output layout and same on-screen
# structure as the PowerShell version -- change one, change the other.
#
# Nothing here deploys, installs or mutates the Room: it only reads. A run that finds
# the deployment stopped stops too, because logs of a Room that is not running are
# evidence of nothing.
#
# What it collects, into ./test-report/<test-name>/ relative to the current directory:
#
#     node-<slug>.txt              one per container node, from the REST log endpoint
#     skycraft-<slug>-vmhost.txt   the Skycraft VM host's own output (see below)
#     app-logcat.txt               the IVI app's tagged logcat -- the [RX] evidence
#     app-crash.txt                Android's separate crash buffer
#     guest-udp6.txt               /proc/net/udp6, proving the socket is bound
#     guest-ifaces.txt             ip -4 addr, proving the guest took the pin address
#     summary.txt                  this run's checklist, in plain text
#
# The Skycraft node's REST log is the VM host's WebRTC/GPU output and carries nothing
# the app wrote, so it is collected for completeness under a name the pcap extractor
# does not glob and the [TX] scan does not read. The app's log comes only from adb.
#
# The ADB tunnel is optional. Without it the CarSky half still lands and the guest half
# is skipped, reported rather than failed.
#
# Dependencies: bash, curl, awk, sed. No jq -- the JSON is parsed by the awk helpers
# below, so the script runs on a bare Linux box and in Git Bash alike.
#
# Exit status: 0 collected | 1 the deployment is not running, or another hard stop |
# 2 usage error.

set -u

# ------------------------------------------------------------------- defaults

BASE_URL="https://hackathon-2.carsky.io"
PORT=5555
TAIL=5000
OUT_ROOT="./test-report"
API_KEY_FILE=""
TEST_ARG=""
BLUEPRINT_ARG=""
PACKAGE="com.hackathon.v2x.ivi"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

usage() {
    cat <<EOF

  collect-logs.sh -- CarSky node logs + AAOS guest evidence for one test run

  Usage: collect-logs.sh [--test <1-5|name> | --blueprint <name>] [options]

    --test <1-5|name>   shortcut for one of the blueprints below; asks if omitted
                          1  system-test         (blueprint m1_system_test)
                          2  ivi-isolated-test   (blueprint phase5_smoked_test)
                          3  ada-isolated-test   (blueprint phase4_smoked_test)
                          4  v2x-isolated-test   (blueprint phase1_smoked_test)
                          5  netcheck-test       (blueprint phase0_smoked_test)
    --blueprint <name>  collect ANY deployed blueprint by name, listed or not. The
                        Room is read from its node list, so every node it contains
                        is collected whatever the node is, and the guest half runs
                        only if one of them is a Skycraft VM.
    --base-url <url>    CarSky gateway            (default $BASE_URL)
    --api-key-file <f>  file holding the REST key (default <repo>/secrets/carsky-api-key.txt)
    --port <n>          local ADB tunnel port     (default $PORT)
    --tail <n>          lines per node log        (default $TAIL)
    --out-root <dir>    where the run folder goes (default $OUT_ROOT, relative to cwd)
    -h, --help          this text

  The API key is never printed, never written to any output file, and never put in a URL.

EOF
}

exit_usage() {
    printf '\n  USAGE: %s\n\n' "$1" >&2
    exit 2
}

while [ $# -gt 0 ]; do
    case "$1" in
        --test)          [ $# -ge 2 ] || exit_usage "--test needs a value";          TEST_ARG="$2"; shift 2 ;;
        --blueprint)     [ $# -ge 2 ] || exit_usage "--blueprint needs a value";     BLUEPRINT_ARG="$2"; shift 2 ;;
        --base-url)      [ $# -ge 2 ] || exit_usage "--base-url needs a value";      BASE_URL="$2"; shift 2 ;;
        --api-key-file)  [ $# -ge 2 ] || exit_usage "--api-key-file needs a value";  API_KEY_FILE="$2"; shift 2 ;;
        --port)          [ $# -ge 2 ] || exit_usage "--port needs a value";          PORT="$2"; shift 2 ;;
        --tail)          [ $# -ge 2 ] || exit_usage "--tail needs a value";          TAIL="$2"; shift 2 ;;
        --out-root)      [ $# -ge 2 ] || exit_usage "--out-root needs a value";      OUT_ROOT="$2"; shift 2 ;;
        -h|--help)       usage; exit 0 ;;
        *)               exit_usage "unknown argument '$1'" ;;
    esac
done

case "$PORT" in ''|*[!0-9]*) exit_usage "--port must be a number, got '$PORT'" ;; esac
case "$TAIL" in ''|*[!0-9]*) exit_usage "--tail must be a number, got '$TAIL'" ;; esac

# ---------------------------------------------------------------- presentation

if [ -t 1 ]; then
    C_CYAN=$'\033[36m'; C_GREEN=$'\033[32m'; C_YELLOW=$'\033[33m'
    C_RED=$'\033[31m';  C_GRAY=$'\033[90m';  C_WHITE=$'\033[97m'; C_OFF=$'\033[0m'
else
    C_CYAN=''; C_GREEN=''; C_YELLOW=''; C_RED=''; C_GRAY=''; C_WHITE=''; C_OFF=''
fi

STEP_NO=0
write_step() { STEP_NO=$((STEP_NO + 1)); printf '\n%s[%d] %s%s\n' "$C_CYAN" "$STEP_NO" "$1" "$C_OFF"; }
write_ok()   { printf '    %sOK    %s%s\n' "$C_GREEN"  "$1" "$C_OFF"; }
write_warn() { printf '    %sWARN  %s%s\n' "$C_YELLOW" "$1" "$C_OFF"; }
write_info() { printf '          %s%s%s\n' "$C_GRAY"   "$1" "$C_OFF"; }

# write_check <0|1> <name> <detail>  -- the [x]/[ ] checklist row, padded to 24.
write_check() {
    if [ "$1" -eq 1 ]; then printf '    %s[x] %-24s %s%s\n' "$C_GREEN"  "$2" "$3" "$C_OFF"
    else                    printf '    %s[ ] %-24s %s%s\n' "$C_YELLOW" "$2" "$3" "$C_OFF"; fi
}

# [-] is the third state: not a pass and not a failure, but nothing this Room can
# answer -- a node with no log stream, or guest evidence in a Room with no guest.
# Reporting those as [ ] would read as a defect in a Room that is behaving.
write_skip() { printf '    %s[-] %-24s %s%s\n' "$C_GRAY" "$1" "$2" "$C_OFF"; }

# summary.txt is the same checklist without the colour, so a run can be read back
# after the terminal is gone. Every line printed as a result is echoed into it.
SUMMARY=""
add_summary()       { SUMMARY="${SUMMARY}$1
"; }
add_summary_check() {
    if [ "$1" -eq 1 ]; then add_summary "$(printf '[x] %-24s %s' "$2" "$3")"
    else                    add_summary "$(printf '[ ] %-24s %s' "$2" "$3")"; fi
}
add_summary_skip() { add_summary "$(printf '[-] %-24s %s' "$1" "$2")"; }

fail() {
    printf '\n  %sFAILED: %s%s\n' "$C_RED" "$1" "$C_OFF" >&2
    if [ -n "${2:-}" ]; then printf '  %sFix:    %s%s\n' "$C_YELLOW" "$2" "$C_OFF" >&2; fi
    printf '\n' >&2
    exit 1
}

# ---------------------------------------------------------------- json helpers

# No jq here, and none assumed. Two awk readers cover every shape this API returns.

# json_str <key> -- first "key":"value" of the JSON on stdin. Ids, names, statuses
# and phases are plain tokens, so a non-greedy field match is enough for them.
json_str() {
    sed -n 's/.*"'"$1"'"[[:space:]]*:[[:space:]]*"\([^"]*\)".*/\1/p' | sed -n '1p'
}

# json_objects -- splits a top-level JSON array into one object per line, so each
# can then be read with json_str. Only the separator is rewritten; nothing is parsed.
json_objects() {
    sed 's/}[[:space:]]*,[[:space:]]*{/}\
{/g' | sed 's/^\[//; s/\][[:space:]]*$//' | sed '/^[[:space:]]*$/d'
}

# json_lines -- prints one element of the "lines" array per output line, unescaped.
# Log text carries \" \\ \n and the odd \uXXXX, so a split on the comma would corrupt
# it; this walks real string literals instead. The regex does the scanning and the
# per-character work happens only inside one matched literal, which keeps it quick
# even on a 5000-line log.
json_lines() {
    awk '
    function unesc(s,   out, i, c, e) {
        out = ""; i = 1
        while (i <= length(s)) {
            c = substr(s, i, 1)
            if (c == "\\") {
                e = substr(s, i + 1, 1)
                if (e == "n") out = out "\n"
                else if (e == "t") out = out "\t"
                else if (e == "r" || e == "b" || e == "f") out = out ""
                else if (e == "u") { out = out "?"; i += 4 }
                else out = out e
                i += 2
            } else { out = out c; i++ }
        }
        return out
    }
    { buf = buf $0 }
    END {
        k = index(buf, "\"lines\"")
        if (k == 0) exit 1
        rest = substr(buf, k + 7)
        b = index(rest, "[")
        if (b == 0) exit 1
        rest = substr(rest, b + 1)
        while (match(rest, /"([^"\\]|\\.)*"/)) {
            # Anything but whitespace and commas before the next literal means the
            # array has ended and this quote belongs to a later key.
            pre = substr(rest, 1, RSTART - 1)
            if (pre ~ /\]/) break
            print unesc(substr(rest, RSTART + 1, RLENGTH - 2))
            rest = substr(rest, RSTART + RLENGTH)
        }
    }'
}

# http_get <path> -- GET $API<path>. Body lands in HTTP_BODY, status in HTTP_CODE.
# The key travels in a header and is never interpolated into the URL or echoed.
HTTP_BODY=""
HTTP_CODE=""
http_get() {
    local tmp
    tmp="$(mktemp)" || fail "cannot create a temporary file" "Check TMPDIR is writable."
    HTTP_CODE="$(curl -sS --max-time 60 -o "$tmp" -w '%{http_code}' \
                      -H "X-API-Key: ${API_KEY}" "${API}$1" 2>/dev/null)" || HTTP_CODE="000"
    HTTP_BODY="$(cat "$tmp")"
    rm -f "$tmp"
}

# urlenc -- percent-encodes a blueprint name for the query string.
urlenc() {
    printf '%s' "$1" | awk '
    BEGIN { for (i = 0; i < 256; i++) ord[sprintf("%c", i)] = i }
    {
        for (i = 1; i <= length($0); i++) {
            c = substr($0, i, 1)
            if (c ~ /[A-Za-z0-9._~-]/) printf "%s", c
            else printf "%%%02X", ord[c]
        }
    }'
}

# slug <displayName> -- lowercase, non-alphanumerics collapsed to '-', ends trimmed.
# "MOCKED ADA ECU" -> mocked-ada-ecu. The node-<slug>.txt prefix is the contract
# tools/pcap-extract/Extract-Pcap.ps1 globs on.
slug() {
    printf '%s' "$1" | tr '[:upper:]' '[:lower:]' | sed 's/[^a-z0-9]\{1,\}/-/g; s/^-//; s/-$//'
}

count_lines() { if [ -f "$1" ]; then awk 'END { print NR + 0 }' "$1"; else printf '0'; fi }

# ------------------------------------------------------------------- the tests

# One row per shortcut: option, folder the evidence lands in, the CarSky blueprint
# it is deployed from, and the one-line description. These are conveniences only --
# --blueprint collects anything deployed, so a Room absent from this table is still
# collectable and adding a row is about saving typing, not about granting access.
#
# Blueprint names are the platform's own, underscore-separated. The deployment
# CarSky builds from one is named <blueprint>-deploy, and that is the name
# /deployments/find matches and the name the summary reports -- so the suffix is
# derived below, never typed into a row.
TESTS="1|system-test|m1_system_test|full blueprint - bench, V2X, ADA, IVI
2|ivi-isolated-test|phase5_smoked_test|IVI ECU with the mocked ADA producer
3|ada-isolated-test|phase4_smoked_test|ADA ECU with its bench
4|v2x-isolated-test|phase1_smoked_test|V2X ECU with the scenario player
5|netcheck-test|phase0_smoked_test|the phase 0 platform baseline, three netcheck nodes"

TEST_NAME=""
TEST_BLUEPRINT=""
TEST_WHAT=""
resolve_test() {
    local want="$1" opt name bp what
    TEST_NAME=""; TEST_BLUEPRINT=""; TEST_WHAT=""
    while IFS='|' read -r opt name bp what; do
        [ -n "$opt" ] || continue
        if [ "$want" = "$opt" ] || [ "$want" = "$name" ]; then
            TEST_NAME="$name"; TEST_BLUEPRINT="$bp"; TEST_WHAT="$what"
            return 0
        fi
    done <<EOF
$TESTS
EOF
    return 1
}

printf '\n  %sTest log collector - CarSky node logs + AAOS guest evidence%s\n' "$C_WHITE" "$C_OFF"
printf '  %sAuthority: documents/Delivery/testing-guide.md Step 3%s\n' "$C_GRAY" "$C_OFF"

command -v curl >/dev/null 2>&1 || fail "curl is not installed." "It is how this script talks to CarSky."
command -v awk  >/dev/null 2>&1 || fail "awk is not installed."  "It is how this script reads the API's JSON."

write_step "Choosing the blueprint"

if [ -n "$TEST_ARG" ] && [ -n "$BLUEPRINT_ARG" ]; then
    exit_usage "--test and --blueprint both given. Pass one."
fi

if [ -n "$BLUEPRINT_ARG" ]; then
    # Any blueprint, named or not. A shortcut row is reused when one matches, purely
    # so the run folder and the description read the same as the shortcut would.
    TEST_NAME=""; TEST_BLUEPRINT=""; TEST_WHAT=""
    while IFS='|' read -r opt name bp what; do
        [ -n "$opt" ] || continue
        if [ "$bp" = "$BLUEPRINT_ARG" ]; then
            TEST_NAME="$name"; TEST_BLUEPRINT="$bp"; TEST_WHAT="$what"
            break
        fi
    done <<EOF
$TESTS
EOF
    if [ -z "$TEST_BLUEPRINT" ]; then
        TEST_BLUEPRINT="$BLUEPRINT_ARG"
        TEST_NAME="$(slug "$BLUEPRINT_ARG")"
        TEST_WHAT="collected by name"
    fi
elif [ -n "$TEST_ARG" ]; then
    resolve_test "$TEST_ARG" || exit_usage "unknown shortcut '$TEST_ARG'. Use 1-5, one of: system-test, ivi-isolated-test, ada-isolated-test, v2x-isolated-test, netcheck-test, or --blueprint <name> for any other."
else
    printf '\n'
    while IFS='|' read -r opt name bp what; do
        [ -n "$opt" ] || continue
        printf '    %s%s) %-20s %s%s\n' "$C_GRAY" "$opt" "$name" "$what" "$C_OFF"
    done <<EOF
$TESTS
EOF
    printf '\n'
    printf '    %sAny other blueprint: re-run with --blueprint <name>%s\n' "$C_GRAY" "$C_OFF"
    printf '\n'
    printf '    Which test? [1-5]: '
    read -r answer || answer=""
    resolve_test "$answer" || exit_usage "'$answer' is not one of 1-5. For any other blueprint use --blueprint <name>."
fi

write_ok "$TEST_NAME  -  $TEST_WHAT"
# CarSky names a deployment after its blueprint with a -deploy suffix.
WANTED_DEPLOYMENT="$TEST_BLUEPRINT-deploy"
write_info "Looking for '$WANTED_DEPLOYMENT', the deployment of blueprint '$TEST_BLUEPRINT'."

# ------------------------------------------------------------------ credentials

write_step "Reading the CarSky API key"

if [ -z "$API_KEY_FILE" ]; then API_KEY_FILE="$REPO_ROOT/secrets/carsky-api-key.txt"; fi
[ -f "$API_KEY_FILE" ] || fail "No API key. $API_KEY_FILE does not exist." \
    "Put the CarSky REST API key in that file, or pass --api-key-file <path>. It is git-ignored."

API_KEY="$(tr -d ' \t\r\n' < "$API_KEY_FILE")"
[ -n "$API_KEY" ] || fail "The API key file is empty." "Paste the CarSky REST API key into $API_KEY_FILE."

# Length only -- the value itself is never printed, logged or written to any file.
write_ok "Key loaded from $(basename "$API_KEY_FILE") (${#API_KEY} chars, not shown)"

BASE_URL="${BASE_URL%/}"
API="$BASE_URL/api/v1"

# ------------------------------------------------------------------ deployment

write_step "Resolving the deployment on $BASE_URL"

http_get "/deployments/find?blueprint=$(urlenc "$WANTED_DEPLOYMENT")"
if [ "$HTTP_CODE" = "000" ]; then
    fail "The find call did not reach $BASE_URL." "Check the gateway is reachable. Nothing was collected."
fi
if [ "$HTTP_CODE" != "200" ]; then
    fail "The find call answered HTTP $HTTP_CODE." "401 or 403 means the API key is wrong or expired. Nothing was collected."
fi

# The endpoint answers with an array, and an unmatched name is 200 with an empty one
# rather than a 404 -- so 'no rows' is the not-deployed signal, not an HTTP status.
DEP_OBJECTS="$(printf '%s' "$HTTP_BODY" | json_objects)"
DEP_COUNT="$(printf '%s' "$DEP_OBJECTS" | grep -c 'roomId' || true)"
if [ -z "$DEP_OBJECTS" ] || [ "$DEP_COUNT" = "0" ]; then
    fail "'$WANTED_DEPLOYMENT' is not running in CarSky - nothing matches that name." \
         "Deploy $TEST_BLUEPRINT, wait for its nodes to go green, then re-run."
fi

DEP_OBJ="$(printf '%s' "$DEP_OBJECTS" | sed -n '1p')"
if [ "$DEP_COUNT" -gt 1 ]; then
    EXACT="$(printf '%s' "$DEP_OBJECTS" | grep -F "\"name\":\"$WANTED_DEPLOYMENT\"" | sed -n '1p' || true)"
    if [ -n "$EXACT" ]; then DEP_OBJ="$EXACT"; fi
    write_warn "$DEP_COUNT deployments match '$WANTED_DEPLOYMENT'; using the first exact or listed match."
fi

DEP_NAME="$(printf '%s' "$DEP_OBJ" | json_str name)"
ROOM_ID="$(printf '%s' "$DEP_OBJ" | json_str roomId)"
DEP_FIND_STATUS="$(printf '%s' "$DEP_OBJ" | json_str status)"
[ -n "$ROOM_ID" ] || fail "The find row carries no roomId." "The API answered a shape this script does not know; report it."

write_ok "$DEP_NAME   room $ROOM_ID"

# The find row carries a status, but the status endpoint is the live one -- a row can
# be stale by a redeploy. Believe the second, and stop unless it says RUNNING.
http_get "/deployments/$ROOM_ID/status"
DEP_STATUS=""
NAMESPACE=""
if [ "$HTTP_CODE" = "200" ]; then
    DEP_STATUS="$(printf '%s' "$HTTP_BODY" | json_str status)"
    NAMESPACE="$(printf '%s' "$HTTP_BODY" | json_str namespace)"
fi
if [ -z "$DEP_STATUS" ]; then DEP_STATUS="$DEP_FIND_STATUS"; fi

DEP_RUNNING=0
if [ "$DEP_STATUS" = "RUNNING" ]; then DEP_RUNNING=1; fi
if [ "$DEP_RUNNING" -ne 1 ]; then
    SHOWN="$DEP_STATUS"
    if [ -z "$SHOWN" ]; then SHOWN="(no status reported)"; fi
    fail "The deployment '$WANTED_DEPLOYMENT' is not running in CarSky: status is $SHOWN." \
         "Start or redeploy that blueprint and wait for RUNNING, then re-run. Logs of a stopped Room prove nothing."
fi
write_ok "status RUNNING   namespace $NAMESPACE"

add_summary "Test log collection - $TEST_NAME"
add_summary "Collected           $(date '+%Y-%m-%d %H:%M:%S')"
add_summary "Gateway             $BASE_URL"
add_summary "Blueprint           $TEST_BLUEPRINT"
add_summary "Deployment sought   $WANTED_DEPLOYMENT"
add_summary "Deployment          $DEP_NAME"
add_summary "Room                $ROOM_ID"
add_summary "Namespace           $NAMESPACE"
add_summary "Log tail            $TAIL lines/node"
add_summary ""
add_summary "BLUEPRINT"
add_summary_check 1 "deployment" "status RUNNING"

# ----------------------------------------------------------------- node health

write_step "Node phases"

http_get "/deployments/$ROOM_ID/nodes"
[ "$HTTP_CODE" = "200" ] || fail "Could not list the nodes: HTTP $HTTP_CODE." \
    "The Room is RUNNING but its node list is unreadable; retry, then report it."

NODE_OBJECTS="$(printf '%s' "$HTTP_BODY" | json_objects)"
[ -n "$NODE_OBJECTS" ] || fail "The Room reports no nodes at all." "Re-check the deployment in the Deployment Viewer."

# One record per node, separated by  (unit separator). Not a tab: tab is IFS
# whitespace, so bash collapses runs of it and a node with an empty displayName
# would shift nodeType into its place.  cannot occur in a display name.
FS=$''
# name<FS>displayName<FS>nodeType<FS>phase, one node per line.
NODE_TABLE=""
NODE_COUNT=0
RUNNING_COUNT=0
ALL_RUNNING=1
while IFS= read -r obj; do
    [ -n "$obj" ] || continue
    n_name="$(printf '%s' "$obj" | json_str name)"
    n_disp="$(printf '%s' "$obj" | json_str displayName)"
    n_type="$(printf '%s' "$obj" | json_str nodeType)"
    n_phase="$(printf '%s' "$obj" | json_str phase)"
    [ -n "$n_name" ] || continue
    NODE_COUNT=$((NODE_COUNT + 1))
    NODE_TABLE="${NODE_TABLE}${n_name}${FS}${n_disp}${FS}${n_type}${FS}${n_phase}
"
    ok=0
    if [ "$n_phase" = "Running" ]; then ok=1; RUNNING_COUNT=$((RUNNING_COUNT + 1)); else ALL_RUNNING=0; fi
    write_check "$ok" "$n_disp" "$n_phase   [$n_type]"
    add_summary_check "$ok" "$n_disp" "$n_phase   [$n_type]"
done <<EOF
$NODE_OBJECTS
EOF

[ "$NODE_COUNT" -gt 0 ] || fail "The Room reports no nodes at all." "Re-check the deployment in the Deployment Viewer."

# app-crash.txt, app-logcat.txt and the two guest probes come off a Skycraft VM
# over ADB. A Room without one has no guest to ask, so the whole ADB half is not
# attempted and its evidence rows read [-] rather than [ ]: absent, not failed.
HAS_SKYCRAFT=0
SKYCRAFT_DISP=""
while IFS="$FS" read -r n_name n_disp n_type n_phase; do
    [ -n "$n_name" ] || continue
    if [ "$n_type" = "skycraft" ] && [ "$HAS_SKYCRAFT" -eq 0 ]; then
        HAS_SKYCRAFT=1; SKYCRAFT_DISP="$n_disp"
    fi
done <<EOF
$NODE_TABLE
EOF

if [ "$ALL_RUNNING" -eq 1 ]; then
    write_ok "All $NODE_COUNT nodes Running - the blueprint is healthy"
else
    # Not fatal: a half-up Room is exactly the case whose logs explain why.
    write_warn "Not every node is Running. Collecting anyway - the logs are what explain it."
fi
add_summary_check "$ALL_RUNNING" "all nodes Running" "$RUNNING_COUNT/$NODE_COUNT Running"

# ------------------------------------------------------------------ run folder

write_step "Preparing the run folder"

mkdir -p "$OUT_ROOT/$TEST_NAME" || fail "Cannot create $OUT_ROOT/$TEST_NAME" "Check the current directory is writable."
OUT_DIR="$(cd "$OUT_ROOT/$TEST_NAME" && pwd)"

write_ok "$OUT_DIR"
write_info "Relative to where you ran this. Existing files of the same name are overwritten."
add_summary ""
add_summary "OUTPUT              $OUT_DIR"

# -------------------------------------------------------------------- node logs

write_step "Collecting node logs over REST"

add_summary ""
add_summary "NODE LOGS"

# Every exported file is named for the node it came from, so a run folder can be
# read without the node list beside it.
#
# The name is built from displayName, not from the REST node name: the latter is an
# opaque key that CarSky mints fresh on every redeploy (l3vaqyshgmtdqzooqernq-n2), so
# naming files after it would make two runs of the same test incomparable. displayName
# is what Nydus shows and what a reader recognises, and it survives a redeploy.
#
# Its one weakness is that nothing stops two nodes sharing a displayName. Where that
# happens -- or where a node has no displayName at all -- the key's trailing segment
# is appended, which is the part that distinguishes nodes within one Room. Names are
# resolved for the whole Room up front, because a collision is only visible from the
# set, never from the node in hand.
BASE_SLUGS=""
while IFS="$FS" read -r n_name n_disp n_type n_phase; do
    [ -n "$n_name" ] || continue
    s="$(slug "$n_disp")"
    if [ -z "$s" ]; then s="$(slug "$n_name")"; fi
    BASE_SLUGS="${BASE_SLUGS}${s}
"
done <<EOF
$NODE_TABLE
EOF

# node_slug <nodeName> <displayName> -- the collision-resolved slug for one node.
node_slug() {
    local nm="$1" disp="$2" s count suffix
    s="$(slug "$disp")"
    if [ -z "$s" ]; then s="$(slug "$nm")"; fi
    count="$(printf '%s' "$BASE_SLUGS" | grep -c -x -- "$s" || true)"
    if [ "$count" -gt 1 ]; then
        suffix="$(slug "${nm##*-}")"
        if [ -z "$suffix" ]; then suffix="$(slug "$nm")"; fi
        s="$s-$suffix"
    fi
    printf '%s' "$s"
}

NODE_FILES=""
while IFS="$FS" read -r n_name n_disp n_type n_phase; do
    [ -n "$n_name" ] || continue
    s="$(node_slug "$n_name" "$n_disp")"

    is_vm=0
    if [ "$n_type" = "skycraft" ]; then is_vm=1; fi

    # A Skycraft node's log is the VM host's own WebRTC/GPU output, never the app's.
    # It is collected for completeness under a name the pcap extractor does not glob
    # and the [TX] scan does not read, so it can never be mistaken for app evidence.
    if [ "$is_vm" -eq 1 ]; then file="$OUT_DIR/skycraft-$s-vmhost.txt"
    else                        file="$OUT_DIR/node-$s.txt"; fi

    # Every node is asked for a log, whatever its type -- the node list is the only
    # thing that decides what gets collected, so a Room of node types this script has
    # never seen still collects. container=user selects the application stream over
    # the platform's sidecar and is what a container answers to; the Skycraft VM has
    # no such container and the Ethernet Bridge 502s it. So both forms are tried, in
    # the order the node type makes likely, and only a node that refuses both is
    # reported as having no log stream.
    last_code=""
    if [ "$n_type" = "container" ]; then
        q1="?container=user&tail=$TAIL"; q2="?tail=$TAIL"
    else
        q1="?tail=$TAIL"; q2="?container=user&tail=$TAIL"
    fi

    http_get "/deployments/$ROOM_ID/logs/$n_name$q1"
    if [ "$HTTP_CODE" != "200" ]; then
        last_code="$HTTP_CODE"
        http_get "/deployments/$ROOM_ID/logs/$n_name$q2"
    fi

    # No stream is not a failure. The Ethernet Bridge has no log to give and never
    # will, and a Room is not less healthy for containing one.
    if [ "$HTTP_CODE" != "200" ]; then
        write_skip "$n_disp" "no log stream [$n_type] - HTTP ${last_code:-$HTTP_CODE}"
        add_summary_skip "$n_disp" "no log stream [$n_type]"
        continue
    fi

    printf '%s' "$HTTP_BODY" | json_lines > "$file" 2>/dev/null || true
    n="$(count_lines "$file")"

    label="$(basename "$file")   $n lines"
    if [ "$is_vm" -eq 1 ]; then label="$label   (VM host output, not app evidence)"; fi
    ok=0
    if [ "$n" -gt 0 ]; then ok=1; fi
    write_check "$ok" "$n_disp" "$label"
    add_summary_check "$ok" "$n_disp" "$label"

    if [ "$is_vm" -eq 0 ]; then NODE_FILES="${NODE_FILES}${file}
"; fi
done <<EOF
$NODE_TABLE
EOF

# -------------------------------------------------------------------------- adb

if [ "$HAS_SKYCRAFT" -eq 0 ]; then
    write_step "Guest evidence"
    write_skip "Skycraft node" "none in this Room - no guest to collect from"
    write_info "app-logcat.txt, app-crash.txt and the guest probes need an AAOS VM node."
    add_summary ""
    add_summary "GUEST               n/a - no Skycraft node in this Room"
else

write_step "Looking for the ADB tunnel on localhost:$PORT"

# No nc here, and none assumed: bash's own /dev/tcp opens the socket, and a failure to
# connect is the answer rather than an error.
test_port() {
    ( exec 3<>"/dev/tcp/127.0.0.1/$1" ) >/dev/null 2>&1
}

# adb: PATH first, then every location an Android SDK is commonly installed to.
# Hardcoding one path breaks on any machine that put the SDK elsewhere. Both the
# .exe and the bare name are tried, so Git Bash and Linux take the same route.
ADB=""
if command -v adb >/dev/null 2>&1; then
    ADB="$(command -v adb)"
else
    for d in \
        "${LOCALAPPDATA:-}/Android/Sdk/platform-tools" \
        "${ANDROID_HOME:-}/platform-tools" \
        "${ANDROID_SDK_ROOT:-}/platform-tools" \
        "${USERPROFILE:-}/Android/Sdk/platform-tools" \
        "${HOME:-}/Android/Sdk/platform-tools" \
        "${HOME:-}/Library/Android/sdk/platform-tools" \
        "${ProgramFiles:-}/Android/Android Studio/platform-tools" \
        "/usr/lib/android-sdk/platform-tools"
    do
        case "$d" in /platform-tools|"/Android/Sdk/platform-tools") continue ;; esac
        if   [ -x "$d/adb.exe" ]; then ADB="$d/adb.exe"; break
        elif [ -x "$d/adb" ];     then ADB="$d/adb";     break; fi
    done
fi

SERIAL="localhost:$PORT"
GUEST_READY=0

if [ -z "$ADB" ]; then
    write_warn "adb not found on PATH or in any standard Android SDK location."
    write_info "Skipping the guest half. Install Android SDK platform-tools, or set ANDROID_HOME."
    add_summary ""
    add_summary "GUEST               skipped - adb not found"
elif ! test_port "$PORT"; then
    write_warn "Nothing is listening on 127.0.0.1:$PORT - the ADB tunnel is down."
    write_info "The node logs above are collected; the guest-side files are skipped, not failed."
    write_info "Open the tunnel:  ./tools/apk-uploader/install-ivi-apk.sh --skip-install --keep-tunnel"
    add_summary ""
    add_summary "GUEST               skipped - no ADB tunnel on localhost:$PORT"
else
    write_ok "adb  ->  $ADB"
    write_ok "port $PORT is serving"

    "$ADB" connect "$SERIAL" >/dev/null 2>&1 || true
    connected=0
    i=0
    while [ "$i" -lt 20 ]; do
        if "$ADB" devices 2>/dev/null | grep -E "^$SERIAL[[:space:]]+device([[:space:]]|$)" >/dev/null 2>&1; then
            connected=1; break
        fi
        sleep 1
        "$ADB" connect "$SERIAL" >/dev/null 2>&1 || true
        i=$((i + 1))
    done

    if [ "$connected" -ne 1 ]; then
        write_warn "The tunnel is serving but $SERIAL never reached state 'device'."
        write_info "Skipping the guest half. The Skycraft node may still be booting."
        add_summary ""
        add_summary "GUEST               skipped - $SERIAL not in state 'device'"
    else
        GUEST_READY=1
        write_ok "$SERIAL  device"
        write_info "Guest node: $SKYCRAFT_DISP. Every adb call below is pinned with -s $SERIAL."
    fi
fi

fi  # end: this Room has a Skycraft node

# ---------------------------------------------------------------- guest health

if [ "$GUEST_READY" -eq 1 ]; then
    write_step "Probing the app on the guest"

    # A node showing Running says the VM is up, never that the app is: a Room can run
    # green for hours with no APK installed at all. Only the guest can answer these.
    APP_VERSION="$("$ADB" -s "$SERIAL" shell "dumpsys package $PACKAGE | grep versionName" 2>/dev/null \
                   | sed -n 's/.*versionName=\([^ ]*\).*/\1/p' | sed -n '1p' | tr -d '\r')"
    APP_PID="$("$ADB" -s "$SERIAL" shell pidof "$PACKAGE" 2>/dev/null | tr -d '\r\n ')"

    ok_ver=0;   [ -n "$APP_VERSION" ] && ok_ver=1
    ok_pid=0;   [ -n "$APP_PID" ]     && ok_pid=1

    ok_res=0
    if "$ADB" -s "$SERIAL" shell "dumpsys activity activities | grep -i ResumedActivity" 2>/dev/null \
        | grep -F "$PACKAGE/.MainActivity" >/dev/null 2>&1; then ok_res=1; fi
    ok_foc=0
    if "$ADB" -s "$SERIAL" shell "dumpsys window | grep -i mCurrentFocus" 2>/dev/null \
        | grep -F "$PACKAGE" >/dev/null 2>&1; then ok_foc=1; fi
    ok_awk=0
    if "$ADB" -s "$SERIAL" shell "dumpsys power | grep -i mWakefulness=" 2>/dev/null \
        | grep -F "mWakefulness=Awake" >/dev/null 2>&1; then ok_awk=1; fi

    d_ver="$PACKAGE NOT FOUND";                if [ "$ok_ver" -eq 1 ]; then d_ver="$PACKAGE $APP_VERSION"; fi
    d_pid="no process";                        if [ "$ok_pid" -eq 1 ]; then d_pid="pid $APP_PID"; fi
    d_res="not the resumed activity";          if [ "$ok_res" -eq 1 ]; then d_res="topResumedActivity"; fi
    d_foc="another window has focus";          if [ "$ok_foc" -eq 1 ]; then d_foc="holds input focus"; fi
    d_awk="asleep - the HMI will not render";  if [ "$ok_awk" -eq 1 ]; then d_awk="mWakefulness=Awake"; fi

    add_summary ""
    add_summary "GUEST"
    write_check "$ok_ver" "package installed" "$d_ver"; add_summary_check "$ok_ver" "package installed" "$d_ver"
    write_check "$ok_pid" "process running"   "$d_pid"; add_summary_check "$ok_pid" "process running"   "$d_pid"
    write_check "$ok_res" "MainActivity up"   "$d_res"; add_summary_check "$ok_res" "MainActivity up"   "$d_res"
    write_check "$ok_foc" "window focused"    "$d_foc"; add_summary_check "$ok_foc" "window focused"    "$d_foc"
    write_check "$ok_awk" "screen awake"      "$d_awk"; add_summary_check "$ok_awk" "screen awake"      "$d_awk"

    # ------------------------------------------------------------ guest evidence

    write_step "Collecting guest evidence over ADB"

    # -d dumps the ring buffer rather than streaming: the app started before we
    # attached, so a live stream would show nothing that already happened.
    "$ADB" -s "$SERIAL" logcat -d -v threadtime -s IVI_V2X R4ListenerService R4Deserializer \
        > "$OUT_DIR/app-logcat.txt" 2>&1 || true
    write_ok "app-logcat.txt    $(count_lines "$OUT_DIR/app-logcat.txt") lines"

    # -b crash reads Android's separate crash buffer rather than the main one.
    "$ADB" -s "$SERIAL" logcat -d -b crash > "$OUT_DIR/app-crash.txt" 2>&1 || true
    write_ok "app-crash.txt     $(count_lines "$OUT_DIR/app-crash.txt") lines"

    # The socket is bound dual-stack, so it appears in udp6 and not in udp.
    "$ADB" -s "$SERIAL" shell cat /proc/net/udp6 > "$OUT_DIR/guest-udp6.txt" 2>&1 || true
    write_ok "guest-udp6.txt    listener on B8C4 = port 47300"

    "$ADB" -s "$SERIAL" shell ip -4 addr show > "$OUT_DIR/guest-ifaces.txt" 2>&1 || true
    write_ok "guest-ifaces.txt  eth0 must carry the node's pin address"
fi

# ----------------------------------------------------------------------- verdict

write_step "Reading the evidence"

has_in_file() { [ -f "$1" ] && grep -F -- "$2" "$1" >/dev/null 2>&1; }

TX_SEEN=0
TX_FILE=""
while IFS= read -r f; do
    [ -n "$f" ] || continue
    if has_in_file "$f" '[TX]'; then TX_SEEN=1; TX_FILE="$(basename "$f")"; break; fi
done <<EOF
$NODE_FILES
EOF

LOGCAT="$OUT_DIR/app-logcat.txt"
CRASH="$OUT_DIR/app-crash.txt"

GUEST_COLLECTED=0
if [ "$GUEST_READY" -eq 1 ] || [ -s "$LOGCAT" ]; then GUEST_COLLECTED=1; fi

NO_GUEST_REASON="not collected - no ADB tunnel"
if [ "$HAS_SKYCRAFT" -eq 0 ]; then NO_GUEST_REASON="no Skycraft node in this Room"; fi

RX_OK=0;   has_in_file "$LOGCAT" '[RX]'              && RX_OK=1
SRC_OK=0;  has_in_file "$LOGCAT" 'source=v2x_relayed' && SRC_OK=1
RISK_OK=0; has_in_file "$LOGCAT" 'riskState=high'     && RISK_OK=1

# Scoped to the package on purpose: a bare FATAL search matches the stock AAOS
# Bluetooth stack aborting at boot, which is guest noise and not our defect.
CRASH_OK=0
if [ "$GUEST_COLLECTED" -eq 1 ] && ! has_in_file "$CRASH" "$PACKAGE"; then CRASH_OK=1; fi

d_tx="no [TX] in any node log";  if [ "$TX_SEEN" -eq 1 ]; then d_tx="in $TX_FILE"; fi
if [ "$GUEST_COLLECTED" -eq 1 ]; then
    d_rx="no [RX] in app-logcat.txt"
    if [ "$RX_OK" -eq 1 ]; then d_rx="$(grep -c -F -- '[RX]' "$LOGCAT" 2>/dev/null || printf '0') lines"; fi
    d_src="absent from app-logcat.txt";  if [ "$SRC_OK" -eq 1 ];  then d_src="every warning carries it"; fi
    d_risk="scenario never reached high"; if [ "$RISK_OK" -eq 1 ]; then d_risk="reached"; fi
    d_crash="crash buffer names $PACKAGE"; if [ "$CRASH_OK" -eq 1 ]; then d_crash="nothing naming $PACKAGE"; fi
else
    d_rx="$NO_GUEST_REASON"
    d_src="$NO_GUEST_REASON"
    d_risk="$NO_GUEST_REASON"
    d_crash="$NO_GUEST_REASON"
    RX_OK=0; SRC_OK=0; RISK_OK=0; CRASH_OK=0
fi

printf '\n  %s------------------------------------------------------%s\n' "$C_WHITE" "$C_OFF"
printf '  %sEVIDENCE - %s%s\n' "$C_WHITE" "$TEST_NAME" "$C_OFF"
printf '  %s------------------------------------------------------%s\n' "$C_WHITE" "$C_OFF"
add_summary ""
add_summary "EVIDENCE"

PASSED=0
TOTAL=0
evidence_row() {
    TOTAL=$((TOTAL + 1))
    if [ "$1" -eq 1 ]; then
        PASSED=$((PASSED + 1))
        printf '  %s[x] %-24s %s%s\n' "$C_GREEN" "$2" "$3" "$C_OFF"
        add_summary "$(printf '[x] %-24s %s' "$2" "$3")"
    else
        printf '  %s[ ] %-24s %s%s\n' "$C_YELLOW" "$2" "$3" "$C_OFF"
        add_summary "$(printf '[ ] %-24s %s' "$2" "$3")"
    fi
}

# The [-] row: a question this Room cannot be asked. It is not scored, so a Room
# with no guest can still pass whole -- a phase 0 or V2X-only run is not a failure
# for lacking an app.
evidence_skip() {
    printf '  %s[-] %-24s %s%s\n' "$C_GRAY" "$1" "$2" "$C_OFF"
    add_summary "$(printf '[-] %-24s %s' "$1" "$2")"
}

# guest_row <ok> <name> <detail> -- scored when this Room has a guest, [-] when not.
guest_row() {
    if [ "$HAS_SKYCRAFT" -eq 1 ]; then evidence_row "$1" "$2" "$3"; else evidence_skip "$2" "$3"; fi
}

evidence_row "$DEP_RUNNING" "deployment RUNNING" "$DEP_STATUS"
evidence_row "$ALL_RUNNING" "all nodes Running"  "$RUNNING_COUNT/$NODE_COUNT"
evidence_row "$TX_SEEN"     "[TX] in a node log" "$d_tx"
guest_row    "$RX_OK"       "[RX] in app-logcat" "$d_rx"
guest_row    "$SRC_OK"      "source=v2x_relayed" "$d_src"
guest_row    "$RISK_OK"     "riskState=high"     "$d_risk"
guest_row    "$CRASH_OK"    "no crash naming app" "$d_crash"

printf '\n'
if [ "$PASSED" -eq "$TOTAL" ] && [ "$HAS_SKYCRAFT" -eq 1 ]; then
    printf '  %sComplete. The app is receiving R4 and warning from relayed data.%s\n' "$C_GREEN" "$C_OFF"
elif [ "$PASSED" -eq "$TOTAL" ]; then
    printf '  %sComplete for a Room with no guest. Every node-side check passed;%s\n' "$C_GREEN" "$C_OFF"
    printf '  %sthe app rows are absent because this blueprint has no Skycraft node.%s\n' "$C_GREEN" "$C_OFF"
elif [ "$HAS_SKYCRAFT" -eq 0 ]; then
    printf '  %sNode-side only, and that is all this Room has. Read the node logs above%s\n' "$C_YELLOW" "$C_OFF"
    printf '  %sfor what the [ ] rows mean.%s\n' "$C_YELLOW" "$C_OFF"
elif [ "$GUEST_COLLECTED" -ne 1 ]; then
    printf '  %sNode-side only. Without the ADB tunnel the app half cannot be evidenced -%s\n' "$C_YELLOW" "$C_OFF"
    printf '  %s[RX], provenance and risk are unknown rather than failed.%s\n' "$C_YELLOW" "$C_OFF"
elif [ "$RX_OK" -eq 1 ]; then
    printf '  %sPartly evidenced. The app is alive and parsing, but not every line appeared.%s\n' "$C_YELLOW" "$C_OFF"
    printf '  %sA missing line can just mean the scenario had not reached that step yet.%s\n' "$C_YELLOW" "$C_OFF"
else
    printf '  %sNo R4 reached the app. Check the producer'"'"'s [TX] target against the IVI pin%s\n' "$C_YELLOW" "$C_OFF"
    printf '  %saddress, and guest-ifaces.txt for eth0 - apk-deploy.md, Troubleshooting.%s\n' "$C_YELLOW" "$C_OFF"
fi

# ----------------------------------------------------------------------- summary

add_summary ""
add_summary "FILES"
for f in "$OUT_DIR"/*; do
    [ -f "$f" ] || continue
    b="$(basename "$f")"
    [ "$b" != "summary.txt" ] || continue
    kb="$(awk -v n="$(wc -c < "$f" | tr -d ' ')" 'BEGIN { printf "%.1f", n / 1024 }')"
    add_summary "$(printf '  %-32s %s KB' "$b" "$kb")"
done
add_summary ""
add_summary "No screen recording is collected here - test guide Step 3-1 is still yours."

printf '%s' "$SUMMARY" > "$OUT_DIR/summary.txt"

printf '\n  %sWritten to %s%s\n' "$C_CYAN" "$OUT_DIR" "$C_OFF"
printf '  %sChecklist:  %s/summary.txt%s\n' "$C_CYAN" "$OUT_DIR" "$C_OFF"
if [ -n "$NODE_FILES" ]; then
    printf '  %sCaptures:   ./tools/pcap-extract/Extract-Pcap.ps1 "%s"%s\n' "$C_GRAY" "$OUT_DIR" "$C_OFF"
fi
printf '\n  %sSTILL YOURS - no script can do this:%s\n' "$C_WHITE" "$C_OFF"
printf '  %sThe Screen widget - EGO and B solid, C dashed with a pulsing glow and the%s\n' "$C_GRAY" "$C_OFF"
printf '  %sbadge [V2X] C - <d> m - RISK: HIGH. This build logs nothing from its UI%s\n' "$C_GRAY" "$C_OFF"
printf '  %slayer, so the switch to the Warning View is confirmable only on screen.%s\n' "$C_GRAY" "$C_OFF"
printf '\n'

exit 0
