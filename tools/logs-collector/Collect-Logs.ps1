<#
.SYNOPSIS
  One-shot evidence collector: pick a test, pull every node log off CarSky and
  every guest-side log off the AAOS IVI over ADB, into ./test-report/<test-name>/.

.DESCRIPTION
  Automates Step 3 "Evidence collection" of documents/Delivery/testing-guide.md,
  which stays the authority. The test note's Path 2 is a dozen commands pasted by
  hand with the node keys filled in from a second call; this runs the same commands,
  resolves the keys itself, and ends in the note's own evidence checklist.

  Nothing here deploys, installs or mutates the Room -- it only reads. A run that
  finds the deployment stopped stops too, because logs of a Room that is not running
  are evidence of nothing.

  What it collects, into ./test-report/<test-name>/ relative to the current directory:

      node-<slug>.txt              one per container node, from the REST log endpoint
      skycraft-<slug>-vmhost.txt   the Skycraft VM host's own output (see below)
      app-logcat.txt               the IVI app's tagged logcat -- the [RX] evidence
      app-crash.txt                Android's separate crash buffer
      guest-udp6.txt               /proc/net/udp6, proving the socket is bound
      guest-ifaces.txt             ip -4 addr, proving the guest took the pin address
      summary.txt                  this run's checklist, in plain text

  The Skycraft node's REST log is the VM host's WebRTC/GPU output and carries nothing
  the app wrote, so it is collected for completeness under a name the pcap extractor
  does not glob and the [TX] scan does not read. The app's log comes only from adb.

  The ADB tunnel is optional. Without it the CarSky half still lands and the guest
  half is skipped, reported rather than failed -- a node-side collection is useful on
  its own. Open the tunnel with tools\apk-uploader\install-ivi-apk.ps1 -SkipInstall.

.PARAMETER Test
  Which test to collect, by number or by name:

      1  system-test          the full four-node blueprint   (m1_system_test)
      2  ivi-isolated-test    IVI + mocked ADA               (phase5_smoked_test)
      3  ada-isolated-test    ADA + its bench                (phase4_smoked_test)
      4  v2x-isolated-test    V2X + the scenario player      (phase1_smoked_test)

  Omitted, the script asks.

.PARAMETER BaseUrl
  CarSky gateway. Default https://hackathon-2.carsky.io.

.PARAMETER ApiKeyFile
  File holding the CarSky REST API key. Default secrets\carsky-api-key.txt at the
  repo root. The key is never printed, never written to any output file, and never
  put in a URL.

.PARAMETER Port
  Local port the ADB tunnel is expected on. Default 5555.

.PARAMETER Tail
  How many lines to pull per node log. Default 5000.

.PARAMETER OutRoot
  Where the run folder is created. Default .\test-report, relative to the current
  directory -- so the folder lands beside wherever you ran this from.

.EXAMPLE
  .\tools\logs-collector\Collect-Logs.ps1 -Test 2
  Collects the IVI isolated test into .\test-report\ivi-isolated-test\.

.EXAMPLE
  .\tools\logs-collector\Collect-Logs.ps1 -Test system-test -Tail 20000
  The full blueprint, with a deeper log window.

.NOTES
  Exit status: 0 collected | 1 the deployment is not running, or another hard stop |
  2 usage error.
#>
[CmdletBinding()]
param(
    [string] $Test,
    [string] $BaseUrl    = 'https://hackathon-2.carsky.io',
    [string] $ApiKeyFile,
    [int]    $Port       = 5555,
    [int]    $Tail       = 5000,
    [string] $OutRoot    = '.\test-report'
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
    Write-Host ("    $box " + $name.PadRight(24) + " $detail") -ForegroundColor $col
}
# [-] is the third state: not a pass and not a failure, but nothing this Room can
# answer -- a node with no log stream, or guest evidence in a Room with no guest.
# Reporting those as [ ] would read as a defect in a Room that is behaving.
function Write-Skip   ($name, $detail) {
    Write-Host ("    [-] " + $name.PadRight(24) + " $detail") -ForegroundColor DarkGray
}

# summary.txt is the same checklist without the colour, so a run can be read back
# after the window is gone. Every line printed as a result is echoed into it.
$script:Summary = New-Object System.Collections.ArrayList
function Add-Summary ($line) { [void]$script:Summary.Add($line) }
function Add-SummaryCheck ($ok, $name, $detail) {
    $box = if ($ok) { '[x]' } else { '[ ]' }
    Add-Summary ("$box " + $name.PadRight(24) + " $detail")
}
function Add-SummarySkip ($name, $detail) { Add-Summary ("[-] " + $name.PadRight(24) + " $detail") }

