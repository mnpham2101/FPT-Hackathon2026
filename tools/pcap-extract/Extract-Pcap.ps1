<#
.SYNOPSIS
  Saved node logs in, .pcap files out -- the PowerShell port of extract_pcap.sh.

.DESCRIPTION
  A capturing node emits its rotated pcap to stdout as base64 between markers,
  because the log is the node's only egress:

      [PCAP-BEGIN <basename>]
      <base64, one or more lines>
      [PCAP-END]

  A saved log interleaves those blocks with unrelated [CAP], [EVT] and [BOOT]
  lines; everything outside a block is ignored.

  The reference implementations are V2X_ECU/tools/extract_pcap.sh and
  ADA_ECU/tools/extract_pcap.sh. They need Git Bash on Windows; this one needs
  nothing but PowerShell. The block format is frozen by the producers,
  V2X_ECU/capture.sh and ADA_ECU/capture.sh -- change it there, then in every
  reader.

  Deliberate behaviours, none silent:
    - Output lands NEXT TO THE INPUT LOG unless -OutDir is given.
    - The .pcap is named after the INPUT LOG, not the embedded marker name:
      node-v2x-ecu.txt -> node-v2x-ecu.pcap. The <basename> in [PCAP-BEGIN] is
      cosmetic only and never reaches the filesystem.
    - No silent clobber: an existing target gets -2, -3, ... before .pcap, and
      after -99 the block fails rather than overwriting -- this is also how a
      second block from the same log gets its own file.
    - CRLF tolerant, because a log saved from a browser on Windows carries \r.
    - Both stdout consumers share one stream, so a [CAP]/[EVT]/[BOOT] line can
      land inside a base64 block. Those are dropped with a warning rather than
      poisoning the decode -- unambiguous, since base64 has neither [ nor space.
    - A truncated final block is reported and NOT written: half a file must not
      masquerade as a complete capture.
    - One failing block never hides the healthy ones.

.PARAMETER Path
  Either one saved log, or a directory of them. Given a directory, every
  'node-*.txt' in it is processed and the .pcap files land in that directory --
  which is the layout the log collector writes, ./test-report/<test-name>/.

.PARAMETER OutDir
  Write the .pcap files here instead of beside the input. Must exist.

.EXAMPLE
  .\tools\pcap-extract\Extract-Pcap.ps1 .\test-report\system-test
  Extracts from every node log of that run.

.EXAMPLE
  .\tools\pcap-extract\Extract-Pcap.ps1 .\test-report\system-test\node-v2x-ecu.txt
  Extracts from one log.

.NOTES
  Exit status: 0 every block extracted | 1 no block found | 2 usage error |
  3 at least one block failed.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true, Position = 0)]
    [Alias('LogFile')]
    [string] $Path,
    [string] $OutDir
)

$ErrorActionPreference = 'Stop'

# Usage errors go to stderr and set the exit code. Write-Error would throw under
# the Stop preference above, so `exit 2` would never run and the caller would
# see a terminating error instead of the documented status.
function Exit-Usage ($message) {
    [Console]::Error.WriteLine("extract-pcap: $message")
    exit 2
}

# ------------------------------------------------------------------ resolve in

if (-not (Test-Path -LiteralPath $Path)) {
    Exit-Usage "input not found: $Path"
}
$resolved = (Resolve-Path -LiteralPath $Path).Path
$isDir    = Test-Path -LiteralPath $resolved -PathType Container

if ($isDir) {
    # The collector names every container node's log node-<slug>.txt; that
    # prefix is the contract between the two tools.
    $inputs = @(Get-ChildItem -LiteralPath $resolved -Filter 'node-*.txt' -File | Sort-Object Name)
    if ($inputs.Count -eq 0) {
        Exit-Usage "no node-*.txt in $resolved - is this a test-report run folder?"
    }
    $defaultOut = $resolved
} else {
    $inputs = @(Get-Item -LiteralPath $resolved)
    $defaultOut = Split-Path -Parent $resolved
}

if ($OutDir) {
    if (-not (Test-Path -LiteralPath $OutDir -PathType Container)) {
        Exit-Usage "output directory does not exist: $OutDir"
    }
    $OutDir = (Resolve-Path -LiteralPath $OutDir).Path
} else {
    $OutDir = $defaultOut
}

# --------------------------------------------------------------------- helpers

