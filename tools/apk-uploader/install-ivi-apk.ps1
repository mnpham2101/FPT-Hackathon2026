<#
.SYNOPSIS
  One-shot: open the reach-backend ADB tunnel, install the IVI APK into the AAOS guest,
  and dump the evidence logcat.

.DESCRIPTION
  Automates Step 3 of documents/Delivery/apk-deploy.md and Step 3-2 of its
  companion documents/Delivery/testing-guide.md, which stay the authority.
  Everything a human still has to do is printed at the end.

  Before running: the blueprint must be deployed with the IVI Skycraft node Running, and
  secrets\reach-adb-token-ivi.txt must hold the a8k_ token from the Local ADB dialog
  (Devices -> KIS -> Connect -> IVI ADB widget -> Local ADB). A redeploy mints a new token.

.PARAMETER Token
  Use this a8k_ token instead of the one in secrets\reach-adb-token-ivi.txt.

.PARAMETER Port
  Local port the tunnel listens on. Default 5555.

.PARAMETER Apk
  APK to install. Defaults to app-debug.apk beside this script.

.PARAMETER KeepTunnel
  Leave the tunnel running after the script finishes, so you can run adb yourself.

.PARAMETER CloseTunnel
  Close the tunnel on -Port when the run finishes, even one this run did not start.
  The default only closes a tunnel this run opened; a tunnel inherited from an earlier
  run is left alone, which is how a dead one survives to strand the next run.
  Collecting logs needs the tunnel -- see the note under .DESCRIPTION.

.PARAMETER SkipInstall
  Only open the tunnel and collect evidence. Use when the APK is already installed.

.PARAMETER SkipNetworkFix
  Do not put the guest on the Room subnet. Without the fix no R4 datagram can arrive,
  so use this only when deliberately testing the unpatched state.

.PARAMETER IviAddr
  The Room address of the IVI node, from the blueprint pin. Default 10.99.0.13.

.PARAMETER RoomGateway
  Add a default route via this address inside the eth0 table. Only needed if the R4
  producer is off-subnet; leave empty for the standard single-subnet Room.
#>
[CmdletBinding()]
param(
    [string] $Token,
    [int]    $Port = 5555,
    [string] $Apk,
    [switch] $KeepTunnel,
    [switch] $CloseTunnel,
    [switch] $SkipInstall,
    [switch] $SkipNetworkFix,
    [string] $IviAddr = '10.99.0.13',
    [string] $RoomGateway = ''
)

$ErrorActionPreference = 'Stop'

# ---------------------------------------------------------------- presentation

$script:StepNo = 0
function Write-Step   ($m) { $script:StepNo++; Write-Host ""; Write-Host "[$script:StepNo] $m" -ForegroundColor Cyan }
function Write-Ok     ($m) { Write-Host "    OK    $m" -ForegroundColor Green }
function Write-Warn   ($m) { Write-Host "    WARN  $m" -ForegroundColor Yellow }
function Write-Info   ($m) { Write-Host "          $m" -ForegroundColor DarkGray }
function Write-Check  ($ok, $name, $detail) {
    $box = if ($ok) { '[x]' } else { '[ ]' }
    $col = if ($ok) { 'Green' } else { 'Yellow' }
    Write-Host ("    $box " + $name.PadRight(20) + " $detail") -ForegroundColor $col
}
function Fail ($m, $fix) {
    Write-Host ""
    Write-Host "  FAILED: $m" -ForegroundColor Red
    if ($fix) { Write-Host "  Fix:    $fix" -ForegroundColor Yellow }
    Write-Host ""
    Stop-Tunnel
    exit 1
}

# ------------------------------------------------------------------ tunnel mgmt

$script:TunnelProc = $null
$script:TunnelLog  = $null

function Stop-Tunnel {
    if ($script:TunnelProc -and -not $script:TunnelProc.HasExited) {
        try { Stop-Process -Id $script:TunnelProc.Id -Force -ErrorAction Stop } catch { }
        Write-Info "Tunnel stopped (pid $($script:TunnelProc.Id))."
    }
}