function Fail ($m, $fix) {
    Write-Host ""
    Write-Host "  FAILED: $m" -ForegroundColor Red
    if ($fix) { Write-Host "  Fix:    $fix" -ForegroundColor Yellow }
    Write-Host ""
    exit 1
}
function Exit-Usage ($m) {
    Write-Host ""
    Write-Host "  USAGE: $m" -ForegroundColor Red
    Write-Host ""
    exit 2
}

# ------------------------------------------------------------------- the tests

# One row per selectable test: the folder the evidence lands in, and the CarSky
# blueprint it is deployed from. Adding a test is adding a row.
#
# Blueprint names are the platform's own, underscore-separated. The deployment
# CarSky builds from one is named <blueprint>-deploy, and that is the name
# /deployments/find matches and the name the summary reports -- so the suffix is
# derived below, never typed into a row.
$Tests = @(
    [pscustomobject]@{ Option = 1; Name = 'system-test';       Blueprint = 'm1_system_test';      What = 'full blueprint - bench, V2X, ADA, IVI' },
    [pscustomobject]@{ Option = 2; Name = 'ivi-isolated-test';  Blueprint = 'phase5_smoked_test';  What = 'IVI ECU with the mocked ADA producer' },
    [pscustomobject]@{ Option = 3; Name = 'ada-isolated-test';  Blueprint = 'phase4_smoked_test';  What = 'ADA ECU with its bench' },
    [pscustomobject]@{ Option = 4; Name = 'v2x-isolated-test';  Blueprint = 'phase1_smoked_test';  What = 'V2X ECU with the scenario player' }
)

# CarSky names a deployment after its blueprint with a -deploy suffix.
function Get-DeploymentName ($blueprint) { "$blueprint-deploy" }

function Resolve-Test ($value) {
    $v = "$value".Trim()
    foreach ($t in $Tests) {
        if ($v -eq "$($t.Option)" -or $v -eq $t.Name) { return $t }
    }
    return $null
}

Write-Host ""
Write-Host "  Test log collector - CarSky node logs + AAOS guest evidence" -ForegroundColor White
Write-Host "  Authority: documents/Delivery/testing-guide.md Step 3" -ForegroundColor DarkGray

if ($PSVersionTable.PSVersion.Major -lt 5) {
    Fail "PowerShell $($PSVersionTable.PSVersion) is too old." "Windows PowerShell 5.1 ships with Windows 10 and 11 on every architecture."
}

# PS 5.1 defaults to TLS 1.0 on some hosts; the gateway needs 1.2. Set it before the
# first call rather than debugging a bare 'connection closed' later.
try {
    [Net.ServicePointManager]::SecurityProtocol = [Net.ServicePointManager]::SecurityProtocol -bor [Net.SecurityProtocolType]::Tls12
} catch { }

Write-Step "Choosing the test"

$Selected = $null
if ($Test) {
    $Selected = Resolve-Test $Test
    if (-not $Selected) {
        Exit-Usage "unknown test '$Test'. Use 1-4, or one of: $(($Tests | ForEach-Object { $_.Name }) -join ', ')."
    }
} else {
    Write-Host ""
    foreach ($t in $Tests) {
        Write-Host ("    $($t.Option)) " + $t.Name.PadRight(20) + " $($t.What)") -ForegroundColor Gray
    }
    Write-Host ""
    $answer   = Read-Host "    Which test? [1-4]"
    $Selected = Resolve-Test $answer
    if (-not $Selected) { Exit-Usage "'$answer' is not one of 1-4." }
}

Write-Ok "$($Selected.Name)  -  $($Selected.What)"
$WantedDeployment = Get-DeploymentName $Selected.Blueprint
Write-Info "Looking for '$WantedDeployment', the deployment of blueprint '$($Selected.Blueprint)'."

# ------------------------------------------------------------------ credentials

Write-Step "Reading the CarSky API key"

$RepoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
if (-not $ApiKeyFile) { $ApiKeyFile = Join-Path $RepoRoot 'secrets\carsky-api-key.txt' }