# Never overwrite: insert -2, -3, ... before the suffix, give up after -99.
function Get-FreePath ($dir, $name) {
    $target = Join-Path $dir $name
    if (-not (Test-Path -LiteralPath $target)) { return $target }
    $stem = $name -replace '\.pcap$', ''
    for ($i = 2; $i -le 99; $i++) {
        $try = Join-Path $dir "$stem-$i.pcap"
        if (-not (Test-Path -LiteralPath $try)) { return $try }
    }
    return $null
}

function Invoke-ExtractOne {
    param($File, $Destination)

    $lines    = [System.IO.File]::ReadAllLines($File.FullName)
    $state    = @{ Found = 0; Written = 0; Failed = 0; Dropped = 0 }
    $inBlock  = $false
    $rawName  = ''
    $buffer   = New-Object System.Collections.Generic.List[string]
    $baseName = "$($File.BaseName).pcap"   # node-v2x-ecu.txt -> node-v2x-ecu.pcap

    $close = {
        param($truncated)

        $state.Found++
        if ($truncated) {
            Write-Warning "$($File.Name) block $($state.Found) ($rawName): no [PCAP-END] - truncated, not written"
            $state.Failed++
            return
        }

        $path = Get-FreePath $Destination $baseName
        if (-not $path) {
            Write-Warning "$($File.Name) block $($state.Found) ($baseName): 99 files of that name exist - not written"
            $state.Failed++
            return
        }

        # An empty body decodes to zero bytes without throwing. Writing that file
        # would let a 0-byte capture masquerade as a complete one -- the same
        # failure the truncated-block rule exists to prevent, and what the shell
        # tool beside this one does.
        $b64 = ($buffer -join '')
        if (-not $b64) {
            Write-Warning "$($File.Name) block $($state.Found) ($baseName): empty body, nothing to decode"
            $state.Failed++
            return
        }

        try {
            $bytes = [Convert]::FromBase64String($b64)
        } catch {
            Write-Warning "$($File.Name) block $($state.Found) ($baseName): base64 decode failed - $($_.Exception.Message)"
            $state.Failed++
            return
        }

        [System.IO.File]::WriteAllBytes($path, $bytes)
        $state.Written++
        Write-Host ("  wrote {0}  ({1:N0} bytes)" -f (Split-Path -Leaf $path), $bytes.Length)
    }

    foreach ($raw in $lines) {
        $line = $raw -replace "`r", ''

        if (-not $inBlock) {
            if ($line -match '^\s*\[PCAP-BEGIN\s*(.*?)\s*\]\s*$') {
                $inBlock = $true
                $rawName = $Matches[1]
                $buffer.Clear()
            }
            continue
        }

        if ($line -match '^\s*\[PCAP-END\]\s*$') {
            & $close $false
            $inBlock = $false
            continue
        }

        # A second [PCAP-BEGIN before the END means the first block was truncated.
        if ($line -match '^\s*\[PCAP-BEGIN\s*(.*?)\s*\]\s*$') {
            & $close $true
            $rawName = $Matches[1]
            $buffer.Clear()
            continue
        }

        # Interleaved log line inside the block: base64 has neither '[' nor spaces.
        if ($line -match '[\[\]\s]') {
            if ($line.Trim()) { $state.Dropped++ }
            continue
        }

        $buffer.Add($line)
    }

    if ($inBlock) { & $close $true }

    if ($state.Dropped) {
        Write-Warning "$($File.Name): $($state.Dropped) interleaved log line(s) dropped from inside blocks"
    }
    return $state
}

# ------------------------------------------------------------------------ main

$found = 0; $written = 0; $failed = 0

foreach ($file in $inputs) {
    if ($inputs.Count -gt 1) { Write-Host "$($file.Name):" }
    $r = Invoke-ExtractOne -File $file -Destination $OutDir
    $found   += $r.Found
    $written += $r.Written
    $failed  += $r.Failed
}

if ($found -eq 0) {
    Write-Warning "no [PCAP-BEGIN ...] block in $resolved"
    Write-Host "A node captures only with `"capabilities`": [`"NET_RAW`"] in its config."
    Write-Host "The isolated tests' stand-in nodes do not capture at all."
    exit 1
}

Write-Host ""
Write-Host "$written of $found block(s) extracted to $OutDir"
if ($failed) {
    Write-Warning "$failed block(s) failed - see the messages above"
    exit 3
}
exit 0
