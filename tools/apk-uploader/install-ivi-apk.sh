#!/usr/bin/env bash
# ---------------------------------------------------------------------------
#  install-ivi-apk.sh
#
#  One-shot: open the reach-backend ADB tunnel, install the IVI APK into the
#  AAOS guest, and dump the evidence logcat.
#
#  Windows equivalent: install-ivi-apk.ps1, beside this file. The two scripts
#  are the same tool for two hosts and MUST stay behaviourally identical -- the
#  same steps in the same order, the same checks, the same on-screen structure
#  and the same exit conditions. A change to one is a change to both; anything
#  that lands in only one of them is a defect, not a platform difference.
#
#  Automates Step 3 of documents/Delivery/apk-deploy.md and Step 3-2 of its
#  companion documents/Delivery/testing-guide.md, which stay the authority.
#  Everything a human still has to do is printed at the end.
#
#  Before running: the blueprint must be deployed with the IVI Skycraft node
#  Running, and secrets/reach-adb-token-ivi.txt must hold the a8k_ token from
#  the Local ADB dialog (Devices -> KIS -> Connect -> IVI ADB widget -> Local
#  ADB). A redeploy mints a new token.
#
#  Requires bash (not sh): the port probe uses bash's /dev/tcp, because nc is
#  not guaranteed to be installed.
# ---------------------------------------------------------------------------

set -u

# ------------------------------------------------------------------ defaults

TOKEN=''
PORT=5555
APK=''
KEEP_TUNNEL=0
CLOSE_TUNNEL=0
SKIP_INSTALL=0
SKIP_NETWORK_FIX=0
IVI_ADDR='10.99.0.13'
ROOM_GATEWAY=''

usage() {
    cat <<'EOF'

  install-ivi-apk.sh - open the ADB tunnel, install the IVI APK, collect evidence

  Usage:
    install-ivi-apk.sh [options]

  Options:
    --token <a8k_...>    Use this a8k_ token instead of the one in
                         secrets/reach-adb-token-ivi.txt.
    --port <n>           Local port the tunnel listens on. Default 5555.
    --apk <path>         APK to install. Defaults to app-debug.apk beside this
                         script.
    --keep-tunnel        Leave the tunnel running after the script finishes, so
                         you can run adb yourself.
    --close-tunnel       Close the tunnel on --port when the run finishes, even one
                         this run did not start. The default only closes a tunnel
                         this run opened; a tunnel inherited from an earlier run is
                         left alone, which is how a dead one survives to strand the
                         next run. Collecting logs needs the tunnel.
    --skip-install       Only open the tunnel and collect evidence. Use when the
                         APK is already installed.
    --skip-network-fix   Do not put the guest on the Room subnet. Without the fix
                         no R4 datagram can arrive, so use this only when
                         deliberately testing the unpatched state.
    --ivi-addr <addr>    The Room address of the IVI node, from the blueprint
                         pin. Default 10.99.0.13.
    --room-gateway <ip>  Add a default route via this address inside the eth0
                         table. Only needed if the R4 producer is off-subnet;
                         leave empty for the standard single-subnet Room.
    -h, --help           Print this help and exit.

  Examples:
    ./install-ivi-apk.sh                  # install and collect evidence
    ./install-ivi-apk.sh --keep-tunnel    # ...and leave adb usable afterwards
    ./install-ivi-apk.sh --skip-install   # re-sample the logs, no reinstall

EOF
}

need_value() {
    # $1 = flag name, $2 = value as parsed ('' when the flag ended the argv)
    if [ -z "$2" ]; then
        printf '\n  FAILED: %s needs a value.\n\n' "$1" >&2
        exit 2
    fi
}

while [ $# -gt 0 ]; do
    case "$1" in
        --token)          shift; need_value '--token' "${1:-}";        TOKEN="$1" ;;
        --token=*)        TOKEN="${1#*=}" ;;
        --port)           shift; need_value '--port' "${1:-}";         PORT="$1" ;;
        --port=*)         PORT="${1#*=}" ;;
        --apk)            shift; need_value '--apk' "${1:-}";          APK="$1" ;;
        --apk=*)          APK="${1#*=}" ;;
        --ivi-addr)       shift; need_value '--ivi-addr' "${1:-}";     IVI_ADDR="$1" ;;
        --ivi-addr=*)     IVI_ADDR="${1#*=}" ;;
        --room-gateway)   shift; need_value '--room-gateway' "${1:-}"; ROOM_GATEWAY="$1" ;;
        --room-gateway=*) ROOM_GATEWAY="${1#*=}" ;;
        --keep-tunnel)     KEEP_TUNNEL=1 ;;
        --close-tunnel)    CLOSE_TUNNEL=1 ;;
        --skip-install)    SKIP_INSTALL=1 ;;
        --skip-network-fix) SKIP_NETWORK_FIX=1 ;;
        -h|--help)        usage; exit 0 ;;
        *)
            printf '\n  FAILED: unknown option %s\n  Fix:    run with --help for the option list.\n\n' "$1" >&2
            exit 2
            ;;
    esac
    shift
done

case "$PORT" in
    ''|*[!0-9]*)
        printf '\n  FAILED: --port must be a number, got %s\n\n' "$PORT" >&2
        exit 2
        ;;
esac