if (-not (Test-Path -LiteralPath $ApiKeyFile)) {
    Fail "No API key. $ApiKeyFile does not exist." `
         "Put the CarSky REST API key in that file, or pass -ApiKeyFile <path>. It is git-ignored."
}
$ApiKey = (Get-Content -LiteralPath $ApiKeyFile -Raw).Trim()
if ([string]::IsNullOrWhiteSpace($ApiKey)) { Fail "The API key file is empty." "Paste the CarSky REST API key into $ApiKeyFile." }

# Length only -- the value itself is never printed, logged or written to any file.
Write-Ok "Key loaded from $(Split-Path -Leaf $ApiKeyFile) ($($ApiKey.Length) chars, not shown)"

$Api     = "$($BaseUrl.TrimEnd('/'))/api/v1"
$Headers = @{ 'X-API-Key' = $ApiKey }

function Invoke-Api ($path) {
    $r = Invoke-RestMethod -Method Get -Uri "$Api$path" -Headers $Headers -TimeoutSec 60
    # Assign first, then return. Invoke-RestMethod writes a JSON array to the pipeline
    # as ONE non-enumerated object, so `return Invoke-RestMethod ...` hands the caller a
    # single item that merely happens to be an array -- @() around it counts 1 whatever
    # the node count is. Returning the variable enumerates it, which is what callers expect.
    return $r
}

# ------------------------------------------------------------------ deployment

Write-Step "Resolving the deployment on $BaseUrl"

$found = $null
try {
    $found = Invoke-Api "/deployments/find?blueprint=$([uri]::EscapeDataString($WantedDeployment))"
} catch {
    Fail "The find call failed: $($_.Exception.Message)" `
         "Check the gateway is reachable and the API key is current. Nothing was collected."
}

# The endpoint answers with an array, and an unmatched name is 200 with an empty one
# rather than a 404 -- so 'no rows' is the not-deployed signal, not an HTTP status.
# $hits, never $matches: $Matches is a PowerShell automatic variable that every
# -match in this script overwrites, so a deployment held there would not survive.
$hits = @($found | Where-Object { $_ })
if ($hits.Count -eq 0) {
    Fail "'$WantedDeployment' is not running in CarSky - nothing matches that name." `
         "Deploy the $($Selected.Name) blueprint, wait for its nodes to go green, then re-run."
}

$Dep = $hits[0]
if ($hits.Count -gt 1) {
    $exact = @($hits | Where-Object { $_.name -eq $WantedDeployment })
    if ($exact.Count -gt 0) { $Dep = $exact[0] }
    Write-Warn "$($hits.Count) deployments match '$WantedDeployment'; using '$($Dep.name)'."
}

$RoomId = $Dep.roomId
Write-Ok "$($Dep.name)   room $RoomId"

# The find row carries a status, but the status endpoint is the live one -- a row can
# be stale by a redeploy. Believe the second, and stop unless it says RUNNING.
$statusObj = $null
try { $statusObj = Invoke-Api "/deployments/$RoomId/status" } catch { }

$DepStatus = ''
if ($statusObj -and $statusObj.status) { $DepStatus = "$($statusObj.status)" }
elseif ($Dep.status)                   { $DepStatus = "$($Dep.status)" }

$DepRunning = ($DepStatus -eq 'RUNNING')
if (-not $DepRunning) {
    $shown = $DepStatus
    if (-not $shown) { $shown = '(no status reported)' }
    $msg = ''
    if ($statusObj -and $statusObj.message) { $msg = " - $($statusObj.message)" }
    Fail "The deployment '$WantedDeployment' is not running in CarSky: status is $shown$msg." `
         "Start or redeploy that blueprint and wait for RUNNING, then re-run. Logs of a stopped Room prove nothing."
}
Write-Ok "status RUNNING   namespace $($statusObj.namespace)"

$Stamp = Get-Date -Format 'yyyy-MM-dd HH:mm:ss'
Add-Summary "Test log collection - $($Selected.Name)"
Add-Summary "Collected           $Stamp"
Add-Summary "Gateway             $BaseUrl"
Add-Summary "Blueprint           $($Selected.Blueprint)"
Add-Summary "Deployment sought   $WantedDeployment"
Add-Summary "Deployment          $($Dep.name)"
Add-Summary "Room                $RoomId"
Add-Summary "Namespace           $($statusObj.namespace)"
Add-Summary "Log tail            $Tail lines/node"
Add-Summary ""
Add-Summary "BLUEPRINT"
Add-SummaryCheck $true 'deployment' "status RUNNING"

