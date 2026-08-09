<#
.SYNOPSIS
  Saved View Log in, .pcap files out -- the PowerShell port of extract_pcap.sh.

.DESCRIPTION
  A capturing node emits its rotated pcap to stdout as base64 between markers,
  because the log is the node's only egress:

      [PCAP-BEGIN <basename>]
      <base64, one or more lines>
      [PCAP-END]

  A saved log interleaves those blocks with unrelated [CAP], [EVT] and [BOOT]
  lines; everything outside a block is ignored.

  The reference implementations are V2X_ECU/tools/extract_pcap.sh and
  ADA_ECU/tools/extract_pcap.sh, which this matches behaviour-for-behaviour.
  They need Git Bash on Windows; this one needs nothing but PowerShell. The
  block format is frozen by the producers, V2X_ECU/capture.sh and
  ADA_ECU/capture.sh -- change it there, then in all three readers.

  Deliberate behaviours, none silent:
    - Output lands NEXT TO THE INPUT LOG unless -OutDir is given.
    - <basename> is untrusted: any path component is stripped so a crafted
      [PCAP-BEGIN ../../evil.pcap] writes evil.pcap inside the output directory
      and cannot escape it. '.', '..' and empty degrade to block-<n>.pcap, and
      a name not ending in .pcap gets the suffix.
    - No silent clobber: an existing target gets -2, -3, ... before .pcap, and
      after -99 the block fails rather than overwriting.
    - CRLF tolerant, because a log saved from a browser on Windows carries \r.
    - Both stdout consumers share one stream, so a [CAP]/[EVT]/[BOOT] line can
      land inside a base64 block. Those are dropped with a warning rather than
      poisoning the decode -- unambiguous, since base64 has neither [ nor space.
    - A truncated final block is reported and NOT written: half a file must not
      masquerade as a complete capture.
    - One failing block never hides the healthy ones.

.PARAMETER LogFile
  The log saved from a node's View Log, or written by the REST collection in
  phase5-ivi-deploy.md Step 5.

.PARAMETER OutDir
  Write the .pcap files here instead of beside the input log. Must exist.

.EXAMPLE
  .\tools\pcap-extract\Extract-Pcap.ps1 tools\apk-uploader\test-report\system\node-v2x.txt

.NOTES
  Exit status: 0 every block extracted | 1 no block found | 2 usage error |
  3 at least one block failed.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true, Position = 0)]
    [string] $LogFile,
    [string] $OutDir
)

$ErrorActionPreference = 'Stop'

if (-not (Test-Path -LiteralPath $LogFile -PathType Leaf)) {
    Write-Error "input log not found: $LogFile"
    exit 2
}
$LogFile = (Resolve-Path -LiteralPath $LogFile).Path

if ($OutDir) {
    if (-not (Test-Path -LiteralPath $OutDir -PathType Container)) {
        Write-Error "output directory does not exist: $OutDir"
        exit 2
    }
    $OutDir = (Resolve-Path -LiteralPath $OutDir).Path
} else {
    $OutDir = Split-Path -Parent $LogFile
}

# Reduce an untrusted block name to a file name inside $OutDir.
function Resolve-BlockName ($raw, $index) {
    $name = $raw
    if ($name) { $name = $name -replace '^.*[\\/]', '' }        # drop any path
    if ([string]::IsNullOrWhiteSpace($name) -or $name -eq '.' -or $name -eq '..') {
        $name = "block-$index"
    }
    if ($name -notmatch '\.pcap$') { $name = "$name.pcap" }
    return $name
}

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

$lines    = [System.IO.File]::ReadAllLines($LogFile)
$found    = 0
$written  = 0
$failed   = 0
$inBlock  = $false
$rawName  = ''
$buffer   = New-Object System.Collections.Generic.List[string]
$dropped  = 0

function Complete-Block {
    param($truncated)

    $script:found++
    if ($truncated) {
        Write-Warning "block $script:found ($script:rawName) has no [PCAP-END] - truncated, not written"
        $script:failed++
        return
    }

    $name = Resolve-BlockName $script:rawName $script:found
    $path = Get-FreePath $OutDir $name
    if (-not $path) {
        Write-Warning "block $script:found ($name): 99 files of that name already exist - not written"
        $script:failed++
        return
    }

    try {
        $bytes = [Convert]::FromBase64String(($script:buffer -join ''))
    } catch {
        Write-Warning "block $script:found ($name): base64 decode failed - $($_.Exception.Message)"
        $script:failed++
        return
    }

    [System.IO.File]::WriteAllBytes($path, $bytes)
    $script:written++
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
        Complete-Block $false
        $inBlock = $false
        continue
    }

    # A second [PCAP-BEGIN before the END means the first block was truncated.
    if ($line -match '^\s*\[PCAP-BEGIN\s*(.*?)\s*\]\s*$') {
        Complete-Block $true
        $rawName = $Matches[1]
        $buffer.Clear()
        continue
    }

    # Interleaved log line inside the block: base64 has neither '[' nor spaces.
    if ($line -match '[\[\]\s]') {
        if ($line.Trim()) { $dropped++ }
        continue
    }

    $buffer.Add($line)
}

if ($inBlock) { Complete-Block $true }

if ($dropped) { Write-Warning "$dropped interleaved log line(s) dropped from inside blocks" }

if ($found -eq 0) {
    Write-Warning "no [PCAP-BEGIN ...] block in $LogFile"
    Write-Host "The node needs `"capabilities`": [`"NET_RAW`"] in its config to capture at all."
    exit 1
}

Write-Host ""
Write-Host "$written of $found block(s) extracted to $OutDir"
if ($failed) { exit 3 }
exit 0