# Closes whatever holds the port, including a tunnel this run did not start.
# Stop-Tunnel above can only reach a process we launched; a tunnel left behind by
# an earlier run is a foreign pid, and it is the one that strands the next run --
# the port still listens, so this script reuses it, while the session behind it is
# long dead. Returns the pids it stopped.
function Stop-TunnelOnPort ($p) {
    $stopped = @()
    try {
        $owners = Get-NetTCPConnection -LocalPort $p -State Listen -ErrorAction Stop |
                  Select-Object -ExpandProperty OwningProcess -Unique
    } catch {
        Write-Warn "Cannot enumerate listeners on port $p - close the tunnel by hand."
        return $stopped
    }
    foreach ($owner in $owners) {
        $proc = Get-Process -Id $owner -ErrorAction SilentlyContinue
        if (-not $proc) { continue }
        try {
            Stop-Process -Id $owner -Force -ErrorAction Stop
            $stopped += $owner
            Write-Info "Closed $($proc.ProcessName) (pid $owner) on port $p."
        } catch {
            Write-Warn "Could not stop pid $owner ($($proc.ProcessName)) - stop it by hand."
        }
    }
    if (-not $stopped) { Write-Info "Nothing was listening on port $p." }
    return $stopped
}

# ------------------------------------------------------------------- token mgmt

# The token is a8k_ + base64(nodeKey), ~36 chars. The CarSky REST API key is a different,
# much longer credential -- pasting that one here is the common mistake.
function Test-TokenShape ($tok) {
    if ([string]::IsNullOrWhiteSpace($tok)) { return "The token is empty." }
    if (-not $tok.StartsWith('a8k_'))       { return "The token does not start with 'a8k_'." }
    if ($tok.Length -gt 60)                 { return "This looks like the CarSky REST API key ($($tok.Length) chars), not the ADB tunnel token (~36)." }
    return $null
}

function Get-TokenNode ($tok) {
    try {
        $b64 = $tok.Substring(4)
        while ($b64.Length % 4 -ne 0) { $b64 += '=' }
        return [Text.Encoding]::UTF8.GetString([Convert]::FromBase64String($b64))
    } catch { return '(could not decode)' }
}

# The platform mints a new token on every deploy, and the old one then 404s on the
# WebSocket upgrade rather than failing visibly -- the tunnel still listens, so the run
# dies later at 'adb offline' with no hint that the credential is the problem. Prompt for
# the new one and persist it, rather than making the operator find the file.
# Returns $null when the run cannot prompt or the operator declines.
function Request-NewToken {
    if ([Console]::IsInputRedirected) {
        Write-Info "Input is redirected, so this run cannot prompt. Update $TokenFile and re-run."
        return $null
    }
    Write-Host ""
    Write-Host "  A deploy mints a new ADB token; the one on disk no longer resolves." -ForegroundColor Yellow
    Write-Host "  Copy it: Devices -> KIS -> Connect -> IVI ADB -> Local ADB" -ForegroundColor Yellow
    Write-Host "  Paste the a8k_ value, or the whole reach-backend command line." -ForegroundColor Yellow
    Write-Host "  Press Enter on an empty line to give up." -ForegroundColor DarkGray
    for ($i = 1; $i -le 3; $i++) {
        $entered = (Read-Host "  token").Trim()
        if (-not $entered) { return $null }
        # Tolerate a pasted command line -- lifting --key out of it is what an operator
        # copying from the dialog actually has on the clipboard.
        if ($entered -match '--key\s+(\S+)') { $entered = $Matches[1] }
        $entered = $entered.Trim('"', "'")
        $bad = Test-TokenShape $entered
        if ($bad) { Write-Warn $bad; continue }
        try {
            Set-Content -Path $TokenFile -Value $entered -Encoding ASCII -NoNewline -ErrorAction Stop
            Write-Ok "Saved to $TokenFile"
        } catch {
            Write-Warn "Could not write $TokenFile - using the token for this run only."
        }
        Write-Ok "Now targets node '$(Get-TokenNode $entered)'"
        return $entered
    }
    return $null
}