# ----------------------------------------------------------------- node health

Write-Step "Node phases"

$nodes = @()
try { $nodes = @(Invoke-Api "/deployments/$RoomId/nodes") } catch {
    Fail "Could not list the nodes: $($_.Exception.Message)" "The Room is RUNNING but its node list is unreadable; retry, then report it."
}
if ($nodes.Count -eq 0) { Fail "The Room reports no nodes at all." "Re-check the deployment in the Deployment Viewer." }

$AllRunning = $true
foreach ($n in $nodes) {
    $ok = ("$($n.phase)" -eq 'Running')
    if (-not $ok) { $AllRunning = $false }
    $detail = "$($n.phase)   [$($n.nodeType)]"
    if ($n.message) { $detail += "  $($n.message)" }
    Write-Check $ok "$($n.displayName)" $detail
    Add-SummaryCheck $ok "$($n.displayName)" $detail
}

# app-crash.txt, app-logcat.txt and the two guest probes come off a Skycraft VM
# over ADB. A Room without one has no guest to ask, so the whole ADB half is not
# attempted and its evidence rows read [-] rather than [ ]: absent, not failed.
$SkycraftNodes = @($nodes | Where-Object { "$($_.nodeType)" -eq 'skycraft' })
$HasSkycraft   = ($SkycraftNodes.Count -gt 0)

if ($AllRunning) {
    Write-Ok "All $($nodes.Count) nodes Running - the blueprint is healthy"
} else {
    # Not fatal: a half-up Room is exactly the case whose logs explain why.
    Write-Warn "Not every node is Running. Collecting anyway - the logs are what explain it."
}
Add-SummaryCheck $AllRunning 'all nodes Running' "$(@($nodes | Where-Object { "$($_.phase)" -eq 'Running' }).Count)/$($nodes.Count) Running"

# ------------------------------------------------------------------ run folder

Write-Step "Preparing the run folder"

if (-not (Test-Path -LiteralPath $OutRoot)) { New-Item -ItemType Directory -Path $OutRoot -Force | Out-Null }
$OutDir = Join-Path $OutRoot $Selected.Name
if (-not (Test-Path -LiteralPath $OutDir)) { New-Item -ItemType Directory -Path $OutDir -Force | Out-Null }
$OutDirFull = (Resolve-Path -LiteralPath $OutDir).Path

Write-Ok "$OutDirFull"
Write-Info "Relative to where you ran this. Existing files of the same name are overwritten."
Add-Summary ""
Add-Summary "OUTPUT              $OutDirFull"

# ---------------------------------------------------------------- node logs

Write-Step "Collecting node logs over REST"

# displayName -> file slug. "MOCKED ADA ECU" -> mocked-ada-ecu. The node-<slug>.txt
# prefix is the contract tools\pcap-extract\Extract-Pcap.ps1 globs on.
function Get-Slug ($name) {
    $s = "$name".ToLowerInvariant()
    $s = [regex]::Replace($s, '[^a-z0-9]+', '-')
    return $s.Trim('-')
}

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
function Get-NodeKeySuffix ($nodeName) {
    $parts = "$nodeName".Split('-')
    $tail  = $parts[$parts.Count - 1]
    if (-not $tail) { $tail = "$nodeName" }
    return Get-Slug $tail
}

$SlugOf   = @{}
$baseSlug = @{}
foreach ($n in $nodes) {
    $s = Get-Slug $n.displayName
    if (-not $s) { $s = Get-Slug $n.name }
    $baseSlug[$n.name] = $s
}
$dupes = @($baseSlug.Values | Group-Object | Where-Object { $_.Count -gt 1 } | ForEach-Object { $_.Name })
foreach ($n in $nodes) {
    $s = $baseSlug[$n.name]
    if ($dupes -contains $s) { $s = "$s-$(Get-NodeKeySuffix $n.name)" }
    $SlugOf[$n.name] = $s
}

Add-Summary ""
Add-Summary "NODE LOGS"