# ---------------------------------------------------------------- presentation

if [ -t 1 ]; then
    C_RESET=$'\033[0m'; C_WHITE=$'\033[97m';  C_CYAN=$'\033[96m'
    C_GREEN=$'\033[92m'; C_YELLOW=$'\033[93m'; C_RED=$'\033[91m'
    C_GRAY=$'\033[90m';  C_LGRAY=$'\033[37m'
else
    C_RESET=''; C_WHITE=''; C_CYAN=''; C_GREEN=''; C_YELLOW=''; C_RED=''; C_GRAY=''; C_LGRAY=''
fi

STEP_NO=0
say()   { printf '%s%s%s\n' "${2:-}" "$1" "$C_RESET"; }
step()  { STEP_NO=$((STEP_NO + 1)); printf '\n%s[%d] %s%s\n' "$C_CYAN" "$STEP_NO" "$1" "$C_RESET"; }
ok()    { printf '%s    OK    %s%s\n' "$C_GREEN"  "$1" "$C_RESET"; }
warn()  { printf '%s    WARN  %s%s\n' "$C_YELLOW" "$1" "$C_RESET"; }
info()  { printf '%s          %s%s\n' "$C_GRAY"   "$1" "$C_RESET"; }

# $1 = 0 (pass) / 1 (fail), $2 = name, $3 = detail
check_line() {
    local box col
    if [ "$1" -eq 0 ]; then box='[x]'; col="$C_GREEN"; else box='[ ]'; col="$C_YELLOW"; fi
    printf '%s    %s %-20s %s%s\n' "$col" "$box" "$2" "$3" "$C_RESET"
}

fail() {
    printf '\n'
    printf '%s  FAILED: %s%s\n' "$C_RED" "$1" "$C_RESET"
    if [ -n "${2:-}" ]; then printf '%s  Fix:    %s%s\n' "$C_YELLOW" "$2" "$C_RESET"; fi
    printf '\n'
    stop_tunnel
    exit 1
}

# ------------------------------------------------------------------ tunnel mgmt

TUNNEL_PID=''
TUNNEL_LOG=''
TUNNEL_REUSED=0
TUNNEL_STOPPED=0

stop_tunnel() {
    if [ -n "$TUNNEL_PID" ] && [ "$TUNNEL_STOPPED" -eq 0 ] && kill -0 "$TUNNEL_PID" 2>/dev/null; then
        kill "$TUNNEL_PID" 2>/dev/null
        # Give it a moment to go down cleanly, then insist.
        local i=0
        while [ $i -lt 20 ] && kill -0 "$TUNNEL_PID" 2>/dev/null; do
            sleep 0.1
            i=$((i + 1))
        done
        kill -9 "$TUNNEL_PID" 2>/dev/null
        wait "$TUNNEL_PID" 2>/dev/null
        TUNNEL_STOPPED=1
        info "Tunnel stopped (pid $TUNNEL_PID)."
    fi
}

# Closes whatever holds the port, including a tunnel this run did not start.
# stop_tunnel above can only reach a process we launched; a tunnel left behind by an
# earlier run is a foreign pid, and it is the one that strands the next run -- the port
# still listens, so this script reuses it, while the session behind it is long dead.
# Echoes the pids it stopped. lsof, then ss, then fuser: none is present everywhere.
port_pids() {
    if command -v lsof >/dev/null 2>&1; then
        lsof -nP -iTCP:"$1" -sTCP:LISTEN -t 2>/dev/null | sort -u
    elif command -v ss >/dev/null 2>&1; then
        ss -lptnH "sport = :$1" 2>/dev/null | grep -o 'pid=[0-9]*' | cut -d= -f2 | sort -u
    elif command -v fuser >/dev/null 2>&1; then
        fuser -n tcp "$1" 2>/dev/null | tr -s ' ' '\n' | grep -E '^[0-9]+$' | sort -u
    fi
}

stop_tunnel_on_port() {
    local port="$1" pids pid found=0
    pids="$(port_pids "$port")"
    if [ -z "$pids" ]; then
        info "Nothing was listening on port $port."
        return 0
    fi
    for pid in $pids; do
        if kill "$pid" 2>/dev/null; then
            local i=0
            while [ $i -lt 20 ] && kill -0 "$pid" 2>/dev/null; do sleep 0.1; i=$((i + 1)); done
            kill -9 "$pid" 2>/dev/null
            found=1
            info "Closed pid $pid on port $port."
        else
            warn "Could not stop pid $pid - stop it by hand."
        fi
    done
    [ "$found" -eq 1 ] && TUNNEL_STOPPED=1
    return 0
}

# ------------------------------------------------------------------- token mgmt

# a8k_ + base64(nodeKey), ~36 chars. The CarSky REST API key is a different, much
# longer credential -- pasting that one here is the common mistake. Echoes the
# complaint and returns 1 when the shape is wrong.
check_token_shape() {
    local tok="$1"
    if [ -z "$tok" ]; then echo "The token is empty."; return 1; fi
    case "$tok" in a8k_*) ;; *) echo "The token does not start with 'a8k_'."; return 1 ;; esac
    if [ "${#tok}" -gt 60 ]; then
        echo "This looks like the CarSky REST API key (${#tok} chars), not the ADB tunnel token (~36)."
        return 1
    fi
    return 0
}