function Test-Port ($p) {
    $c = New-Object System.Net.Sockets.TcpClient
    try   { $c.Connect('127.0.0.1', $p); return $true }
    catch { return $false }
    finally { $c.Dispose() }
}

# ------------------------------------------------------------------- locate all

Write-Host ""
Write-Host "  IVI APK installer - tunnel, install, evidence" -ForegroundColor White
Write-Host "  Authority: documents/Delivery/apk-deploy.md Step 3" -ForegroundColor DarkGray

Write-Step "Locating tools and files"

$RepoRoot  = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$ReachExe  = Join-Path $PSScriptRoot 'reach_be\reach\reach-backend.exe'
$TokenFile = Join-Path $RepoRoot 'secrets\reach-adb-token-ivi.txt'
$LogDir    = Join-Path $PSScriptRoot 'logs'
if (-not $Apk) { $Apk = Join-Path $PSScriptRoot 'app-debug.apk' }

$Gateway = 'https://hackathon-2.carsky.io'

if ($PSVersionTable.PSVersion.Major -lt 5) {
    Fail "PowerShell $($PSVersionTable.PSVersion) is too old." "Windows PowerShell 5.1 ships with Windows 10 and 11 on every architecture."
}

if (-not (Test-Path $ReachExe)) {
    Fail "Tunnel CLI not found at $ReachExe" "Unpack the organizers' reach_be zip into tools\apk-uploader\."
}
# The organizers ship an x64 build only. Windows 11 on ARM64 runs it under x64 emulation,
# so no separate binary is needed -- but say which path is in play if the tunnel later dies.
Write-Ok "Tunnel CLI  ($env:PROCESSOR_ARCHITECTURE host$(if ($env:PROCESSOR_ARCHITECTURE -eq 'ARM64') { ', x64 emulated' }))"

# adb: PATH first, then every location an Android SDK is commonly installed to. Hardcoding
# one path breaks on any machine that put the SDK elsewhere.
$Adb = $null
$onPath = Get-Command adb.exe -ErrorAction SilentlyContinue
if ($onPath) { $Adb = $onPath.Source }
else {
    $candidates = @(
        (Join-Path $env:LOCALAPPDATA 'Android\Sdk\platform-tools'),
        $(if ($env:ANDROID_HOME)     { Join-Path $env:ANDROID_HOME 'platform-tools' }),
        $(if ($env:ANDROID_SDK_ROOT) { Join-Path $env:ANDROID_SDK_ROOT 'platform-tools' }),
        (Join-Path $env:USERPROFILE 'Android\Sdk\platform-tools'),
        (Join-Path $env:ProgramFiles 'Android\Android Studio\platform-tools'),
        (Join-Path ${env:ProgramFiles(x86)} 'Android\android-sdk\platform-tools')
    ) | Where-Object { $_ }

    foreach ($c in $candidates) {
        $try = Join-Path $c 'adb.exe'
        if (Test-Path $try) { $Adb = $try; break }
    }
}
if (-not $Adb) {
    Fail "adb.exe not found on PATH or in any standard Android SDK location." `
         "Install Android SDK platform-tools, then either add its folder to PATH or set ANDROID_HOME."
}
Write-Ok "adb  ->  $Adb"

if (-not $SkipInstall) {
    if (-not (Test-Path $Apk)) {
        Fail "APK not found at $Apk" "Build it, or download the CI app-debug-apk artifact (deploy note Step 1)."
    }
    $apkInfo = Get-Item $Apk
    Write-Ok "APK  ->  $($apkInfo.Name)  ($([math]::Round($apkInfo.Length/1MB,1)) MB, built $($apkInfo.LastWriteTime.ToString('yyyy-MM-dd HH:mm')))"
}

# ---------------------------------------------------------------- token & shape

Write-Step "Reading the ADB tunnel token"

if (-not $Token) {
    if (-not (Test-Path $TokenFile)) {
        Fail "No token. $TokenFile does not exist." `
             "Copy the a8k_ value from the Local ADB dialog into that file, or pass -Token a8k_..."
    }
    $Token = (Get-Content $TokenFile -Raw).Trim()
}