$NodeFiles = New-Object System.Collections.ArrayList
foreach ($n in $nodes) {
    $slug    = $SlugOf[$n.name]
    $isVm    = ("$($n.nodeType)" -eq 'skycraft')

    # A Skycraft node's log is the VM host's own WebRTC/GPU output, never the app's.
    # It is collected for completeness under a name the pcap extractor does not glob
    # and the [TX] scan does not read, so it can never be mistaken for app evidence.
    if ($isVm) { $file = Join-Path $OutDirFull "skycraft-$slug-vmhost.txt" }
    else       { $file = Join-Path $OutDirFull "node-$slug.txt" }

    # Every node is asked for a log, whatever its type -- the node list is the only
    # thing that decides what gets collected, so a Room of node types this script has
    # never seen still collects. container=user selects the application stream over
    # the platform's sidecar and is what a container answers to; the Skycraft VM has
    # no such container and the Ethernet Bridge 502s it. So both forms are tried, in
    # the order the node type makes likely, and only a node that refuses both is
    # reported as having no log stream.
    $variants = @("?tail=$Tail", "?container=user&tail=$Tail")
    if ("$($n.nodeType)" -eq 'container') { $variants = @("?container=user&tail=$Tail", "?tail=$Tail") }

    $lines   = $null
    $got     = $false
    $lastErr = ''
    foreach ($q in $variants) {
        try {
            $resp  = Invoke-Api "/deployments/$RoomId/logs/$($n.name)$q"
            $lines = $resp.lines
            $got   = $true
            break
        } catch {
            $lastErr = $_.Exception.Message
        }
    }

    # No stream is not a failure. The Ethernet Bridge has no log to give and never
    # will, and a Room is not less healthy for containing one.
    if (-not $got) {
        Write-Skip "$($n.displayName)" "no log stream [$($n.nodeType)] - $lastErr"
        Add-SummarySkip "$($n.displayName)" "no log stream [$($n.nodeType)]"
        continue
    }

    if (-not $lines) { $lines = @() }
    $lines = @($lines)
    ($lines -join "`r`n") | Out-File -FilePath $file -Encoding utf8

    $label = "$(Split-Path -Leaf $file)   $($lines.Count) lines"
    if ($isVm) { $label += "   (VM host output, not app evidence)" }
    Write-Check ($lines.Count -gt 0) "$($n.displayName)" $label
    Add-SummaryCheck ($lines.Count -gt 0) "$($n.displayName)" $label

    if (-not $isVm) { [void]$NodeFiles.Add($file) }
}

# ---------------------------------------------------------------------- adb

if (-not $HasSkycraft) {
    Write-Step "Guest evidence"
    Write-Skip 'Skycraft node' "none in this Room - no guest to collect from"
    Write-Info "app-logcat.txt, app-crash.txt and the guest probes need an AAOS VM node."
    Add-Summary ""
    Add-Summary "GUEST               n/a - no Skycraft node in this Room"
} else {

Write-Step "Looking for the ADB tunnel on localhost:$Port"

function Test-Port ($p) {
    $c = New-Object System.Net.Sockets.TcpClient
    try   { $c.Connect('127.0.0.1', $p); return $true }
    catch { return $false }
    finally { $c.Dispose() }
}

# adb: PATH first, then every location an Android SDK is commonly installed to.
# Hardcoding one path breaks on any machine that put the SDK elsewhere.
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

$Package    = 'com.hackathon.v2x.ivi'
$serial     = "localhost:$Port"
$GuestReady = $false

if (-not $Adb) {
    Write-Warn "adb.exe not found on PATH or in any standard Android SDK location."
    Write-Info "Skipping the guest half. Install Android SDK platform-tools, or set ANDROID_HOME."
    Add-Summary ""
    Add-Summary "GUEST               skipped - adb not found"
} elseif (-not (Test-Port $Port)) {
    Write-Warn "Nothing is listening on 127.0.0.1:$Port - the ADB tunnel is down."
    Write-Info "The node logs above are collected; the guest-side files are skipped, not failed."
    Write-Info "Open the tunnel:  .\tools\apk-uploader\install-ivi-apk.ps1 -SkipInstall -KeepTunnel"
    Add-Summary ""
    Add-Summary "GUEST               skipped - no ADB tunnel on localhost:$Port"
} else {
    Write-Ok "adb  ->  $Adb"
    Write-Ok "port $Port is serving"

    & $Adb connect $serial 2>&1 | Out-Null
    $connected = $false
    $deadline  = (Get-Date).AddSeconds(20)
    while ((Get-Date) -lt $deadline) {
        $devices = & $Adb devices 2>&1 | Out-String
        if ($devices -match [regex]::Escape($serial) + '\s+device\b') { $connected = $true; break }
        Start-Sleep -Seconds 1
        & $Adb connect $serial 2>&1 | Out-Null
    }

    if (-not $connected) {
        Write-Warn "The tunnel is serving but $serial never reached state 'device'."
        Write-Info "Skipping the guest half. The Skycraft node may still be booting."
        Add-Summary ""
        Add-Summary "GUEST               skipped - $serial not in state 'device'"
    } else {
        $GuestReady = $true
        Write-Ok "$serial  device"
        Write-Info "Guest node: $($SkycraftNodes[0].displayName). Every adb call below is pinned with -s $serial."
    }
}

}  # end: this Room has a Skycraft node