token_node() {
    local b64="${1#a8k_}" out=''
    while [ $(( ${#b64} % 4 )) -ne 0 ]; do b64="${b64}="; done
    if command -v base64 >/dev/null 2>&1; then
        out="$(printf '%s' "$b64" | base64 -d 2>/dev/null || printf '%s' "$b64" | base64 -D 2>/dev/null || true)"
    fi
    if [ -n "$out" ]; then printf '%s' "$out"; else printf '%s' '(could not decode)'; fi
}

# A stale token does not fail the upgrade loudly: the gateway answers the WebSocket
# handshake 404 and reach-backend keeps retrying, so the port listens while no ADB
# transport ever forms. The log is the only place that says so.
tunnel_rejected() {
    local f
    for f in "$TUNNEL_LOG" "${TUNNEL_LOG}.err"; do
        [ -n "$f" ] && [ -f "$f" ] || continue
        grep -q 'Unexpected server response: 404' "$f" 2>/dev/null && return 0
    done
    return 1
}

# The platform mints a new token on every deploy, and the old one then 404s rather than
# failing visibly. Prompt for the new one and persist it, rather than making the operator
# find the file. Echoes the token; empty when the run cannot prompt or the operator declines.
request_new_token() {
    if [ ! -t 0 ]; then
        info "Input is not a terminal, so this run cannot prompt. Update $TOKEN_FILE and re-run." >&2
        return 0
    fi
    printf '\n' >&2
    say '  A deploy mints a new ADB token; the one on disk no longer resolves.' "$C_YELLOW" >&2
    say '  Copy it: Devices -> KIS -> Connect -> IVI ADB -> Local ADB' "$C_YELLOW" >&2
    say '  Paste the a8k_ value, or the whole reach-backend command line.' "$C_YELLOW" >&2
    say '  Press Enter on an empty line to give up.' "$C_GRAY" >&2
    local i=0 entered complaint
    while [ $i -lt 3 ]; do
        i=$((i + 1))
        printf '  token: ' >&2
        IFS= read -r entered || return 0
        entered="$(trim "$entered")"
        [ -z "$entered" ] && return 0
        # Tolerate a pasted command line -- lifting --key out of it is what an operator
        # copying from the dialog actually has on the clipboard.
        case "$entered" in *--key*) entered="$(printf '%s' "$entered" | sed -n 's/.*--key[[:space:]]\{1,\}\([^[:space:]]\{1,\}\).*/\1/p')" ;; esac
        entered="$(printf '%s' "$entered" | tr -d '"'"'"'')"
        if ! complaint="$(check_token_shape "$entered")"; then
            warn "$complaint" >&2
            continue
        fi
        if printf '%s' "$entered" > "$TOKEN_FILE" 2>/dev/null; then
            ok "Saved to $TOKEN_FILE" >&2
        else
            warn "Could not write $TOKEN_FILE - using the token for this run only." >&2
        fi
        ok "Now targets node '$(token_node "$entered")'" >&2
        printf '%s' "$entered"
        return 0
    done
    return 0
}

on_exit() {
    # --keep-tunnel is honoured on a normal finish only; see on_signal.
    if [ "$KEEP_TUNNEL" -eq 0 ]; then stop_tunnel; fi
}

on_signal() {
    # An interrupted run never printed the pid, so leaving the tunnel behind
    # would strand a process nobody can address. Kill it even under
    # --keep-tunnel: that flag means "keep it after a completed run".
    printf '\n'
    say '  Interrupted - shutting the tunnel down.' "$C_YELLOW"
    stop_tunnel
    exit 130
}

trap on_exit EXIT
trap on_signal INT
trap on_signal TERM

# TCP probe with no external dependency: nc is not installed everywhere, and
# bash's /dev/tcp is always present in a normal bash build.
test_port() {
    ( exec 3<>"/dev/tcp/127.0.0.1/$1" ) >/dev/null 2>&1
}

# adb output carries CRLF on every host; strip the CR so matching and trimming
# behave the same way PowerShell's .Trim() does.
adb_out() {
    "$ADB" -s "$SERIAL" "$@" 2>&1 | tr -d '\r'
}

trim() {
    local s="$1"
    s="${s#"${s%%[![:space:]]*}"}"
    s="${s%"${s##*[![:space:]]}"}"
    printf '%s' "$s"
}

file_size() {
    stat -c '%s' "$1" 2>/dev/null || stat -f '%z' "$1" 2>/dev/null || wc -c <"$1"
}

file_mtime() {
    local m
    m="$(stat -c '%y' "$1" 2>/dev/null)" && [ -n "$m" ] && { printf '%s' "$m" | cut -c1-16; return 0; }
    m="$(stat -f '%Sm' -t '%Y-%m-%d %H:%M' "$1" 2>/dev/null)" && [ -n "$m" ] && { printf '%s' "$m"; return 0; }
    printf 'unknown'
}

# ------------------------------------------------------------------- locate all

printf '\n'
say '  IVI APK installer - tunnel, install, evidence' "$C_WHITE"
say '  Authority: documents/Delivery/apk-deploy.md Step 3' "$C_GRAY"

step 'Locating tools and files'

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "$SCRIPT_DIR/../.." && pwd)"
REACH_BIN="$SCRIPT_DIR/reach_be/reach/reach-backend"
TOKEN_FILE="$REPO_ROOT/secrets/reach-adb-token-ivi.txt"
LOG_DIR="$SCRIPT_DIR/logs"
if [ -z "$APK" ]; then APK="$SCRIPT_DIR/app-debug.apk"; fi

GATEWAY='https://hackathon-2.carsky.io'

if [ -z "${BASH_VERSION:-}" ]; then
    printf '\n  FAILED: this script needs bash, not sh.\n  Fix:    run it as "bash install-ivi-apk.sh".\n\n' >&2
    exit 1
fi
if [ "${BASH_VERSINFO[0]}" -lt 4 ]; then
    fail "Bash $BASH_VERSION is too old." "Bash 4 or newer; the port probe needs a build with /dev/tcp compiled in."
fi

if [ ! -f "$REACH_BIN" ]; then
    fail "Tunnel CLI not found at $REACH_BIN" "Unpack the organizers' reach_be zip into tools/apk-uploader/."
fi
if [ ! -x "$REACH_BIN" ]; then
    # A zip extracted on Windows, or copied off a FAT/NTFS share, loses the
    # execute bit. Restore it rather than failing on something this cheap.
    chmod +x "$REACH_BIN" 2>/dev/null
    if [ ! -x "$REACH_BIN" ]; then
        fail "Tunnel CLI at $REACH_BIN is not executable." "chmod +x '$REACH_BIN' (the zip loses the execute bit when unpacked on Windows)."
    fi
    info 'Restored the execute bit on the tunnel CLI.'
fi
# The organizers ship an x86_64 build only. An arm64 host runs it under
# emulation, so no separate binary is needed -- but say which path is in play
# if the tunnel later dies.
HOST_ARCH="$(uname -m 2>/dev/null || printf 'unknown')"
EMULATED=''
case "$HOST_ARCH" in
    aarch64|arm64) EMULATED=', x86_64 emulated' ;;
esac
ok "Tunnel CLI  ($(uname -s 2>/dev/null || printf 'unknown') $HOST_ARCH host$EMULATED)"

# adb: PATH first, then every location an Android SDK is commonly installed to.
# Hardcoding one path breaks on any machine that put the SDK elsewhere.
ADB=''
if command -v adb >/dev/null 2>&1; then
    ADB="$(command -v adb)"
else
    CANDIDATES=''
    [ -n "${ANDROID_HOME:-}" ]     && CANDIDATES="$CANDIDATES $ANDROID_HOME/platform-tools"
    [ -n "${ANDROID_SDK_ROOT:-}" ] && CANDIDATES="$CANDIDATES $ANDROID_SDK_ROOT/platform-tools"
    CANDIDATES="$CANDIDATES ${HOME:-/root}/Android/Sdk/platform-tools"
    CANDIDATES="$CANDIDATES ${HOME:-/root}/android-sdk/platform-tools"
    CANDIDATES="$CANDIDATES ${HOME:-/root}/Library/Android/sdk/platform-tools"
    CANDIDATES="$CANDIDATES /usr/lib/android-sdk/platform-tools"
    CANDIDATES="$CANDIDATES /opt/android-sdk/platform-tools"
    CANDIDATES="$CANDIDATES /opt/android-sdk-linux/platform-tools"
    CANDIDATES="$CANDIDATES /usr/local/android-sdk/platform-tools"
    for c in $CANDIDATES; do
        if [ -x "$c/adb" ]; then ADB="$c/adb"; break; fi
    done
fi
if [ -z "$ADB" ]; then
    fail "adb not found on PATH or in any standard Android SDK location." \
         "Install Android SDK platform-tools, then either add its folder to PATH or set ANDROID_HOME (or ANDROID_SDK_ROOT)."
fi
ok "adb  ->  $ADB"

if [ "$SKIP_INSTALL" -eq 0 ]; then
    if [ ! -f "$APK" ]; then
        fail "APK not found at $APK" "Build it, or download the CI app-debug-apk artifact (deploy note Step 1)."
    fi
    APK_MB="$(awk -v b="$(file_size "$APK")" 'BEGIN { printf "%.1f", b / 1048576 }')"
    ok "APK  ->  $(basename "$APK")  ($APK_MB MB, built $(file_mtime "$APK"))"
fi

# ---------------------------------------------------------------- token & shape

step 'Reading the ADB tunnel token'

if [ -z "$TOKEN" ]; then
    if [ ! -f "$TOKEN_FILE" ]; then
        fail "No token. $TOKEN_FILE does not exist." \
             "Copy the a8k_ value from the Local ADB dialog into that file, or pass --token a8k_..."
    fi
    TOKEN="$(trim "$(cat "$TOKEN_FILE")")"
fi
TOKEN="$(trim "$TOKEN")"

if [ -z "$TOKEN" ]; then fail "The token is empty." "Paste the a8k_ value from the Local ADB dialog."; fi
case "$TOKEN" in
    a8k_*) ;;
    *) fail "The token does not start with 'a8k_'." "Copy only the --key value from the dialog, not the whole command." ;;