$shapeError = Test-TokenShape $Token
if ($shapeError) {
    Fail $shapeError "The tunnel token is the short a8k_ value in the Local ADB dialog - a different credential from the CarSky REST API key."
}

Write-Ok "Token accepted - targets node '$(Get-TokenNode $Token)'"
Write-Info "If that is not the IVI node of the Room you just deployed, the token is stale:"
Write-Info "re-copy it from Devices -> KIS -> Connect -> IVI ADB -> Local ADB."

# ----------------------------------------------------------------- open tunnel

function Open-Tunnel ($tok) {
    if (Test-Port $Port) {
        $script:TunnelReused = $true
        Write-Warn "Port $Port is already serving - reusing it (another tunnel is probably open)."
        return
    }
    $script:TunnelReused = $false
    if (-not (Test-Path $LogDir)) { New-Item -ItemType Directory -Path $LogDir -Force | Out-Null }
    $script:TunnelLog = Join-Path $LogDir 'tunnel-last.log'

    $script:TunnelProc = Start-Process -FilePath $ReachExe `
        -ArgumentList @('adb', '--gateway', $Gateway, '--key', $tok, '--port', "$Port") `
        -PassThru -WindowStyle Hidden `
        -RedirectStandardOutput $script:TunnelLog `
        -RedirectStandardError  "$script:TunnelLog.err"

    $deadline = (Get-Date).AddSeconds(25)
    while ((Get-Date) -lt $deadline) {
        if ($script:TunnelProc.HasExited) {
            $why = ''
            foreach ($f in @($script:TunnelLog, "$script:TunnelLog.err")) {
                if (Test-Path $f) { $why += ((Get-Content $f -Raw).Trim() + "`n") }
            }
            Write-Host ""
            Write-Host "  Tunnel output:" -ForegroundColor DarkGray
            Write-Host "  $($why.Trim())" -ForegroundColor DarkGray
            Fail "The tunnel exited immediately (exit $($script:TunnelProc.ExitCode))." `
                 "Three causes, in order of likelihood: the token is stale (redeploy mints a new one - re-copy it); the IVI node is not Running yet; port $Port is taken (pass -Port 5556)."
        }
        if (Test-Port $Port) { break }
        Start-Sleep -Milliseconds 500
    }
    if (-not (Test-Port $Port)) { Fail "The tunnel did not start listening within 25s." "Check $script:TunnelLog." }
    Write-Ok "Tunnel serving (pid $($script:TunnelProc.Id)), log: $script:TunnelLog"
}

# A stale token does not fail the upgrade loudly: the gateway answers the WebSocket
# handshake 404 and reach-backend keeps retrying, so the port listens while no ADB
# transport ever forms. The log is the only place that says so.
function Test-TunnelRejected {
    if (-not $script:TunnelLog) { return $false }
    foreach ($f in @($script:TunnelLog, "$script:TunnelLog.err")) {
        if (Test-Path $f) {
            if ((Get-Content $f -Raw -ErrorAction SilentlyContinue) -match 'Unexpected server response: 404') { return $true }
        }
    }
    return $false
}

function Connect-Guest ($serial) {
    & $Adb connect $serial | Out-Null
    $deadline = (Get-Date).AddSeconds(30)
    while ((Get-Date) -lt $deadline) {
        $devices = & $Adb devices 2>&1 | Out-String
        if ($devices -match [regex]::Escape($serial) + '\s+device\b') { return $true }
        Start-Sleep -Seconds 1
        & $Adb connect $serial | Out-Null
    }
    return $false
}

Write-Step "Opening the tunnel on 127.0.0.1:$Port"

$script:TunnelReused = $false
Open-Tunnel $Token

# ------------------------------------------------------------------ adb connect

Write-Step "Connecting adb to the guest"

$serial = "localhost:$Port"
$connected = Connect-Guest $serial

# A 404 means the gateway has no such ADB session -- the node key in the token belongs to
# a deployment that no longer exists. Waiting for the node to boot cannot fix that, so
# offer the one thing that can: a fresh token.
if (-not $connected -and (Test-TunnelRejected)) {
    Write-Host ""
    Write-Warn "The gateway answered 404 on every upgrade - this token no longer resolves."
    Write-Info "It decodes to node '$(Get-TokenNode $Token)', which is not a node of any live deployment."

    $newToken = Request-NewToken
    if ($newToken) {
        $Token = $newToken
        Stop-Tunnel
        Stop-TunnelOnPort $Port | Out-Null
        Start-Sleep -Seconds 1
        Write-Info "Reopening the tunnel with the new token."
        Open-Tunnel $Token
        $connected = Connect-Guest $serial
    }
}

if (-not $connected) {
    Write-Host ""
    Write-Host (& $Adb devices | Out-String) -ForegroundColor DarkGray
    if (Test-TunnelRejected) {
        Fail "The gateway rejected the tunnel token (404 on every upgrade)." `
             "Re-copy the a8k_ value from Devices -> KIS -> Connect -> IVI ADB -> Local ADB into $TokenFile. A deploy mints a new one, so the node key in the old token no longer exists."
    }
    Fail "The guest never reached state 'device' (offline, or not listed)." `
         "The tunnel is serving and the token resolves, so the Skycraft node may still be booting. Wait for it to go green in the Deployment Viewer, then re-run. If it is already green, its adbd may be wedged - Restart Node in the Inspector."
}
Write-Ok "$serial  device"
Write-Info "All adb calls below are pinned with -s $serial, so a local emulator cannot be hit by mistake."

$sdk = (& $Adb -s $serial shell getprop ro.build.version.sdk 2>&1 | Out-String).Trim()
$auto = (& $Adb -s $serial shell pm list features 2>&1 | Out-String)
if ($sdk -match '^\d+$' -and [int]$sdk -lt 29) { Fail "Guest is API $sdk, below the APK's minSdk 29." "Wrong node or wrong AAOS artifact." }
Write-Ok "Guest API $sdk; automotive feature: $(if ($auto -match 'android\.hardware\.type\.automotive') { 'present' } else { 'ABSENT' })"

# ------------------------------------------------------------- room networking

# The AAOS guest boots with its bridge NIC named 'buried_eth0'. AAOS EthernetTracker does
# not recognise that name, so netd never adopts the interface, it never takes the Room
# address, and every R4 datagram sent to the node's pin address is dropped before the guest
# sees it -- the app binds its socket and waits forever. Renaming the NIC to 'eth0' makes
# EthernetTracker adopt it; netd then creates the 'eth0' network, its routing table and its
# policy rules by itself, so only the address still has to be set by hand.
#
# Organiser-supplied procedure. It is a live mutation of the running guest and does NOT
# survive a guest reboot or a redeploy -- re-run this script after either.

if ($SkipNetworkFix) {
    Write-Step "Skipping the Room-network fix (-SkipNetworkFix)"
    Write-Warn "No R4 can reach the app in this state. Evidence below will show zero [RX]."
} else {
    Write-Step "Putting the guest on the Room network ($IviAddr)"

    $addrs = & $Adb -s $serial shell ip -4 addr show 2>&1 | Out-String

    if ($addrs -match ([regex]::Escape($IviAddr) + '/')) {
        Write-Ok "Already on the Room subnet - nothing to change"
    }
    elseif (((& $Adb -s $serial shell su 0 id 2>&1 | Out-String) -notmatch 'uid=0')) {
        Write-Warn "No root on this guest, so the NIC cannot be renamed. R4 will not arrive."
        Write-Warn "This is a finding to report, not something the script can work around."
    }
    else {
        if ($addrs -match 'buried_eth0') {
            # Chained in one shell so the NIC can never be left down by an interrupted call.
            & $Adb -s $serial shell "su 0 sh -c 'ip link set buried_eth0 down; ip link set buried_eth0 name eth0; ip link set eth0 up'" 2>&1 | Out-Null
            Write-Ok "Renamed buried_eth0 -> eth0 (EthernetTracker now adopts it)"
            Start-Sleep -Seconds 4
        } else {
            Write-Info "No buried_eth0 present - assuming eth0 already exists."
        }

        & $Adb -s $serial shell "su 0 ifconfig eth0 $IviAddr/24 up" 2>&1 | Out-Null

        # netd normally creates these already; 'File exists' is the expected, healthy answer.
        & $Adb -s $serial shell "su 0 ip route add 10.99.0.0/24 dev eth0 table eth0 proto static scope link" 2>&1 | Out-Null
        if ($RoomGateway) {
            & $Adb -s $serial shell "su 0 ip route add default via $RoomGateway dev eth0 table eth0 proto static" 2>&1 | Out-Null
            Write-Info "Default route via $RoomGateway added to the eth0 table."
        }

        Start-Sleep -Seconds 3
        $addrs = & $Adb -s $serial shell ip -4 addr show eth0 2>&1 | Out-String
        if ($addrs -match ([regex]::Escape($IviAddr) + '/')) {
            Write-Ok "eth0 is $IviAddr/24 - the Room can now reach the app"
        } else {
            Write-Warn "eth0 did not take $IviAddr. R4 will not arrive; check the pin address."
        }
    }
}

# ---------------------------------------------------------------------- install

$Package = 'com.hackathon.v2x.ivi'

# A package that was never on the guest cannot have been running, so the self-start check
# below only means something when the app was already installed before this run.
$wasInstalled = ((& $Adb -s $serial shell pm list packages 2>&1 | Out-String) -match [regex]::Escape($Package))

if ($SkipInstall) {
    Write-Step "Skipping install (-SkipInstall)"
} else {
    Write-Step "Installing $($Package) (this takes ~30-60s for a 28 MB APK)"

    $out = & $Adb -s $serial install -r $Apk 2>&1 | Out-String
    if ($out -notmatch 'Success') {
        Write-Host $out -ForegroundColor DarkGray
        Fail "adb install did not report Success." "Read the INSTALL_FAILED_ reason above; deploy note Step 3 covers the common ones."
    }
    Write-Ok "Success"

    $pkgs = & $Adb -s $serial shell pm list packages 2>&1 | Out-String
    if ($pkgs -match [regex]::Escape($Package)) { Write-Ok "package:$Package present on the guest" }
    else { Fail "Install reported Success but the package is not listed." "Re-run; if it persists this is a finding worth recording." }
}

# ----------------------------------------------------------------- launch check

Write-Step "Checking the app on the guest"

# A node showing Running in the blueprint says the VM is up, never that the app is: this
# Room ran green for hours with no APK installed at all. Only the guest can answer these.
function Get-AppState {
    $s = @{}
    $s.AppPid  = (& $Adb -s $serial shell pidof $Package 2>&1 | Out-String).Trim()
    $s.Version = ''
    $d = & $Adb -s $serial shell "dumpsys package $Package | grep versionName" 2>&1 | Out-String
    if ($d -match 'versionName=(\S+)') { $s.Version = $Matches[1] }
    $a = & $Adb -s $serial shell "dumpsys activity activities | grep -i ResumedActivity" 2>&1 | Out-String
    $s.Resumed = ($a -match [regex]::Escape("$Package/.MainActivity"))
    $w = & $Adb -s $serial shell "dumpsys window | grep -i mCurrentFocus" 2>&1 | Out-String
    $s.Focused = ($w -match [regex]::Escape($Package))
    $p = & $Adb -s $serial shell "dumpsys power | grep -i mWakefulness=" 2>&1 | Out-String
    $s.Awake = ($p -match 'mWakefulness=Awake')
    return $s
}

Start-Sleep -Seconds 3
$st = Get-AppState

# The deploy note Step 4 fixes MainActivity as the node's only MAIN/LAUNCHER activity, so on
# a correct deploy it is already resumed. Starting it by hand is a fallback, not a step.
$neededStart = $false
if (-not $st.AppPid -or -not $st.Resumed) {
    $neededStart = $true
    & $Adb -s $serial shell am start -n "$Package/.MainActivity" 2>&1 | Out-String | Out-Null
    Start-Sleep -Seconds 5
    $st = Get-AppState
}

Write-Check ([bool]$st.Version)  'package installed' $(if ($st.Version) { "$Package $($st.Version)" } else { "$Package NOT FOUND" })
Write-Check ([bool]$st.AppPid)   'process running'   $(if ($st.AppPid) { "pid $($st.AppPid)" } else { 'no process' })
Write-Check $st.Resumed          'MainActivity up'   $(if ($st.Resumed) { 'topResumedActivity' } else { 'not the resumed activity' })
Write-Check $st.Focused          'window focused'    $(if ($st.Focused) { 'holds input focus' } else { 'another window has focus' })
Write-Check $st.Awake            'screen awake'      $(if ($st.Awake) { 'mWakefulness=Awake' } else { 'asleep - the HMI will not render' })

if ($neededStart -and $wasInstalled -and -not $SkipInstall) {
    Write-Info "It needed a manual start though it was already installed - the deploy note"
    Write-Info "calls that a finding, because a correct deploy starts it by itself."
} elseif ($neededStart) {
    Write-Info "Started by hand, which is expected on a first install."
}

# -------------------------------------------------------------------- evidence

Write-Step "Collecting evidence logcat"

if (-not (Test-Path $LogDir)) { New-Item -ItemType Directory -Path $LogDir -Force | Out-Null }
$stamp   = Get-Date -Format 'yyyyMMdd-HHmmss'
$logFile = Join-Path $LogDir "ivi-logcat-$stamp.txt"

Write-Info "Waiting 15s for the simulator's 1 Hz stream to produce warnings..."
Start-Sleep -Seconds 15

# -d dumps the ring buffer rather than streaming: the app was already running before we
# attached, so the startup lines are only reachable this way (test guide Step 3-2).
& $Adb -s $serial logcat -d -v threadtime -s IVI_V2X R4ListenerService R4Deserializer MainViewModel WarningViewModel `
    2>&1 | Out-File -FilePath $logFile -Encoding utf8
$log = Get-Content $logFile -Raw
if (-not $log) { $log = '' }

$fatal = & $Adb -s $serial logcat -d -b crash 2>&1 | Out-String

Write-Ok "Saved: $logFile  ($((Get-Content $logFile | Measure-Object -Line).Lines) lines)"

# ---------------------------------------------------------------------- verdict

# No [UI] check: this build has no logging in its UI layer at all, so the mode switch is
# only observable on the Screen widget. A check that can never pass is a false failure.
$checks = @(
    @{ Name = 'UDP socket bound on 47300';         Pass = ($log -match 'UDP socket open on port 47300' -or $log -match '\[LINK\] state=bound') },
    @{ Name = 'R4 messages received  ([RX])';      Pass = ($log -match '\[RX\]') },
    @{ Name = 'Provenance source=v2x_relayed';     Pass = ($log -match 'source=v2x_relayed') },
    @{ Name = 'Risk reached high';                 Pass = ($log -match 'riskState=high') },
    @{ Name = 'No fatal exception';                Pass = -not ($fatal -match 'FATAL EXCEPTION' -and $fatal -match [regex]::Escape($Package)) }
)

Write-Host ""
Write-Host "  ------------------------------------------------------" -ForegroundColor White
Write-Host "  EVIDENCE" -ForegroundColor White
Write-Host "  ------------------------------------------------------" -ForegroundColor White
foreach ($c in $checks) {
    $box = if ($c.Pass) { '[x]' } else { '[ ]' }
    $col = if ($c.Pass) { 'Green' } else { 'Yellow' }
    Write-Host ("  $box " + $c.Name) -ForegroundColor $col
}

$passed = @($checks | Where-Object { $_.Pass }).Count
Write-Host ""
if ($passed -eq $checks.Count) {
    Write-Host "  The app is receiving R4 and rendering warnings from relayed data." -ForegroundColor Green
} elseif ($log -match '\[RX\]') {
    Write-Host "  Partly evidenced. The app is alive and parsing, but not every line appeared." -ForegroundColor Yellow
    Write-Host "  Read $logFile before concluding - a missing line can just mean the" -ForegroundColor Yellow
    Write-Host "  scenario had not reached its high-risk step yet. Re-run with -SkipInstall to sample again." -ForegroundColor Yellow
} else {
    Write-Host "  No R4 reached the app. The install is fine; the stream is not arriving." -ForegroundColor Yellow
    Write-Host "  Check the producer node's log for [TX] lines, and that it targets 10.99.0.13:47300." -ForegroundColor Yellow
}

Write-Host ""
Write-Host "  STILL YOURS - no script can do these:" -ForegroundColor White
Write-Host "  1. Look at the Screen widget: EGO and B solid, C dashed with a pulsing glow" -ForegroundColor Gray
Write-Host "     and the badge [V2X] C - <d> m - RISK: HIGH. A yellow [? UNKNOWN SOURCE]" -ForegroundColor Gray
Write-Host "     where ghost C belongs is a blocking defect on the approach scenario." -ForegroundColor Gray
Write-Host "     This build logs nothing from its UI layer, so the mode switch to the" -ForegroundColor Gray
Write-Host "     Warning View is confirmable ONLY on screen - the logs cannot show it." -ForegroundColor Gray
Write-Host "  2. Record or screenshot it - set Recorder Part BEFORE the run starts." -ForegroundColor Gray
Write-Host "  (test guide Step 3-1)" -ForegroundColor DarkGray

# ---------------------------------------------------------------------- cleanup

Write-Host ""
if ($CloseTunnel) {
    # Explicit: close the port whoever opened it. This is the only branch that can
    # reach a tunnel inherited from an earlier run.
    Stop-Tunnel
    Stop-TunnelOnPort $Port | Out-Null
    Write-Host "  Tunnel on port $Port closed." -ForegroundColor Cyan
    Write-Host "  Collecting logs needs it again - the collector's guest half is skipped without it." -ForegroundColor DarkGray
} elseif ($script:TunnelReused) {
    # Started by an earlier run, so the default leaves it alone - but print the pid,
    # because a tunnel nobody can address is what strands the next run.
    $owners = @()
    try {
        $owners = Get-NetTCPConnection -LocalPort $Port -State Listen -ErrorAction Stop |
                  Select-Object -ExpandProperty OwningProcess -Unique
    } catch { }
    Write-Host "  Tunnel on port $Port was already open and has been left alone." -ForegroundColor Cyan
    Write-Host "  Live logs:  adb -s $serial logcat -s IVI_V2X" -ForegroundColor Cyan
    if ($owners) {
        Write-Host "  Stop it:    Stop-Process -Id $($owners -join ',') -Force" -ForegroundColor Cyan
        Write-Host "              or re-run with -CloseTunnel" -ForegroundColor Cyan
    }
} elseif ($KeepTunnel -and $script:TunnelProc) {
    Write-Host "  Tunnel left running on port $Port (pid $($script:TunnelProc.Id))." -ForegroundColor Cyan
    Write-Host "  Live logs:  adb -s $serial logcat -s IVI_V2X" -ForegroundColor Cyan
    Write-Host "  Stop it:    Stop-Process -Id $($script:TunnelProc.Id)" -ForegroundColor Cyan
} else {
    Stop-Tunnel
    Write-Host "  Done. Re-run with -KeepTunnel to keep adb usable afterwards." -ForegroundColor DarkGray
}
Write-Host ""