# ------------------------------------------------------------- guest health

if ($GuestReady) {
    Write-Step "Probing the app on the guest"

    # A node showing Running says the VM is up, never that the app is: a Room can run
    # green for hours with no APK installed at all. Only the guest can answer these.
    $st = @{}
    $st.Version = ''
    $d = & $Adb -s $serial shell "dumpsys package $Package | grep versionName" 2>&1 | Out-String
    if ($d -match 'versionName=(\S+)') { $st.Version = $Matches[1] }
    $st.AppPid = (& $Adb -s $serial shell pidof $Package 2>&1 | Out-String).Trim()
    $a = & $Adb -s $serial shell "dumpsys activity activities | grep -i ResumedActivity" 2>&1 | Out-String
    $st.Resumed = ($a -match [regex]::Escape("$Package/.MainActivity"))
    $w = & $Adb -s $serial shell "dumpsys window | grep -i mCurrentFocus" 2>&1 | Out-String
    $st.Focused = ($w -match [regex]::Escape($Package))
    $p = & $Adb -s $serial shell "dumpsys power | grep -i mWakefulness=" 2>&1 | Out-String
    $st.Awake = ($p -match 'mWakefulness=Awake')

    $vDetail = "$Package NOT FOUND"
    if ($st.Version) { $vDetail = "$Package $($st.Version)" }
    $pDetail = 'no process'
    if ($st.AppPid) { $pDetail = "pid $($st.AppPid)" }
    $rDetail = 'not the resumed activity'
    if ($st.Resumed) { $rDetail = 'topResumedActivity' }
    $fDetail = 'another window has focus'
    if ($st.Focused) { $fDetail = 'holds input focus' }
    $aDetail = 'asleep - the HMI will not render'
    if ($st.Awake) { $aDetail = 'mWakefulness=Awake' }

    Add-Summary ""
    Add-Summary "GUEST"
    Write-Check ([bool]$st.Version) 'package installed' $vDetail; Add-SummaryCheck ([bool]$st.Version) 'package installed' $vDetail
    Write-Check ([bool]$st.AppPid)  'process running'   $pDetail; Add-SummaryCheck ([bool]$st.AppPid)  'process running'   $pDetail
    Write-Check $st.Resumed         'MainActivity up'   $rDetail; Add-SummaryCheck $st.Resumed         'MainActivity up'   $rDetail
    Write-Check $st.Focused         'window focused'    $fDetail; Add-SummaryCheck $st.Focused         'window focused'    $fDetail
    Write-Check $st.Awake           'screen awake'      $aDetail; Add-SummaryCheck $st.Awake           'screen awake'      $aDetail

    # --------------------------------------------------------- guest evidence

    Write-Step "Collecting guest evidence over ADB"

    $LogcatFile = Join-Path $OutDirFull 'app-logcat.txt'
    $CrashFile  = Join-Path $OutDirFull 'app-crash.txt'
    $Udp6File   = Join-Path $OutDirFull 'guest-udp6.txt'
    $IfaceFile  = Join-Path $OutDirFull 'guest-ifaces.txt'

    # -d dumps the ring buffer rather than streaming: the app started before we
    # attached, so a live stream would show nothing that already happened.
    & $Adb -s $serial logcat -d -v threadtime -s IVI_V2X R4ListenerService R4Deserializer 2>&1 |
        Out-File -FilePath $LogcatFile -Encoding utf8
    Write-Ok "app-logcat.txt    $((Get-Content -LiteralPath $LogcatFile | Measure-Object -Line).Lines) lines"

    # -b crash reads Android's separate crash buffer rather than the main one.
    & $Adb -s $serial logcat -d -b crash 2>&1 | Out-File -FilePath $CrashFile -Encoding utf8
    Write-Ok "app-crash.txt     $((Get-Content -LiteralPath $CrashFile | Measure-Object -Line).Lines) lines"

    # The socket is bound dual-stack, so it appears in udp6 and not in udp.
    & $Adb -s $serial shell cat /proc/net/udp6 2>&1 | Out-File -FilePath $Udp6File -Encoding utf8
    Write-Ok "guest-udp6.txt    listener on B8C4 = port 47300"

    & $Adb -s $serial shell ip -4 addr show 2>&1 | Out-File -FilePath $IfaceFile -Encoding utf8
    Write-Ok "guest-ifaces.txt  eth0 must carry the node's pin address"
}