esac

# The ADB tunnel token is a8k_ + base64(nodeKey), ~36 chars. The CarSky REST API
# key is a different, much longer credential -- pasting that one here is the
# common mistake.
if [ "${#TOKEN}" -gt 60 ]; then
    fail "This looks like the CarSky REST API key (${#TOKEN} chars), not the ADB tunnel token (~36)." \
         "The tunnel token is the short a8k_ value in the Local ADB dialog. They are different credentials."
fi

TARGET_NODE='(could not decode)'
B64="${TOKEN#a8k_}"
while [ $(( ${#B64} % 4 )) -ne 0 ]; do B64="${B64}="; done
DECODED=''
if command -v base64 >/dev/null 2>&1; then
    # GNU base64 takes -d, BSD/macOS base64 takes -D. Try both, keep neither on failure.
    DECODED="$(printf '%s' "$B64" | base64 -d 2>/dev/null)" || DECODED=''
    if [ -z "$DECODED" ]; then
        DECODED="$(printf '%s' "$B64" | base64 -D 2>/dev/null)" || DECODED=''
    fi
fi
DECODED="$(printf '%s' "$DECODED" | tr -d '\000\r')"
if [ -n "$DECODED" ]; then TARGET_NODE="$DECODED"; fi
ok "Token accepted - targets node '$TARGET_NODE'"
info 'If that is not the IVI node of the Room you just deployed, the token is stale:'
info 're-copy it from Devices -> KIS -> Connect -> IVI ADB -> Local ADB.'

# ----------------------------------------------------------------- open tunnel

open_tunnel() {
    local tok="$1"
    if test_port "$PORT"; then
        TUNNEL_REUSED=1
        warn "Port $PORT is already serving - reusing it (another tunnel is probably open)."
        return 0
    fi
    TUNNEL_REUSED=0
    TUNNEL_STOPPED=0
    mkdir -p "$LOG_DIR"
    TUNNEL_LOG="$LOG_DIR/tunnel-last.log"

    # The token is passed as an argument and never echoed.
    "$REACH_BIN" adb --gateway "$GATEWAY" --key "$tok" --port "$PORT" \
        >"$TUNNEL_LOG" 2>"$TUNNEL_LOG.err" </dev/null &
    TUNNEL_PID=$!

    TUNNEL_UP=1
    i=0
    while [ $i -lt 50 ]; do            # 50 x 0.5s = 25s
        if ! kill -0 "$TUNNEL_PID" 2>/dev/null; then
            wait "$TUNNEL_PID" 2>/dev/null
            TUNNEL_RC=$?
            TUNNEL_STOPPED=1
            WHY=''
            for f in "$TUNNEL_LOG" "$TUNNEL_LOG.err"; do
                if [ -f "$f" ]; then WHY="$WHY$(trim "$(cat "$f")")"$'\n'; fi
            done
            printf '\n'
            say '  Tunnel output:' "$C_GRAY"
            say "  $(trim "$WHY")" "$C_GRAY"
            fail "The tunnel exited immediately (exit $TUNNEL_RC)." \
                 "Three causes, in order of likelihood: the token is stale (redeploy mints a new one - re-copy it); the IVI node is not Running yet; port $PORT is taken (pass --port 5556)."
        fi
        if test_port "$PORT"; then TUNNEL_UP=0; break; fi
        sleep 0.5
        i=$((i + 1))
    done
    if [ "$TUNNEL_UP" -ne 0 ] && ! test_port "$PORT"; then
        fail "The tunnel did not start listening within 25s." "Check $TUNNEL_LOG."
    fi
    ok "Tunnel serving (pid $TUNNEL_PID), log: $TUNNEL_LOG"
}

step "Opening the tunnel on 127.0.0.1:$PORT"

open_tunnel "$TOKEN"

# ------------------------------------------------------------------ adb connect

step 'Connecting adb to the guest'

SERIAL="localhost:$PORT"
"$ADB" connect "$SERIAL" >/dev/null 2>&1

connect_guest() {
    "$ADB" connect "$SERIAL" >/dev/null 2>&1
    local i=0
    while [ $i -lt 30 ]; do               # 30 x 1s = 30s
        if "$ADB" devices 2>&1 | tr -d '\r' \
            | grep -E "^localhost:$PORT[[:space:]]+device([[:space:]]|$)" >/dev/null 2>&1; then
            return 0
        fi
        sleep 1
        "$ADB" connect "$SERIAL" >/dev/null 2>&1
        i=$((i + 1))
    done
    return 1
}

CONNECTED=1
if connect_guest; then CONNECTED=0; fi

# A 404 means the gateway has no such ADB session -- the node key in the token belongs to
# a deployment that no longer exists. Waiting for the node to boot cannot fix that, so
# offer the one thing that can: a fresh token.
if [ "$CONNECTED" -ne 0 ] && tunnel_rejected; then
    printf '\n'
    warn 'The gateway answered 404 on every upgrade - this token no longer resolves.'
    info "It decodes to node '$(token_node "$TOKEN")', which is not a node of any live deployment."
    NEW_TOKEN="$(request_new_token)"
    if [ -n "$NEW_TOKEN" ]; then
        TOKEN="$NEW_TOKEN"
        stop_tunnel
        stop_tunnel_on_port "$PORT"
        sleep 1
        info 'Reopening the tunnel with the new token.'
        open_tunnel "$TOKEN"
        if connect_guest; then CONNECTED=0; fi
    fi
fi

if [ "$CONNECTED" -ne 0 ]; then
    printf '\n'
    say "$("$ADB" devices 2>&1 | tr -d '\r')" "$C_GRAY"
    if tunnel_rejected; then
        fail "The gateway rejected the tunnel token (404 on every upgrade)." \
             "Re-copy the a8k_ value from Devices -> KIS -> Connect -> IVI ADB -> Local ADB into $TOKEN_FILE. A deploy mints a new one, so the node key in the old token no longer exists."
    fi
    fail "The guest never reached state 'device' (offline, or not listed)." \
         "The tunnel is serving and the token resolves, so the Skycraft node may still be booting. Wait for it to go green in the Deployment Viewer, then re-run. If it is already green, its adbd may be wedged - Restart Node in the Inspector."
fi
ok "$SERIAL  device"
info "All adb calls below are pinned with -s $SERIAL, so a local emulator cannot be hit by mistake."

SDK="$(trim "$(adb_out shell getprop ro.build.version.sdk)")"
AUTO="$(adb_out shell pm list features)"
case "$SDK" in
    ''|*[!0-9]*) ;;
    *) if [ "$SDK" -lt 29 ]; then fail "Guest is API $SDK, below the APK's minSdk 29." "Wrong node or wrong AAOS artifact."; fi ;;
esac
if printf '%s' "$AUTO" | grep -F 'android.hardware.type.automotive' >/dev/null 2>&1; then
    AUTO_STATE='present'
else
    AUTO_STATE='ABSENT'
fi
ok "Guest API $SDK; automotive feature: $AUTO_STATE"

# ------------------------------------------------------------- room networking

# The AAOS guest boots with its bridge NIC named 'buried_eth0'. AAOS
# EthernetTracker does not recognise that name, so netd never adopts the
# interface, it never takes the Room address, and every R4 datagram sent to the
# node's pin address is dropped before the guest sees it -- the app binds its
# socket and waits forever. Renaming the NIC to 'eth0' makes EthernetTracker
# adopt it; netd then creates the 'eth0' network, its routing table and its
# policy rules by itself, so only the address still has to be set by hand.
#
# Organiser-supplied procedure. It is a live mutation of the running guest and
# does NOT survive a guest reboot or a redeploy -- re-run this script after
# either.

if [ "$SKIP_NETWORK_FIX" -eq 1 ]; then
    step 'Skipping the Room-network fix (--skip-network-fix)'
    warn 'No R4 can reach the app in this state. Evidence below will show zero [RX].'
else
    step "Putting the guest on the Room network ($IVI_ADDR)"

    ADDRS="$(adb_out shell ip -4 addr show)"

    if printf '%s' "$ADDRS" | grep -F "$IVI_ADDR/" >/dev/null 2>&1; then
        ok 'Already on the Room subnet - nothing to change'
    elif ! adb_out shell su 0 id | grep -F 'uid=0' >/dev/null 2>&1; then
        warn 'No root on this guest, so the NIC cannot be renamed. R4 will not arrive.'
        warn 'This is a finding to report, not something the script can work around.'
    else
        if printf '%s' "$ADDRS" | grep -F 'buried_eth0' >/dev/null 2>&1; then
            # Chained in one shell so the NIC can never be left down by an
            # interrupted call.
            adb_out shell "su 0 sh -c 'ip link set buried_eth0 down; ip link set buried_eth0 name eth0; ip link set eth0 up'" >/dev/null 2>&1
            ok 'Renamed buried_eth0 -> eth0 (EthernetTracker now adopts it)'
            sleep 4
        else
            info 'No buried_eth0 present - assuming eth0 already exists.'
        fi

        adb_out shell "su 0 ifconfig eth0 $IVI_ADDR/24 up" >/dev/null 2>&1

        # netd normally creates these already; 'File exists' is the expected,
        # healthy answer.
        adb_out shell "su 0 ip route add 10.99.0.0/24 dev eth0 table eth0 proto static scope link" >/dev/null 2>&1
        if [ -n "$ROOM_GATEWAY" ]; then
            adb_out shell "su 0 ip route add default via $ROOM_GATEWAY dev eth0 table eth0 proto static" >/dev/null 2>&1
            info "Default route via $ROOM_GATEWAY added to the eth0 table."
        fi

        sleep 3
        ADDRS="$(adb_out shell ip -4 addr show eth0)"
        if printf '%s' "$ADDRS" | grep -F "$IVI_ADDR/" >/dev/null 2>&1; then
            ok "eth0 is $IVI_ADDR/24 - the Room can now reach the app"
        else
            warn "eth0 did not take $IVI_ADDR. R4 will not arrive; check the pin address."
        fi
    fi
fi

# ---------------------------------------------------------------------- install

PACKAGE='com.hackathon.v2x.ivi'

# A package that was never on the guest cannot have been running, so the
# self-start check below only means something when the app was already installed
# before this run.
WAS_INSTALLED=1
if adb_out shell pm list packages | grep -F "$PACKAGE" >/dev/null 2>&1; then WAS_INSTALLED=0; fi

if [ "$SKIP_INSTALL" -eq 1 ]; then
    step 'Skipping install (--skip-install)'
else
    step "Installing $PACKAGE (this takes ~30-60s for a 28 MB APK)"

    OUT="$("$ADB" -s "$SERIAL" install -r "$APK" 2>&1 | tr -d '\r')"
    if ! printf '%s' "$OUT" | grep -F 'Success' >/dev/null 2>&1; then
        say "$OUT" "$C_GRAY"
        fail "adb install did not report Success." "Read the INSTALL_FAILED_ reason above; deploy note Step 3 covers the common ones."
    fi
    ok 'Success'

    if adb_out shell pm list packages | grep -F "$PACKAGE" >/dev/null 2>&1; then
        ok "package:$PACKAGE present on the guest"
    else
        fail "Install reported Success but the package is not listed." "Re-run; if it persists this is a finding worth recording."
    fi
fi

# ----------------------------------------------------------------- launch check

step 'Checking the app on the guest'

# A node showing Running in the blueprint says the VM is up, never that the app
# is: this Room ran green for hours with no APK installed at all. Only the guest
# can answer these.
ST_PID=''; ST_VERSION=''; ST_RESUMED=1; ST_FOCUSED=1; ST_AWAKE=1

get_app_state() {
    local d a w p
    ST_PID="$(trim "$(adb_out shell pidof "$PACKAGE")")"

    ST_VERSION=''
    d="$(adb_out shell "dumpsys package $PACKAGE | grep versionName")"
    ST_VERSION="$(printf '%s' "$d" | grep -o 'versionName=[^[:space:]]*' | head -n 1 | cut -d= -f2-)"

    a="$(adb_out shell "dumpsys activity activities | grep -i ResumedActivity")"
    if printf '%s' "$a" | grep -F "$PACKAGE/.MainActivity" >/dev/null 2>&1; then ST_RESUMED=0; else ST_RESUMED=1; fi

    w="$(adb_out shell "dumpsys window | grep -i mCurrentFocus")"
    if printf '%s' "$w" | grep -F "$PACKAGE" >/dev/null 2>&1; then ST_FOCUSED=0; else ST_FOCUSED=1; fi

    p="$(adb_out shell "dumpsys power | grep -i mWakefulness=")"
    if printf '%s' "$p" | grep -F 'mWakefulness=Awake' >/dev/null 2>&1; then ST_AWAKE=0; else ST_AWAKE=1; fi
}

sleep 3
get_app_state

# The deploy note Step 4 fixes MainActivity as the node's only MAIN/LAUNCHER
# activity, so on a correct deploy it is already resumed. Starting it by hand is
# a fallback, not a step.
NEEDED_START=1
if [ -z "$ST_PID" ] || [ "$ST_RESUMED" -ne 0 ]; then
    NEEDED_START=0
    adb_out shell am start -n "$PACKAGE/.MainActivity" >/dev/null 2>&1
    sleep 5
    get_app_state
fi

if [ -n "$ST_VERSION" ]; then
    check_line 0 'package installed' "$PACKAGE $ST_VERSION"
else
    check_line 1 'package installed' "$PACKAGE NOT FOUND"
fi
if [ -n "$ST_PID" ]; then
    check_line 0 'process running' "pid $ST_PID"
else
    check_line 1 'process running' 'no process'
fi
if [ "$ST_RESUMED" -eq 0 ]; then
    check_line 0 'MainActivity up' 'topResumedActivity'
else
    check_line 1 'MainActivity up' 'not the resumed activity'
fi
if [ "$ST_FOCUSED" -eq 0 ]; then
    check_line 0 'window focused' 'holds input focus'
else
    check_line 1 'window focused' 'another window has focus'
fi
if [ "$ST_AWAKE" -eq 0 ]; then
    check_line 0 'screen awake' 'mWakefulness=Awake'
else
    check_line 1 'screen awake' 'asleep - the HMI will not render'
fi

if [ "$NEEDED_START" -eq 0 ] && [ "$WAS_INSTALLED" -eq 0 ] && [ "$SKIP_INSTALL" -eq 0 ]; then
    info 'It needed a manual start though it was already installed - the deploy note'
    info 'calls that a finding, because a correct deploy starts it by itself.'
elif [ "$NEEDED_START" -eq 0 ]; then
    info 'Started by hand, which is expected on a first install.'
fi

# -------------------------------------------------------------------- evidence

step 'Collecting evidence logcat'

mkdir -p "$LOG_DIR"
STAMP="$(date '+%Y%m%d-%H%M%S')"
LOG_FILE="$LOG_DIR/ivi-logcat-$STAMP.txt"

info "Waiting 15s for the simulator's 1 Hz stream to produce warnings..."
sleep 15

# -d dumps the ring buffer rather than streaming: the app was already running
# before we attached, so the startup lines are only reachable this way (test
# guide Step 3-2).
"$ADB" -s "$SERIAL" logcat -d -v threadtime \
    -s IVI_V2X R4ListenerService R4Deserializer MainViewModel WarningViewModel \
    2>&1 | tr -d '\r' >"$LOG_FILE"

FATAL="$("$ADB" -s "$SERIAL" logcat -d -b crash 2>&1 | tr -d '\r')"

ok "Saved: $LOG_FILE  ($(wc -l <"$LOG_FILE" | tr -d ' ') lines)"

# ---------------------------------------------------------------------- verdict

log_has() { grep -F -- "$1" "$LOG_FILE" >/dev/null 2>&1; }

# No [UI] check: this build has no logging in its UI layer at all, so the mode
# switch is only observable on the Screen widget. A check that can never pass is
# a false failure.
CHECK_NAMES=(
    'UDP socket bound on 47300'
    'R4 messages received  ([RX])'
    'Provenance source=v2x_relayed'
    'Risk reached high'
    'No fatal exception'
)
CHECK_PASS=(1 1 1 1 1)

if log_has 'UDP socket open on port 47300' || log_has '[LINK] state=bound'; then CHECK_PASS[0]=0; fi
if log_has '[RX]';                                                            then CHECK_PASS[1]=0; fi
if log_has 'source=v2x_relayed';                                              then CHECK_PASS[2]=0; fi
if log_has 'riskState=high';                                                  then CHECK_PASS[3]=0; fi
if ! { printf '%s' "$FATAL" | grep -F 'FATAL EXCEPTION' >/dev/null 2>&1 \
    && printf '%s' "$FATAL" | grep -F "$PACKAGE" >/dev/null 2>&1; }; then CHECK_PASS[4]=0; fi

printf '\n'
say '  ------------------------------------------------------' "$C_WHITE"
say '  EVIDENCE' "$C_WHITE"
say '  ------------------------------------------------------' "$C_WHITE"
PASSED=0
for idx in "${!CHECK_NAMES[@]}"; do
    if [ "${CHECK_PASS[$idx]}" -eq 0 ]; then
        PASSED=$((PASSED + 1))
        printf '%s  [x] %s%s\n' "$C_GREEN" "${CHECK_NAMES[$idx]}" "$C_RESET"
    else
        printf '%s  [ ] %s%s\n' "$C_YELLOW" "${CHECK_NAMES[$idx]}" "$C_RESET"
    fi
done

printf '\n'
if [ "$PASSED" -eq "${#CHECK_NAMES[@]}" ]; then
    say '  The app is receiving R4 and rendering warnings from relayed data.' "$C_GREEN"
elif log_has '[RX]'; then
    say '  Partly evidenced. The app is alive and parsing, but not every line appeared.' "$C_YELLOW"
    say "  Read $LOG_FILE before concluding - a missing line can just mean the" "$C_YELLOW"
    say '  scenario had not reached its high-risk step yet. Re-run with --skip-install to sample again.' "$C_YELLOW"
else
    say '  No R4 reached the app. The install is fine; the stream is not arriving.' "$C_YELLOW"
    say '  Check the producer node'"'"'s log for [TX] lines, and that it targets 10.99.0.13:47300.' "$C_YELLOW"
fi

printf '\n'
say '  STILL YOURS - no script can do these:' "$C_WHITE"
say '  1. Look at the Screen widget: EGO and B solid, C dashed with a pulsing glow' "$C_LGRAY"
say '     and the badge [V2X] C - <d> m - RISK: HIGH. A yellow [? UNKNOWN SOURCE]' "$C_LGRAY"
say '     where ghost C belongs is a blocking defect on the approach scenario.' "$C_LGRAY"
say '     This build logs nothing from its UI layer, so the mode switch to the' "$C_LGRAY"
say '     Warning View is confirmable ONLY on screen - the logs cannot show it.' "$C_LGRAY"
say '  2. Record or screenshot it - set Recorder Part BEFORE the run starts.' "$C_LGRAY"
say '  (test guide Step 3-1)' "$C_GRAY"

# ---------------------------------------------------------------------- cleanup

printf '\n'
if [ "$CLOSE_TUNNEL" -eq 1 ]; then
    # Explicit: close the port whoever opened it. This is the only branch that can
    # reach a tunnel inherited from an earlier run.
    stop_tunnel
    stop_tunnel_on_port "$PORT"
    say "  Tunnel on port $PORT closed." "$C_CYAN"
    say "  Collecting logs needs it again - the collector's guest half is skipped without it." "$C_GRAY"
elif [ "$TUNNEL_REUSED" -eq 1 ]; then
    # Started by an earlier run, so the default leaves it alone - but print the pid,
    # because a tunnel nobody can address is what strands the next run.
    say "  Tunnel on port $PORT was already open and has been left alone." "$C_CYAN"
    say "  Live logs:  adb -s $SERIAL logcat -s IVI_V2X" "$C_CYAN"
    REUSED_PIDS="$(port_pids "$PORT" | tr '\n' ' ')"
    if [ -n "${REUSED_PIDS// /}" ]; then
        say "  Stop it:    kill $REUSED_PIDS" "$C_CYAN"
        say "              or re-run with --close-tunnel" "$C_CYAN"
    fi
elif [ "$KEEP_TUNNEL" -eq 1 ] && [ -n "$TUNNEL_PID" ]; then
    say "  Tunnel left running on port $PORT (pid $TUNNEL_PID)." "$C_CYAN"
    say "  Live logs:  adb -s $SERIAL logcat -s IVI_V2X" "$C_CYAN"
    say "  Stop it:    kill $TUNNEL_PID" "$C_CYAN"
else
    stop_tunnel
    say '  Done. Re-run with --keep-tunnel to keep adb usable afterwards.' "$C_GRAY"
fi
printf '\n'