# ---------------------------------------------------------------------- verdict

Write-Step "Reading the evidence"

function Get-FileText ($path) {
    if (-not (Test-Path -LiteralPath $path)) { return '' }
    $t = Get-Content -LiteralPath $path -Raw
    if (-not $t) { return '' }
    return $t
}

$LogcatText = Get-FileText (Join-Path $OutDirFull 'app-logcat.txt')
$CrashText  = Get-FileText (Join-Path $OutDirFull 'app-crash.txt')

$TxSeen = $false
$TxFile = ''
foreach ($f in $NodeFiles) {
    if ((Get-FileText $f) -match '\[TX\]') { $TxSeen = $true; $TxFile = Split-Path -Leaf $f; break }
}

$GuestCollected = ($LogcatText -ne '' -or $GuestReady)
$NoGuestReason  = 'not collected - no ADB tunnel'
if (-not $HasSkycraft) { $NoGuestReason = 'no Skycraft node in this Room' }

$txDetail = 'no [TX] in any node log'
if ($TxSeen) { $txDetail = "in $TxFile" }
$rxDetail = $NoGuestReason
if ($GuestCollected) {
    $rxDetail = 'no [RX] in app-logcat.txt'
    if ($LogcatText -match '\[RX\]') { $rxDetail = "$(([regex]::Matches($LogcatText, '\[RX\]')).Count) lines" }
}
$srcDetail = $NoGuestReason
if ($GuestCollected) {
    $srcDetail = 'absent from app-logcat.txt'
    if ($LogcatText -match 'source=v2x_relayed') { $srcDetail = 'every warning carries it' }
}
$riskDetail = $NoGuestReason
if ($GuestCollected) {
    $riskDetail = 'scenario never reached high'
    if ($LogcatText -match 'riskState=high') { $riskDetail = 'reached' }
}
# Scoped to the package on purpose: a bare FATAL search matches the stock AAOS
# Bluetooth stack aborting at boot, which is guest noise and not our defect.
$crashDetail = $NoGuestReason
$crashOk     = $false
if ($GuestCollected) {
    $crashOk = -not ($CrashText -match [regex]::Escape($Package))
    $crashDetail = "crash buffer names $Package"
    if ($crashOk) { $crashDetail = "nothing naming $Package" }
}

# Applies=false is the [-] state. The four guest rows are R4/app questions that a
# Room with no Skycraft node cannot be asked, so they are excluded from the score
# instead of counting against it -- a phase 0 or V2X-only Room can now pass whole.
$Evidence = @(
    [pscustomobject]@{ Name = 'deployment RUNNING';   Applies = $true;         Pass = $DepRunning;  Detail = $DepStatus },
    [pscustomobject]@{ Name = 'all nodes Running';    Applies = $true;         Pass = $AllRunning;  Detail = "$(@($nodes | Where-Object { "$($_.phase)" -eq 'Running' }).Count)/$($nodes.Count)" },
    [pscustomobject]@{ Name = '[TX] in a node log';   Applies = $true;         Pass = $TxSeen;      Detail = $txDetail },
    [pscustomobject]@{ Name = '[RX] in app-logcat';   Applies = $HasSkycraft;  Pass = ($GuestCollected -and $LogcatText -match '\[RX\]');           Detail = $rxDetail },
    [pscustomobject]@{ Name = 'source=v2x_relayed';   Applies = $HasSkycraft;  Pass = ($GuestCollected -and $LogcatText -match 'source=v2x_relayed'); Detail = $srcDetail },
    [pscustomobject]@{ Name = 'riskState=high';       Applies = $HasSkycraft;  Pass = ($GuestCollected -and $LogcatText -match 'riskState=high');     Detail = $riskDetail },
    [pscustomobject]@{ Name = "no crash naming app";  Applies = $HasSkycraft;  Pass = $crashOk;     Detail = $crashDetail }
)

Write-Host ""
Write-Host "  ------------------------------------------------------" -ForegroundColor White
Write-Host "  EVIDENCE - $($Selected.Name)" -ForegroundColor White
Write-Host "  ------------------------------------------------------" -ForegroundColor White
Add-Summary ""
Add-Summary "EVIDENCE"
foreach ($e in $Evidence) {
    if (-not $e.Applies) {
        Write-Host ("  [-] " + $e.Name.PadRight(24) + " $($e.Detail)") -ForegroundColor DarkGray
        Add-Summary ("[-] " + $e.Name.PadRight(24) + " $($e.Detail)")
        continue
    }
    $box = if ($e.Pass) { '[x]' } else { '[ ]' }
    $col = if ($e.Pass) { 'Green' } else { 'Yellow' }
    Write-Host ("  $box " + $e.Name.PadRight(24) + " $($e.Detail)") -ForegroundColor $col
    Add-Summary ("$box " + $e.Name.PadRight(24) + " $($e.Detail)")
}

$Applicable = @($Evidence | Where-Object { $_.Applies })
$Passed     = @($Applicable | Where-Object { $_.Pass }).Count

Write-Host ""
if ($Passed -eq $Applicable.Count -and $HasSkycraft) {
    Write-Host "  Complete. The app is receiving R4 and warning from relayed data." -ForegroundColor Green
} elseif ($Passed -eq $Applicable.Count) {
    Write-Host "  Complete for a Room with no guest. Every node-side check passed;" -ForegroundColor Green
    Write-Host "  the app rows are absent because this blueprint has no Skycraft node." -ForegroundColor Green
} elseif (-not $HasSkycraft) {
    Write-Host "  Node-side only, and that is all this Room has. Read the node logs above" -ForegroundColor Yellow
    Write-Host "  for what the [ ] rows mean." -ForegroundColor Yellow
} elseif (-not $GuestCollected) {
    Write-Host "  Node-side only. Without the ADB tunnel the app half cannot be evidenced -" -ForegroundColor Yellow
    Write-Host "  [RX], provenance and risk are unknown rather than failed." -ForegroundColor Yellow
} elseif ($LogcatText -match '\[RX\]') {
    Write-Host "  Partly evidenced. The app is alive and parsing, but not every line appeared." -ForegroundColor Yellow
    Write-Host "  A missing line can just mean the scenario had not reached that step yet." -ForegroundColor Yellow
} else {
    Write-Host "  No R4 reached the app. Check the producer's [TX] target against the IVI pin" -ForegroundColor Yellow
    Write-Host "  address, and guest-ifaces.txt for eth0 - apk-deploy.md, Troubleshooting." -ForegroundColor Yellow
}

# ---------------------------------------------------------------------- summary

Add-Summary ""
Add-Summary "FILES"
foreach ($f in (Get-ChildItem -LiteralPath $OutDirFull -File | Sort-Object Name)) {
    if ($f.Name -eq 'summary.txt') { continue }
    Add-Summary ("  " + $f.Name.PadRight(32) + " $([math]::Round($f.Length/1KB,1)) KB")
}
Add-Summary ""
Add-Summary "No screen recording is collected here - test guide Step 3-1 is still yours."

$SummaryFile = Join-Path $OutDirFull 'summary.txt'
($script:Summary -join "`r`n") | Out-File -FilePath $SummaryFile -Encoding utf8

Write-Host ""
Write-Host "  Written to $OutDirFull" -ForegroundColor Cyan
Write-Host "  Checklist:  $SummaryFile" -ForegroundColor Cyan
if ($NodeFiles.Count -gt 0) {
    Write-Host "  Captures:   .\tools\pcap-extract\Extract-Pcap.ps1 `"$OutDirFull`"" -ForegroundColor DarkGray
}
Write-Host ""
Write-Host "  STILL YOURS - no script can do this:" -ForegroundColor White
Write-Host "  The Screen widget - EGO and B solid, C dashed with a pulsing glow and the" -ForegroundColor Gray
Write-Host "  badge [V2X] C - <d> m - RISK: HIGH. This build logs nothing from its UI" -ForegroundColor Gray
Write-Host "  layer, so the switch to the Warning View is confirmable only on screen." -ForegroundColor Gray
Write-Host ""

exit 0
